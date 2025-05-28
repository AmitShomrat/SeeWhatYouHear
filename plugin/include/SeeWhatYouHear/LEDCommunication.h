#pragma once
#include "CommonDef.h"  // Must come first for Color and RGB definitions
#include "ColorDecisionML.h"
#include <atomic>
#include <vector>
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <juce_dsp/juce_dsp.h>  // For FastMathApproximations

// Forward declare Windows HANDLE type to avoid including windows.h in header
#ifdef _WIN32
    typedef void* HANDLE;
#endif

namespace audio_plugin {  // Add namespace to match Color and RGB definitions
class AudioPluginAudioProcessor;

class LEDCommunication : public juce::Thread {
  public:
    LEDCommunication();
    ~LEDCommunication();
    void run() override;
    void setColor(Color c) { currentColor.store(c); }
    
    // Add connection state query
    bool isConnected() const { return isPortConnected.load(); }
    
    // Add method to change port
    void setPort(const juce::String& newPort) {
        portName = newPort;
        shouldReconnect.store(true);
    }

    // void setBrightness(float leftBrightness, float rightBrightness, float userBrightness);
    AudioPluginAudioProcessor* processorPointer = nullptr;
  private:
    int setBrightness(float monoBrightness);
    void prepareData(RGB rgbLeftValues, RGB rgbRightValues);
    bool tryConnect();
    
    juce::String portName;
    const int numLEDs = 300;
    std::vector<unsigned char> ledData; 
    HANDLE hserial;

    std::atomic<Color> currentColor{Color::Yellow};
    RGB constRGB{RGB{255, 0, 0}};

    std::atomic<int> currentLeftBrightness{0};
    std::atomic<int> currentRightBrightness{0};
    std::atomic<bool> isPortConnected{false};
    std::atomic<bool> shouldReconnect{false};


    // BrightnessDecision& brightnessDecision;
    // ColorDecisionML& leftColorDecisionML;
    // ColorDecisionML& rightColorDecisionML;

    // Connection retry parameters
    static constexpr int RETRY_INTERVAL_MS = 5000; // 5 seconds between retries
};

} // namespace audio_plugin
