#pragma once
#include "CommonDef.h"  // Must come first for Color and RGB definitions
#include "ColorDecisionML.h"
#include <atomic>
#include <vector>
#include <fstream>
#include <regex>
#include <iostream> 
#include <string>

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <juce_dsp/juce_dsp.h>  // For FastMathApproximations

#include <boost/asio.hpp> // using a cross-platform UART interface. 
#include <boost/process.hpp>
namespace bp = boost::process;

namespace audio_plugin {  // Add namespace to match Color and RGB definitions
class AudioPluginAudioProcessor;
class LEDCommunication : public juce::Thread {
  public:
    LEDCommunication(AudioPluginAudioProcessor* processor);
    ~LEDCommunication();
    void run() override; 
    // Device:
    void openSerial(unsigned baud = 115200);
    void closeSerial();   
    std::size_t writeBytes(const uint8_t* data, std::size_t n);
    std::size_t readSome(uint8_t* dst, std::size_t max);

  private:
    void changeMode();
    int setBrightness(float monoBrightness);
    void prepareData(RGB rgbLeftValues, RGB rgbRightValues);
    void sendData();

    std::atomic<int> currentMode{0};
    std::atomic<int> currentLeftBrightness{0};
    std::atomic<int> currentRightBrightness{0};
    
    // Reference and pointers
    AudioPluginAudioProcessor* processorPointer = nullptr;
    
    // Leds Objects
    const int numLEDs = 300;
    std::vector<unsigned char> ledData;

    // Serial port connection
    bool checkConnection();
    

    std::string portName;
    boost::asio::io_context io_;
    std::unique_ptr<boost::asio::serial_port> serial_;


    juce::int64 lastRetryTime = 0;
    static constexpr int RETRY_INTERVAL_MS = 5000; // 5 seconds between retries
    
    // Test pearsons members
    // void initializePearsonData();
    // void logPearsonData();
    // void readSerialData();
    // std::ofstream pearsonDataFile;
    // std::ofstream esp32DataFile;

    static const size_t SERIAL_BUFFER_SIZE = 256;
    char serialBuffer[SERIAL_BUFFER_SIZE];
    juce::uint32 startTimeMs;
    
};

} // namespace audio_plugin
