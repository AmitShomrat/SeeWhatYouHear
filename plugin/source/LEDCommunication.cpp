#include "SeeWhatYouHear/LEDCommunication.h"
#include <windows.h>
#include <iostream>

namespace audio_plugin {

LEDCommunication::LEDCommunication(const juce::String& portName, ColorDecisionML& leftColorDecisionML, ColorDecisionML& rightColorDecisionML) 
    : juce::Thread("LEDCommunicationThread"), portName(portName),
      hserial(INVALID_HANDLE_VALUE), leftColorDecisionML(leftColorDecisionML), rightColorDecisionML(rightColorDecisionML)
{   
    ledData.resize(numLEDs * 3 + 1 + 2);
    startThread();
}

LEDCommunication::~LEDCommunication() 
{
    stopThread(1000);
    if (hserial != INVALID_HANDLE_VALUE) {
        // Turn off LEDs before closing
        std::cout << "Turning off LEDs before closing." << std::endl;
        prepareData(RGB{0, 0, 0}, RGB{0, 0, 0});
        DWORD bytesWritten;
        WriteFile(hserial, ledData.data(), static_cast<DWORD>(ledData.size()), &bytesWritten, NULL);
        CloseHandle(hserial);
    }
}

void LEDCommunication::prepareData(RGB rgbLeftValues, RGB rgbRightValues)
{
    // Scale RGB values by brightness
    float leftScale = static_cast<float>(currentLeftBrightness.load()) / 255.0f;
    float rightScale = static_cast<float>(currentRightBrightness.load()) / 255.0f;
    
    RGB scaledRGBLeft{
        static_cast<int>(rgbLeftValues.r * (leftScale)),
        static_cast<int>(rgbLeftValues.g * (leftScale)),
        static_cast<int>(rgbLeftValues.b * (leftScale))
    };
    // std::cout << "scaledRGBLeft: " << static_cast<int> (scaledRGBLeft.r) << " " << static_cast<int> (scaledRGBLeft.g) << " " << static_cast<int> (scaledRGBLeft.b) << std::endl;
    // std::cout << "leftScale: " << leftScale << std::endl;
    RGB scaledRGBRight{
        static_cast<int>(rgbRightValues.r * (rightScale)),
        static_cast<int>(rgbRightValues.g * (rightScale)),
        static_cast<int>(rgbRightValues.b * (rightScale))
    };
    // std::cout << "scaledRGBRight: " << static_cast<int> (scaledRGBRight.r) << " " << static_cast<int> (scaledRGBRight.g) << " " << static_cast<int> (scaledRGBRight.b) << std::endl;
    // std::cout << "rightScale: " << rightScale << std::endl;
    ledData[0] = static_cast<unsigned char>(0xFF);
    ledData[1] = static_cast<unsigned char>(currentLeftBrightness.load());
    ledData[2] = static_cast<unsigned char>(currentRightBrightness.load());

    const int halfLEDs = numLEDs / 2;
    for(int i = 0; i < halfLEDs - 1; ++i) {
        ledData[i * 3 + 3] = static_cast<unsigned char>(scaledRGBLeft.r);
        ledData[i * 3 + 4] = static_cast<unsigned char>(scaledRGBLeft.g);
        ledData[i * 3 + 5] = static_cast<unsigned char>(scaledRGBLeft.b);

        ledData[(i + halfLEDs) * 3 + 3] = static_cast<unsigned char>(scaledRGBRight.r);
        ledData[(i + halfLEDs) * 3 + 4] = static_cast<unsigned char>(scaledRGBRight.g);
        ledData[(i + halfLEDs) * 3 + 5] = static_cast<unsigned char>(scaledRGBRight.b);
    }

    //Debugging:
    // ledData[3] = static_cast<unsigned char>(255);
    // ledData[4] = static_cast<unsigned char>(rgbLeftValues.g);
    // ledData[5] = static_cast<unsigned char>(rgbLeftValues.b);
    // std::cout << "ledData[3]: " << static_cast<int> (ledData[3]) << std::endl;
    // std::cout << "ledData[4]: " << static_cast<int> (ledData[4]) << std::endl;
    // std::cout << "ledData[5]: " << static_cast<int> (ledData[5]) << std::endl;

    // ledData[453] = static_cast<unsigned char>(255);
    // ledData[454] = static_cast<unsigned char>(rgbRightValues.g);
    // ledData[455] = static_cast<unsigned char>(rgbRightValues.b);
    // std::cout << "ledData[453]: " << static_cast<int> (ledData[453]) << std::endl;
    // std::cout << "ledData[454]: " << static_cast<int> (ledData[454]) << std::endl;
    // std::cout << "ledData[455]: " << static_cast<int> (ledData[455]) << std::endl;
}

bool LEDCommunication::tryConnect()
{
    if (hserial != INVALID_HANDLE_VALUE) {
        CloseHandle(hserial);
        hserial = INVALID_HANDLE_VALUE;
    }

    hserial = CreateFile(portName.toRawUTF8(), 
                        GENERIC_WRITE,
                        0, 
                        NULL, 
                        OPEN_EXISTING, 
                        0, 
                        NULL);

    if (hserial == INVALID_HANDLE_VALUE) {
        std::cout << "Failed to open serial port: " << portName << std::endl;
        isPortConnected.store(false);
        return false;
    }

    DCB dcb = {0};
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(hserial, &dcb)) {
        std::cout << "Failed to get comm state" << std::endl;
        CloseHandle(hserial);
        hserial = INVALID_HANDLE_VALUE;
        isPortConnected.store(false);
        return false;
    }

    dcb.BaudRate = CBR_115200;
    dcb.ByteSize = 8;
    dcb.StopBits = ONESTOPBIT;
    dcb.Parity = NOPARITY;

    if (!SetCommState(hserial, &dcb)) {
        std::cout << "Failed to set comm state" << std::endl;
        CloseHandle(hserial);
        hserial = INVALID_HANDLE_VALUE;
        isPortConnected.store(false);
        return false;
    }

    isPortConnected.store(true);
    return true;
}

void LEDCommunication::run()
{
    juce::int64 lastRetryTime = 0;
    std::cout << "=== LED Communication Thread Started ===" << std::endl;
    std::cout << "Initial port: " << portName << std::endl;

    while (!threadShouldExit()) {
        if (shouldReconnect.load() || (!isPortConnected.load() && juce::Time::currentTimeMillis() - lastRetryTime > RETRY_INTERVAL_MS)) {
            shouldReconnect.store(false);
            lastRetryTime = juce::Time::currentTimeMillis();
            
            std::cout << "Attempting to connect to port: " << portName << " at " << juce::Time::getCurrentTime().toString(true, true) << std::endl;
            if (!tryConnect()) {
                std::cout << "Connection attempt failed - will retry in " << RETRY_INTERVAL_MS/1000 << " seconds" << std::endl;
                wait(100);
                continue;
            }
            std::cout << "Successfully connected to port: " << portName << std::endl;
        }

        if (!isPortConnected.load()) {
            wait(100);
            continue;
        }

        RGB rgbLeft = leftColorDecisionML.getCurrentRGB();
        RGB rgbRight = rightColorDecisionML.getCurrentRGB();
        // std::cout << "rgbLeft: " << static_cast<int> (rgbLeft.r) << " " << static_cast<int> (rgbLeft.g) << " " << static_cast<int> (rgbLeft.b) << std::endl;
        // std::cout << "rgbRight: " << static_cast<int> (rgbRight.r) << " " << static_cast<int> (rgbRight.g) << " " << static_cast<int> (rgbRight.b) << std::endl;
        prepareData(rgbLeft, rgbRight);
        
        DWORD bytesWritten;
        if (!WriteFile(hserial, ledData.data(), static_cast<DWORD>(ledData.size()), &bytesWritten, NULL)) {
            DWORD error = GetLastError();
            std::cout << "Failed to write to serial port. Error code: " << error << std::endl;
            isPortConnected.store(false);
            continue;
        }

        wait(16);
    }
    
    std::cout << "=== LED Communication Thread Stopping ===" << std::endl;
}

} // namespace audio_plugin

