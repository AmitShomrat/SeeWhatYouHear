#include "SimpleEQ/LEDCommunication.h"
#include <windows.h>

namespace audio_plugin {

LEDCommunication::LEDCommunication(const juce::String& portName) 
    : juce::Thread("LEDCommunicationThread"), portName(portName),
      lastColor(Color::Red),
      lastLeftBrightness(-1),
      lastRightBrightness(-1),
      hserial(INVALID_HANDLE_VALUE)
{   
    ledData.resize(numLEDs * 3 + 1 + 2);
    startThread();
}

LEDCommunication::~LEDCommunication() 
{
    stopThread(1000);
    if (hserial != INVALID_HANDLE_VALUE) {
        // Turn off LEDs before closing
        prepareData(RGB{0, 0, 0}, RGB{0, 0, 0});
        DWORD bytesWritten;
        WriteFile(hserial, ledData.data(), static_cast<DWORD>(ledData.size()), &bytesWritten, NULL);
        CloseHandle(hserial);
    }
}

void LEDCommunication::prepareData(RGB rgbLeftValues, RGB rgbRightValues)
{
    ledData[0] = static_cast<unsigned char>(0xFF);
    ledData[1] = static_cast<unsigned char>(currentLeftBrightness.load());
    ledData[2] = static_cast<unsigned char>(currentRightBrightness.load());
    
    const int halfLEDs = numLEDs / 2;
    for(int i = 0; i < halfLEDs - 1; ++i)
    {
        ledData[i * 3 + 3] = static_cast<unsigned char>(rgbLeftValues.r);
        ledData[i * 3 + 4] = static_cast<unsigned char>(rgbLeftValues.g);
        ledData[i * 3 + 5] = static_cast<unsigned char>(rgbLeftValues.b);

        ledData[(i + halfLEDs) * 3 + 3] = static_cast<unsigned char>(rgbRightValues.r);
        ledData[(i + halfLEDs) * 3 + 4] = static_cast<unsigned char>(rgbRightValues.g);
        ledData[(i + halfLEDs) * 3 + 5] = static_cast<unsigned char>(rgbRightValues.b);
    }
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
        DBG("Failed to open serial port: " + portName);
        isPortConnected.store(false);
        return false;
    }

    DCB dcb = {0};
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(hserial, &dcb)) {
        DBG("Failed to get comm state");
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
        DBG("Failed to set comm state");
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
    DBG("=== LED Communication Thread Started ===");
    DBG("Initial port: " + portName);

    while (!threadShouldExit()) {
        // Check if we need to reconnect
        if (shouldReconnect.load() || (!isPortConnected.load() && juce::Time::currentTimeMillis() - lastRetryTime > RETRY_INTERVAL_MS)) {
            shouldReconnect.store(false);
            lastRetryTime = juce::Time::currentTimeMillis();
            
            DBG("Attempting to connect to port: " + portName + " at " + juce::Time::getCurrentTime().toString(true, true));
            if (!tryConnect()) {
                DBG("Connection attempt failed - will retry in " + juce::String(RETRY_INTERVAL_MS/1000) + " seconds");
                wait(100); // Short wait to avoid busy loop when disconnected
                continue;
            }
            DBG("Successfully connected to port: " + portName);
        }

        // If not connected, skip processing
        if (!isPortConnected.load()) {
            wait(100);
            continue;
        }

        Color color = currentColor.load();
        int leftBrightness = currentLeftBrightness.load();
        int rightBrightness = currentRightBrightness.load();
        
        // Only update if there's a change in color or brightness
        if (color != lastColor || 
            leftBrightness != lastLeftBrightness || 
            rightBrightness != lastRightBrightness) {
            
            DBG("Updating LEDs - Color: " + juce::String(static_cast<int>(color)) + 
                " Left: " + juce::String(leftBrightness) + 
                " Right: " + juce::String(rightBrightness));
            
            switch(color) {
                case Color::Red:
                    prepareData(RGB{255, 0, 0}, RGB{255, 0, 0});        
                    break;
                case Color::Blue:
                    prepareData(RGB{0, 0, 255}, RGB{0, 0, 255});        
                    break;
                case Color::Green:
                    prepareData(RGB{0, 255, 0}, RGB{0, 255, 0});        
                    break;
                case Color::Yellow:
                    prepareData(RGB{255, 255, 0}, RGB{255, 255, 0});        
                    break;
            }
            
            DWORD bytesWritten;
            if (!WriteFile(hserial, ledData.data(), static_cast<DWORD>(ledData.size()), &bytesWritten, NULL)) {
                DWORD error = GetLastError();
                DBG("Failed to write to serial port. Error code: " + juce::String(error));
                isPortConnected.store(false);
                continue;
            }
            
            DBG("Successfully wrote " + juce::String(bytesWritten) + " bytes to LED port");
            
            // Update last values only on successful write
            lastColor = color;
            lastLeftBrightness = leftBrightness;
            lastRightBrightness = rightBrightness;
        }
        
        wait(16);
    }
    
    DBG("=== LED Communication Thread Stopping ===");
}

} // namespace audio_plugin

