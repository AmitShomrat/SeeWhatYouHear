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

class LEDCommunication : public juce::Thread {
  public:
    LEDCommunication(const juce::String& portName, ColorDecisionML& leftColorDecisionML, ColorDecisionML& rightColorDecisionML);
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

    void setBrightness(float leftBrightness, float rightBrightness, float userBrightness)
    { 
      // Use decibels scaling with expanded range (-60dB to 0dB)
      float leftScaled = juce::Decibels::decibelsToGain(leftBrightness * 60.0f - 60.0f);
      float rightScaled = juce::Decibels::decibelsToGain(rightBrightness * 60.0f - 60.0f);

      // Apply user brightness with non-linear scaling to maintain sensitivity at lower values
      // Normalize userBrightness to 0-1 range considering the max value of 0.135f
      float normalizedBrightness = userBrightness / 0.135f;
      
      // Apply more aggressive exponential scaling for better control at lower values
      float brightnessScale = std::pow(normalizedBrightness, 2.0f);  // Square for more aggressive curve at low values

      float mappedValueLeft = juce::jmap(
          leftScaled,
          juce::Decibels::decibelsToGain(-60.0f),   // input range start (minimum gain)
          juce::Decibels::decibelsToGain(0.0f),     // input range end (maximum gain = 1.0)
          0.0f,   // output range start
          255.0f  // output range end
      ) * brightnessScale;  // Apply user brightness scaling

      float mappedValueRight = juce::jmap(
          rightScaled,
          juce::Decibels::decibelsToGain(-60.0f),   // input range start (minimum gain)
          juce::Decibels::decibelsToGain(0.0f),     // input range end (maximum gain = 1.0)
          0.0f,   // output range start
          255.0f  // output range end
      ) * brightnessScale;  // Apply user brightness scaling

      // Convert to int with rounding
      currentLeftBrightness.store(static_cast<int>(std::round(mappedValueLeft))); 
      currentRightBrightness.store(static_cast<int>(std::round(mappedValueRight)));
      // std::cout << "currentLeftBrightness: " << currentLeftBrightness.load() << std::endl;
      // std::cout << "currentRightBrightness: " << currentRightBrightness.load() << std::endl;
    }
  private:
    void prepareData(RGB rgbLeftValues, RGB rgbRightValues);
    bool tryConnect();
    
    juce::String portName;
    const int numLEDs = 300;
    std::vector<unsigned char> ledData; 
    HANDLE hserial;

    std::atomic<Color> currentColor{Color::Yellow};
    std::atomic<int> currentLeftBrightness{0};
    std::atomic<int> currentRightBrightness{0};
    std::atomic<bool> isPortConnected{false};
    std::atomic<bool> shouldReconnect{false};

    ColorDecisionML& leftColorDecisionML;
    ColorDecisionML& rightColorDecisionML;

    // Connection retry parameters
    static constexpr int RETRY_INTERVAL_MS = 5000; // 5 seconds between retries
};

} // namespace audio_plugin
