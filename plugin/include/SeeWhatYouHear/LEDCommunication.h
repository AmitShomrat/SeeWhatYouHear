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

enum class LEDMode {
    Static,
    Chase,
    Fade,
    Rainbow
};


class LEDCommunication : public juce::Thread {
  public:
    LEDCommunication(AudioPluginAudioProcessor* processor);
    ~LEDCommunication();
    void run() override;    
    // Add connection state query
    bool isConnected() const { return isPortConnected.load(); }
    
    // Add method to change port
    void setPort(const juce::String& newPort) {
        portName = newPort;
        shouldReconnect.store(true);
    }

  private:
    void changeMode();
    int setBrightness(float monoBrightness);
    void prepareData(RGB rgbLeftValues, RGB rgbRightValues);
    
    std::atomic<int> currentLeftBrightness{0};
    std::atomic<int> currentRightBrightness{0};
    std::atomic<bool> isPortConnected{false};
    std::atomic<bool> shouldReconnect{false};
    std::atomic<LEDMode> currentMode{LEDMode::Static};
    
    // Reference and pointers
    AudioPluginAudioProcessor* processorPointer = nullptr;
    // Serial port connection
    bool tryConnect();
    juce::String portName;
    const int numLEDs = 300;
    std::vector<unsigned char> ledData; 
    HANDLE hserial;

    // Connection retry parameters
    static constexpr int RETRY_INTERVAL_MS = 5000; // 5 seconds between retries
};

} // namespace audio_plugin
