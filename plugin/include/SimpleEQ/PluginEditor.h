#pragma once

#include "PluginProcessor.h"

namespace audio_plugin {
struct CustomRotarySlider : juce::Slider {
  CustomRotarySlider() : juce::Slider(juce::Slider::SliderStyle::RotaryHorizontalVerticalDrag,
  juce::Slider::TextEntryBoxPosition::NoTextBox)
  {

  }
};
class AudioPluginAudioProcessorEditor : public juce::AudioProcessorEditor,
juce::AudioProcessorParameter::Listener, 
juce::Timer {
public:
  explicit AudioPluginAudioProcessorEditor(AudioPluginAudioProcessor&);
  ~AudioPluginAudioProcessorEditor() override;

  void paint(juce::Graphics&) override;
  void resized() override;

  void parameterValueChanged (int parameterIndex, float newValue) override;
  void parameterGestureChanged (int parameterIndex, bool gestureIsStarting) override {  juce::ignoreUnused(parameterIndex, gestureIsStarting); }
  void timerCallback() override;
private:
  // This reference is provided as a quick way for your editor to
  // access the processor object that created it.
  AudioPluginAudioProcessor& processorRef;
  juce::Atomic<bool> parametersChanged {false};
  CustomRotarySlider 
  peakFreqSlider, 
  peakGainSlider, 
  peakQualitySlider,
  lowCutFreqSlider, 
  highCutFreqSlider,
  lowCutSlopeSlider,
  highCutSlopeSlider; 


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

  MonoChain monoChain;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessorEditor)
};
}  // namespace audio_plugin
