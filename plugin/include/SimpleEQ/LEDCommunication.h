#pragma once
#include "PluginProcessor.h"
#include "CommonDef.h"
#include <vector>

class LEDCommunication : public juce::Thread {
  public:
    LEDCommunication(const juce::String& portName);
    ~LEDCommunication();
    void run() override;
    void setColor(Color c){currentColor.store(c);}
    void setBrightness(float brightness){ currentBrightness.store(static_cast<int>(brightness)); }
    
  private:
    juce::String portName;
    std::atomic<Color> currentColor = Color::Yellow;

    std::atomic<int> currentBrightness = 100;
   
};
