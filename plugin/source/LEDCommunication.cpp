#include "SeeWhatYouHear/LEDCommunication.h"
#include "SeeWhatYouHear/PluginProcessor.h"
#include <windows.h>
#include <iostream>

namespace audio_plugin {

LEDCommunication::LEDCommunication(AudioPluginAudioProcessor* processor) 
    : juce::Thread("LEDCommunicationThread"), portName("COM3"), processorPointer(processor),
      hserial(INVALID_HANDLE_VALUE)
{   
    // initializePearsonData(); Pearson Test.
    // ------------------------------------------------------------

    // 1 Mode byte + 2 * Brightness byte (R/L) + 2 * 3 Color byte (R/L).
    ledData.resize(9);
    startThread();
}

LEDCommunication::~LEDCommunication() 
{
    stopThread(1500);
    if (hserial != INVALID_HANDLE_VALUE) {
        // Turn off LEDs before closing
        std::cout << "Turning off LEDs before closing." << std::endl;
        ledData[0] = static_cast<unsigned char>(4);
        DWORD bytesWritten;
        WriteFile(hserial, ledData.data(), static_cast<DWORD>(ledData.size()), &bytesWritten, NULL);
        CloseHandle(hserial);
        std::cout << "LEDCommunication destroyed." << std::endl;
    }
}

int LEDCommunication::setBrightness(float monoBrightness)
{ 
    float monoChannelScaled = 0.0f;

    // Add threshold check for zero values
    if (std::abs(monoBrightness) < 0.001f) {
        monoChannelScaled = 0.0f;
    } else {
        // Use decibels scaling with expanded range (-60dB to 0dB)
        monoChannelScaled = monoBrightness;
    }
    // Apply power curve for better sensitivity at low levels
    // float memory = 0.9898f sensetive for low values start leds at -40Db due to the totalyzor
    // float memory2 = 3.0f sensetive for high values start leds at -10Db due to the totalyzor
    monoChannelScaled = std::pow(monoChannelScaled, 0.5f);

    // Map to 0-255 range with threshold at 0.004
    float mappedValueLeft = juce::jmap(
        monoChannelScaled, 
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
    
    currentMode.store(static_cast<int>(processorPointer->apvts.getRawParameterValue("Mode")->load()));
    ledData[0] = static_cast<unsigned char>(currentMode.load());
    // Why not sending only mode bit, color and brightness and the LED itself will duplicate the color for each led?
    ledData[1] = static_cast<unsigned char>(currentLeftBrightness.load());
    ledData[2] = static_cast<unsigned char>(currentRightBrightness.load());
    ledData[3] = static_cast<unsigned char>(scaledRGBLeft.r);
    ledData[4] = static_cast<unsigned char>(scaledRGBLeft.g);
    ledData[5] = static_cast<unsigned char>(scaledRGBLeft.b);
    ledData[6] = static_cast<unsigned char>(scaledRGBRight.r);
    ledData[7] = static_cast<unsigned char>(scaledRGBRight.g);
    ledData[8] = static_cast<unsigned char>(scaledRGBRight.b);
}

void LEDCommunication::sendData() {
    if (processorPointer) {
        prepareData(processorPointer->FFTProcessor->getLeftRGB(),
                    processorPointer->FFTProcessor->getRightRGB());
 
        // logPearsonData(); Pearson Test.
        // ------------------------------------------------------------
        DWORD bytesWritten;
        if (!WriteFile(hserial, ledData.data(), static_cast<DWORD>(ledData.size()), &bytesWritten, NULL)) {
            DWORD error = GetLastError();
            std::cout << "Failed to write to serial port. Error code: " << error << std::endl;
            isPortConnected.store(false);
            return;
        }
    }
}

void LEDCommunication::readSerialData() {
    if (!isPortConnected.load() || hserial == INVALID_HANDLE_VALUE) {
        std::cout << "Port is not connected or invalid handle value" << std::endl;
        return;
    }

    // Check if data is available
    DWORD errors;
    COMSTAT status;
    if (!ClearCommError(hserial, &errors, &status)) {
        DWORD error = GetLastError();
        std::cout << "ClearCommError failed with error: " << error << std::endl;
        return;
    }

    if (status.cbInQue > 0) {
        // Allocate buffer for incoming data
        std::vector<char> buffer(status.cbInQue);
        DWORD bytesRead = 0;

        // Read the data
        if (ReadFile(hserial, buffer.data(), status.cbInQue, &bytesRead, nullptr)) {
            // Convert to string and process the data
            std::string receivedData(buffer.data(), bytesRead);
            // std::cout << "Received: " << receivedData << std::endl;
            
            // Process the received data - assuming format "ESP32:timestamp,leftBrightness,rightBrightness"
            if (receivedData.find("ESP32:") == 0) {
                try {
                    // Remove "ESP32:" prefix
                    std::string data = receivedData.substr(6);
                    
                    // Parse the comma-separated values
                    std::stringstream ss(data);
                    std::string item;
                    std::vector<std::string> values;
                    
                    while (std::getline(ss, item, ',')) {
                        values.push_back(item);
                    }
                    
                    if (values.size() >= 3) {
                        // Convert values to appropriate types
                        uint64_t esp32ElapsedTime = std::stoull(values[0]); // This is already elapsed time
                        float leftBrightness = std::stof(values[1]);
                        float rightBrightness = std::stof(values[2]);
                        
                        // Log data to ESP32Data.csv file
                        if (esp32DataFile.is_open()) {
                            esp32DataFile << esp32ElapsedTime << ","
                                        << static_cast<int>(leftBrightness) << ","
                                        << static_cast<int>(rightBrightness) << "\n";
                            esp32DataFile.flush(); // Ensure data is written immediately
                        }
                    }
                } catch (const std::exception& e) {
                    std::cout << "Error parsing data: " << e.what() << std::endl;
                }
            }
        } else {
            DWORD error = GetLastError();
            std::cout << "ReadFile failed with error: " << error << std::endl;
        }
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

bool LEDCommunication::checkConnection(){
    if (shouldReconnect.load() || (!isPortConnected.load() && juce::Time::currentTimeMillis() - lastRetryTime > RETRY_INTERVAL_MS)) {
            shouldReconnect.store(false);
            lastRetryTime = juce::Time::currentTimeMillis();
            
            std::cout << "Attempting to connect to port: " << portName << " at " << juce::Time::getCurrentTime().toString(true, true) << std::endl;
            if (!tryConnect()) {
                std::cout << "Connection attempt failed - will retry in " << RETRY_INTERVAL_MS/1000 << " seconds" << std::endl;
                wait(100);
                return false;
            }
            std::cout << "Successfully connected to port: " << portName << std::endl;
        }

        if (!isPortConnected.load()) {
            wait(100);
            return false;
        }
    return true;
}

void LEDCommunication::initializePearsonData()
{
    // Go one level up from AUDIO_PRO directory
    std::string basePath = "C:\\Users\\amit5\\Desktop\\AUDIO_PRO\\audio-plugin-template";
    std::string pearsonPath = basePath + "\\PearsonData";
    
    // Create directory using Windows API
    if (CreateDirectoryA(pearsonPath.c_str(), NULL) || 
        GetLastError() == ERROR_ALREADY_EXISTS) {
        
        // Create/open the DSP data CSV file
        std::string dspPath = pearsonPath + "\\dspData.csv";
        pearsonDataFile.open(dspPath, std::ios::out);
        
        // Create/open the ESP32 data CSV file
        std::string esp32Path = pearsonPath + "\\ESP32Data.csv";
        esp32DataFile.open(esp32Path, std::ios::out);
        
        if (pearsonDataFile.is_open() && esp32DataFile.is_open()) {
            // Write headers to both files
            pearsonDataFile << "timestamp_ms,left_brightness,right_brightness\n";
            esp32DataFile << "timestamp_ms,left_brightness,right_brightness\n";
        } else {
            std::cout << "Failed to create/open one or both CSV files" << std::endl;
            if (!pearsonDataFile.is_open()) std::cout << "Failed to open dspData.csv" << std::endl;
            if (!esp32DataFile.is_open()) std::cout << "Failed to open ESP32Data.csv" << std::endl;
        }
    } else {
        std::cout << "Failed to create PearsonData directory! Error: " << GetLastError() << std::endl;
    }
}

void LEDCommunication::logPearsonData()
{
    juce::uint32 currentTimeMs = juce::Time::getMillisecondCounter();
    if (pearsonDataFile.is_open()) {
        juce::uint32 elapsedMs = currentTimeMs - (startTimeMs + 37);
        
        pearsonDataFile << elapsedMs << "," 
                       << currentLeftBrightness.load() << "," 
                       << currentRightBrightness.load() << "\n";
    }
}   


void LEDCommunication::run() {
    std::cout << "=== LED Communication Thread Started ===" << std::endl;
    std::cout << "Initial port: " << portName << std::endl;
    startTimeMs = juce::Time::getMillisecondCounter();
    while (!threadShouldExit()) {
        if(!checkConnection()) continue;
//------------------------------run operation--------------------------------
        sendData();

        // readSerialData(); Pearson Test.
        wait(16);
    }
    std::cout << "=== LED Communication Thread Stopping ===" << std::endl;
}

} // namespace audio_plugin