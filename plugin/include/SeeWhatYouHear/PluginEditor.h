#pragma once

#include "PluginProcessor.h"
#include "CommonDef.h"
#include "FFTProcessor.h"
#include "ColorDecisionML.h"
#include <memory>

// Forward declaration
class LEDCommunication;
class FFTProcessor;

namespace audio_plugin {

// Add utility function to measure text width
float getTextWidth(const juce::Font& font, const juce::String& text);

template <typename PathType>
struct AnalyzerPathGenerator
{
  void generatePath(const std::vector<float>& renderData,
                    juce::Rectangle<float> fftBounds,
                    int fftSize,
                    float binWidth,
                    float negativeInfinityDb)
  {
    auto top = fftBounds.getY();
    auto bottom = fftBounds.getHeight();
    auto width = fftBounds.getWidth();

    int numBins = fftSize / 2;

    PathType p;
    p.preallocateSpace(3 * static_cast<int>(width));

    auto map = [bottom, top, negativeInfinityDb](float v)
    {
      return juce::jmap(v, 
              negativeInfinityDb, 
              0.f, 
              bottom, top);
    };

    auto y = map(renderData[0]);

    jassert( !std::isnan(y) && !std::isinf(y) );

    p.startNewSubPath(0, y);

    const int pathResolution = 2;

    for(int binNum = 1; binNum < numBins; binNum += pathResolution)
    {
      y = map(renderData[binNum]);
      jassert( !std::isnan(y) && !std::isinf(y) );

      if( !std::isnan(y) && !std::isinf(y) )
      {
        auto binFreq = binNum * binWidth;
        auto normalizedBinX = juce::mapFromLog10(binFreq, 20.f, 20000.f);
        auto binX = static_cast<float>(std::floor(normalizedBinX * width));
        p.lineTo(binX, y);
      }
    }

    pathFifo.push(p);
  }

  int getNumPathsAvailable() const { return pathFifo.getNumAvailableForReading(); }
  bool getPath(PathType& path) { return pathFifo.pull(path); }

private:
  Fifo<PathType> pathFifo;
};


struct LookAndFeel : juce::LookAndFeel_V4 {
  void drawRotarySlider(juce::Graphics& g,
                        int x,
                        int y,
                        int width,
                        int height, 
                        float sliderPosProportional, 
                        float rotaryStartAngle, 
                        float rotaryEndAngle, 
                        juce::Slider& slider) override;
};
struct RotarySliderWithLabels : juce::Slider {
  RotarySliderWithLabels(juce::RangedAudioParameter& rap, juce::String unitSuffix):
  juce::Slider(juce::Slider::SliderStyle::RotaryHorizontalVerticalDrag, juce::Slider::TextEntryBoxPosition::NoTextBox),
  suffix(unitSuffix), param(&rap)
  {
    setLookAndFeel(&lnf);
  }
  ~RotarySliderWithLabels() {
    setLookAndFeel(nullptr);
  }

  struct LabelPos {
    float pos;
    juce::String label;
  };
  
  juce::Array<LabelPos> labels;


  void paint(juce::Graphics& g)override;
  juce::Rectangle<int> getSliderBounds() const;
  int getTextHeight() const{return 14;}
  juce::String getDisplayString() const;
  private:
    LookAndFeel lnf;
    juce::String suffix;
    juce::RangedAudioParameter* param;
    
};

struct PathProducer {
  PathProducer(std::shared_ptr<FFTProcessor> fftProcessor) : monoChannelFFTProcessor(fftProcessor){}

  void process(juce::Rectangle<float> fftBounds, double sampleRate);
  juce::Path getPath() {return leftChannelFFTPath;}
  private:
    //TODO: This class might not have the FFTProcessor member. take it from the audioProcessor as you use it.
    std::shared_ptr<FFTProcessor> monoChannelFFTProcessor;

    AnalyzerPathGenerator<juce::Path> pathProducer;

    juce::Path leftChannelFFTPath;
};

struct ResponseCurveComponent : juce::Component, 
juce::AudioProcessorParameter::Listener, juce::Timer 
{
  ResponseCurveComponent(AudioPluginAudioProcessor&);
  ~ResponseCurveComponent() override;

  void parameterValueChanged (int parameterIndex, float newValue) override;

  void parameterGestureChanged (int parameterIndex, bool gestureIsStarting) override {  juce::ignoreUnused(parameterIndex, gestureIsStarting); }

  void timerCallback() override;

  void paint(juce::Graphics&) override;

  void resized() override;
  private:
    AudioPluginAudioProcessor& processorRef;
   // LEDCommunication& ledCommunication;
    juce::Atomic<bool> parametersChanged {false};
 
    // MonoChain monoChain;

    void updateChain();

    juce::Image background;

    juce::Rectangle<int> getRenderArea();

    juce::Rectangle<int> getAnalysisArea();

    PathProducer leftPathProducer, rightPathProducer;
   
};

// Add LEDSimulator component
struct LEDSimulator : juce::Component, juce::Timer
{
    LEDSimulator(AudioPluginAudioProcessor& p);
    ~LEDSimulator() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;
    juce::Rectangle<float> getLEDArea();
private:
    AudioPluginAudioProcessor& processorRef;
    float leftChannelLevel = {0.0f};
    float rightChannelLevel = {0.0f};
    // ColorDecisionML& leftColorDecisionML;
    // ColorDecisionML& rightColorDecisionML;
    RGB currentLeftRGB{/*RGB{0, 0, 0}*/};
    RGB currentRightRGB{/*RGB{0, 0, 0}*/};
    
    void drawLED(juce::Graphics& g, juce::Rectangle<float> bounds, float brightness, juce::Colour color);
};

class AudioPluginAudioProcessorEditor : public juce::AudioProcessorEditor {
public:
  explicit AudioPluginAudioProcessorEditor(AudioPluginAudioProcessor&);
  ~AudioPluginAudioProcessorEditor() override;

  void paint(juce::Graphics&) override;
  void resized() override;

private:
  // This reference is provided as a quick way for your editor to
  // access the processor object that created it.
  AudioPluginAudioProcessor& processorRef;
  
  std::shared_ptr<LEDCommunication> ledComm;

  // Add components here.
  RotarySliderWithLabels brightnessSlider, colorSlider; 
  ResponseCurveComponent responseCurveComponent;
  LEDSimulator ledSimulator;

  using APVTS = juce::AudioProcessorValueTreeState;
  using Attachment = APVTS::SliderAttachment;
  Attachment brightnessSliderAttachment, colorSliderAttachment;

  std::vector<juce::Component*> getComps();


  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessorEditor)
};
} // namespace audio_plugin
