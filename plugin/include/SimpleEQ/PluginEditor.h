#pragma once

#include "PluginProcessor.h"

namespace audio_plugin {
struct LookAndFeel : juce::LookAndFeel_V4 {
  void drawRotarySlider(juce::Graphics& g,
                        int x,
                        int y,
                        int width,
                        int height, 
                        float sliderPosProportional, 
                        float rotaryStartAngle, 
                        float rotaryEndAngle, 
                        juce::Slider& slider) override { juce::ignoreUnused(g, x, y, width, height, sliderPosProportional, rotaryStartAngle, rotaryEndAngle, slider); }
};
struct RotarySliderWithLabels : juce::Slider {
  RotarySliderWithLabels(juce::RangedAudioParameter& rap, const juce::String& unitSuffix):
  juce::Slider(juce::Slider::SliderStyle::RotaryHorizontalVerticalDrag, juce::Slider::TextEntryBoxPosition::NoTextBox),
  param(&rap),
  suffix(unitSuffix)
  {
    setLookAndFeel(&lnf);
  }
  ~RotarySliderWithLabels() {
    setLookAndFeel(nullptr);
  }

  void paint(juce::Graphics& g)override { juce::ignoreUnused(g); }
  juce::Rectangle<int> getSliderBounds() const;
  int getTextHeight() const{return 14;}
  juce::String getDisplayString() const;
  private:
    LookAndFeel lnf;
    juce::RangedAudioParameter* param;
    juce::String suffix;
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
  private:
    AudioPluginAudioProcessor& processorRef;
    juce::Atomic<bool> parametersChanged {false};
    MonoChain monoChain;
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
  // Add components here.
  RotarySliderWithLabels 
  peakFreqSlider, 
  peakGainSlider, 
  peakQualitySlider,
  lowCutFreqSlider, 
  highCutFreqSlider,
  lowCutSlopeSlider,
  highCutSlopeSlider; 
  ResponseCurveComponent responseCurveComponent;

  using APVTS = juce::AudioProcessorValueTreeState;
  using Attachment = APVTS::SliderAttachment;
  Attachment peakFreqSliderAttachment,
  peakGainSliderAttachment,
  peakQualitySliderAttachment,
  lowCutFreqSliderAttachment,
  highCutFreqSliderAttachment,
  lowCutSlopeSliderAttachment,
  highCutSlopeSliderAttachment;

  std::vector<juce::Component*> getComps();


  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessorEditor)
};
} // namespace audio_plugin
