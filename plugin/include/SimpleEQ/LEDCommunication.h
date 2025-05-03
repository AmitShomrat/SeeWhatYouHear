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

    void setBrightness(float leftBrightness, float rightBrightness )
    { 
      currentLeftBrightness.store(static_cast<int>(leftBrightness)); 
      currentRightBrightness.store(static_cast<int>(rightBrightness));
    }
    void prepareData(RGB rgbLeftValues, RGB rgbRightValues)
    {
      ledData[0] = static_cast<unsigned char>(0xFF);
      ledData[1] = static_cast<unsigned char>(currentLeftBrightness.load());
      ledData[2] = static_cast<unsigned char>(currentRightBrightness.load());
      //TODO add Right Brightness.
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

      // for(int i = ( numLEDs / 2 ); i < numLEDs - 1 ; ++i)
      // {
      //   ledData[i * 3 + 2] = static_cast<unsigned char>(rgbRightValues.r);
      //   ledData[i * 3 + 3] = static_cast<unsigned char>(rgbRightValues.g);
      //   ledData[i * 3 + 4] = static_cast<unsigned char>(rgbRightValues.b);
      // }
    }

  private:
    juce::String portName;
    const int numLEDs = 300;
    std::vector<unsigned char> ledData; 

    std::atomic<Color> currentColor = Color::Yellow;
    std::atomic<int> currentLeftBrightness = 0;
    std::atomic<int> currentRightBrightness = 0;

};
