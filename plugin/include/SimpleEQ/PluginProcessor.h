#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <memory>
#include "CommonDef.h"
#include "LEDCommunication.h"
#include "FFTProcessor.h"

namespace audio_plugin {

// Define common types before using them
using BlockType = juce::AudioBuffer<float>;

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

  //Shared instances:
  SingleChannelSampleFifo<float> leftChannelFifo{Channel::Left};                                        
  SingleChannelSampleFifo<float> rightChannelFifo{Channel::Right};

  std::shared_ptr<FFTProcessor> leftChannelFFTProcessor;
  std::shared_ptr<FFTProcessor> rightChannelFFTProcessor;
  
  std::shared_ptr<LEDCommunication> ledComm;
  
  // Add getter for LEDCommunication
  std::shared_ptr<LEDCommunication> getLEDCommunication() { return ledComm; }

  // Audio file loading and playback methods
  bool loadFile(const juce::String& path);
  bool isFileLoaded() const { return fileLoaded; }
  void setPosition(double positionInSecs);
  double getLengthInSeconds() const;
  void setLooping(bool shouldLoop);
  bool isLooping() const;
  void startPlayback();
  void stopPlayback();
  void updateLEDs(float leftLevel, float rightLevel);
  bool isPlaying() const { return playing; }

  // Add channel level tracking
  juce::Atomic<float> leftChannelLevel{0.0f};
  juce::Atomic<float> rightChannelLevel{0.0f};
  
  // Helper to calculate channel level
  float calculateChannelLevel(const juce::AudioBuffer<float>& buffer, int channel);

private:
  // Audio file playback members
  juce::AudioFormatManager formatManager;
  std::unique_ptr<juce::AudioFormatReader> formatReader;
  juce::AudioSampleBuffer fileBuffer;
  int position = 0;
  bool fileLoaded = false;
  bool playing = false;
  bool looping = false;
  double mySampleRate = 44100.0;
  
  juce::dsp::Oscillator<float> osc;  // Add oscillator
  
  #if JUCE_DEBUG
      std::unique_ptr<juce::FileLogger> fileLogger;
  #endif
              
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessor)
};

}  // namespace audio_plugin
