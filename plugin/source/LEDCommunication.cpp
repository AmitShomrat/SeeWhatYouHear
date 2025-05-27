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
    stopThread(2000);
    if (hserial != INVALID_HANDLE_VALUE) {
        // Turn off LEDs before closing
        std::cout << "Turning off LEDs before closing." << std::endl;
        prepareData(RGB{0, 0, 0}, RGB{0, 0, 0});
        DWORD bytesWritten;
        WriteFile(hserial, ledData.data(), static_cast<DWORD>(ledData.size()), &bytesWritten, NULL);
        CloseHandle(hserial);
    }
}
void LEDCommunication::setBrightness(float leftBrightness, float rightBrightness, float userBrightness)
{ 
    float leftScaled = 0.0f;
    float rightScaled = 0.0f;
    // Add threshold check for zero values
    if (std::abs(leftBrightness) < 0.001f) {
        leftScaled = 0.0f;
    } else {
        // Use decibels scaling with expanded range (-60dB to 0dB)
        leftScaled = juce::Decibels::decibelsToGain(std::abs(leftBrightness) * 60.0f - 60.0f);
    }

    if (std::abs(rightBrightness) < 0.001f) {
        rightScaled = 0.0f;
    } else {
        rightScaled = juce::Decibels::decibelsToGain(std::abs(rightBrightness) * 60.0f - 60.0f);
    }

    // Apply power curve for better sensitivity at low levels
    leftScaled = std::pow(leftScaled, 0.4f);
    rightScaled = std::pow(rightScaled, 0.4f);

    // Normalize userBrightness to 0-1 range
    // float normalizedBrightness = userBrightness / 0.135f;
    
    float userBrightnessScale = 0.0f;
    if (userBrightness >= 0.019f) {  // Start from step 1
        // Rescale to ensure step 1 is visible
        userBrightnessScale = juce::jmap(userBrightness, 
                                   0.019f, 0.135f,  // Input range: from step 1 to max
                                   0.2f, 1.0f);     // Output range: start at 20% brightness
    }

    // Map to 0-255 range with threshold at 0.004
    float mappedValueLeft = juce::jmap(
        leftScaled,
        juce::Decibels::decibelsToGain(-60.0f),  // input range start
        juce::Decibels::decibelsToGain(0.0f),    // input range end
        0.0f,                                    // output range start
        255.0f                                   // output range end
    ) * userBrightnessScale;

    float mappedValueRight = juce::jmap(
        rightScaled,
        juce::Decibels::decibelsToGain(-60.0f),  // input range start
        juce::Decibels::decibelsToGain(0.0f),    // input range end
        0.0f,                                    // output range start
        255.0f                                   // output range end
    ) * userBrightnessScale;

    // Store the brightness values with threshold applied
    currentLeftBrightness.store(juce::jlimit(0, 255, static_cast<int>(std::round(mappedValueLeft))));
    currentRightBrightness.store(juce::jlimit(0, 255, static_cast<int>(std::round(mappedValueRight))));
    // std::cout << "currentLeftBrightness: " << currentLeftBrightness.load() << std::endl;
    // std::cout << "currentRightBrightness: " << currentRightBrightness.load() << std::endl;
}
void LEDCommunication::prepareData(RGB rgbLeftValues, RGB rgbRightValues)
{
    // Scale RGB values by brightness
    float leftScale = static_cast<float>(currentLeftBrightness.load()) / 255.0f;
    float rightScale = static_cast<float>(currentRightBrightness.load()) / 255.0f;

    juce::ignoreUnused(rgbLeftValues, rgbRightValues, leftScale, rightScale);
    RGB scaledRGBLeft{
        static_cast<uint8_t>(rgbLeftValues.r * (leftScale)),
        static_cast<uint8_t>(rgbLeftValues.g * (leftScale)),
        static_cast<uint8_t>(rgbLeftValues.b * (leftScale))
    };
    // std::cout << "scaledRGBLeft: " << static_cast<int> (scaledRGBLeft.r) << " " << static_cast<int> (scaledRGBLeft.g) << " " << static_cast<int> (scaledRGBLeft.b) << std::endl;
    // std::cout << "leftScale: " << leftScale << std::endl;
    RGB scaledRGBRight{
        static_cast<uint8_t>(rgbRightValues.r * (rightScale)),
        static_cast<uint8_t>(rgbRightValues.g * (rightScale)),
        static_cast<uint8_t>(rgbRightValues.b * (rightScale))
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
}

bool LEDCommunication::tryConnect()
{
    if (hserial != INVALID_HANDLE_VALUE) {
        CloseHandle(hserial);
        hserial = INVALID_HANDLE_VALUE;
    }

    // Convert JUCE String to ANSI string
    char ansiPortName[MAX_PATH];
    WideCharToMultiByte(CP_ACP, 0, portName.toWideCharPointer(), -1, ansiPortName, MAX_PATH, nullptr, nullptr);

    // Open the serial port
    hserial = CreateFileA(ansiPortName,
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

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

        wait(8);
    }
    
    std::cout << "=== LED Communication Thread Stopping ===" << std::endl;
}

} // namespace audio_plugin

