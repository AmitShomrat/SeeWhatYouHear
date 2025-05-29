#include "SeeWhatYouHear/LEDCommunication.h"
#include "SeeWhatYouHear/PluginProcessor.h"
#include <windows.h>
#include <iostream>

namespace audio_plugin {

LEDCommunication::LEDCommunication(AudioPluginAudioProcessor* processor) 
    : juce::Thread("LEDCommunicationThread"), portName("COM3"), processorPointer(processor),
      hserial(INVALID_HANDLE_VALUE)
{   
    ledData.resize(numLEDs * 3 + 1 + 2);
    startThread();
}

LEDCommunication::~LEDCommunication() 
{
    if (hserial != INVALID_HANDLE_VALUE) {
        // Turn off LEDs before closing
        std::cout << "Turning off LEDs before closing." << std::endl;
        prepareData(RGB{0, 0, 0}, RGB{0, 0, 0});
        DWORD bytesWritten;
        if (!WriteFile(hserial, ledData.data(), static_cast<DWORD>(ledData.size()), &bytesWritten, NULL)) {
            std::cout << "Failed to write to serial port. Error code: " << GetLastError() << std::endl;
        }
        CloseHandle(hserial);
        std::cout << "LEDCommunication destroyed." << std::endl;
    }
    stopThread(1000);
}

int LEDCommunication::setBrightness(float monoBrightness)
{ 
    float leftScaled = 0.0f;

    // Add threshold check for zero values
    if (std::abs(monoBrightness) < 0.001f) {
        leftScaled = 0.0f;
    } else {
        // Use decibels scaling with expanded range (-60dB to 0dB)
        leftScaled = monoBrightness;
    }
    // Apply power curve for better sensitivity at low levels
    // float memory = 0.9898f sensetive for low values start leds at -40Db due to the totalyzor
    // float memory2 = 3.0f sensetive for high values start leds at -10Db due to the totalyzor
    leftScaled = std::pow(leftScaled, 3.0f);

    // Map to 0-255 range with threshold at 0.004
    float mappedValueLeft = juce::jmap(
        leftScaled, 
        0.0f,
        1.0f,
        0.0f,
        255.0f
    );

    // Store the brightness values with threshold applied
    return juce::jlimit(0, 255, static_cast<int>(std::round(mappedValueLeft)));
}

void LEDCommunication::prepareData(RGB rgbLeftValues, RGB rgbRightValues)
{
    if (processorPointer) {
        currentLeftBrightness.store(setBrightness(processorPointer->getBrightnessDecision().getLeftBrightness().get()));
        currentRightBrightness.store(setBrightness(processorPointer->getBrightnessDecision().getRightBrightness().get()));
    }

    juce::ignoreUnused(rgbLeftValues, rgbRightValues);
    RGB scaledRGBLeft{
        static_cast<uint8_t>(rgbLeftValues.r * (currentLeftBrightness.load() / 255.0f)),
        static_cast<uint8_t>(rgbLeftValues.g * (currentLeftBrightness.load() / 255.0f)),
        static_cast<uint8_t>(rgbLeftValues.b * (currentLeftBrightness.load() / 255.0f))
    };
    // std::cout << "scaledRGBLeft: " << static_cast<int> (scaledRGBLeft.r) << " " << static_cast<int> (scaledRGBLeft.g) << " " << static_cast<int> (scaledRGBLeft.b) << std::endl;
    // std::cout << "leftScale: " << leftScale << std::endl;
    RGB scaledRGBRight{
        static_cast<uint8_t>(rgbRightValues.r * (currentRightBrightness.load() / 255.0f)),
        static_cast<uint8_t>(rgbRightValues.g * (currentRightBrightness.load() / 255.0f)),
        static_cast<uint8_t>(rgbRightValues.b * (currentRightBrightness.load() / 255.0f))
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

void LEDCommunication::run() {
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

        if (processorPointer) {
            prepareData(processorPointer->FFTProcessor->getLeftRGB(),
                        processorPointer->FFTProcessor->getRightRGB());
        
            DWORD bytesWritten;
                if (!WriteFile(hserial, ledData.data(), static_cast<DWORD>(ledData.size()), &bytesWritten, NULL)) {
                    DWORD error = GetLastError();
                    std::cout << "Failed to write to serial port. Error code: " << error << std::endl;
                    isPortConnected.store(false);
                    continue;
                }
        }
        wait(8);
    }
    std::cout << "=== LED Communication Thread Stopping ===" << std::endl;
}

} // namespace audio_plugin

