#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_formats/juce_audio_formats.h>


namespace audio_plugin {
class AudioPluginAudioProcessor : public juce::AudioProcessor {
public:
  AudioPluginAudioProcessor();
  ~AudioPluginAudioProcessor() override;

  void prepareToPlay(double sampleRate, int samplesPerBlock) override;
  void releaseResources() override;

  bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

  void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
  using AudioProcessor::processBlock;

  juce::AudioProcessorEditor* createEditor() override;
  bool hasEditor() const override;

  const juce::String getName() const override;

  bool acceptsMidi() const override;
  bool producesMidi() const override;
  bool isMidiEffect() const override;
  double getTailLengthSeconds() const override;

  int getNumPrograms() override;
  int getCurrentProgram() override;
  void setCurrentProgram(int index) override;
  const juce::String getProgramName(int index) override;
  void changeProgramName(int index, const juce::String& newName) override;

  void getStateInformation(juce::MemoryBlock& destData) override;
  void setStateInformation(const void* data, int sizeInBytes) override;

  static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
  juce::AudioProcessorValueTreeState apvts{*this, nullptr, "PARAMETERS",
                                          createParameterLayout()};

  // Audio file loading and playback methods
  bool loadFile(const juce::String& path);
  bool isFileLoaded() const { return fileLoaded; }
  void setPosition(double positionInSecs);
  double getLengthInSeconds() const;
  void setLooping(bool shouldLoop);
  bool isLooping() const;
  void startPlayback();
  void stopPlayback();
  bool isPlaying() const { return playing; }

private:

  // The using k.w is for aliasing.
  using Filter = juce::dsp::IIR::Filter<float>;
  //CutFilter is a chain of 4 filters because we have 4 bands. and then passing a processing context through eace member of the chain automatically.
  using CutFilter = juce::dsp::ProcessorChain<Filter, Filter, Filter, Filter>;
  //MonoChain is a chain of 3 filters (2 cut filters(Low and High) and 1 peaking filter).
  using MonoChain = juce::dsp::ProcessorChain<CutFilter, Filter, CutFilter>;
  //MonoChain for each channel.
  MonoChain leftChain, rightChain; 

  // Audio file playback members
  juce::AudioFormatManager formatManager;
  std::unique_ptr<juce::AudioFormatReader> formatReader;
  juce::AudioSampleBuffer fileBuffer;
  int position = 0;
  bool fileLoaded = false;
  bool playing = false;
  bool looping = false;
  
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessor)
};
}  // namespace audio_plugin
