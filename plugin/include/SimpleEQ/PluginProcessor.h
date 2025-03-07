#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_formats/juce_audio_formats.h>

namespace audio_plugin {

enum Slope { 
  Slope_12,
  Slope_24,
  Slope_36,
  Slope_48
};

struct ChainSettings {
  float lowCutFreq{0}, highCutFreq{0}, 
  peakFreq{0}, peakGainInDecibels{0},
  peakQuality{1.f};
  int lowCutSlope{static_cast<int>(Slope::Slope_12)}, highCutSlope{static_cast<int>(Slope::Slope_12)};
};

ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts);

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

  enum ChainPositions {
    LowCut,
    Peak,
    HighCut
  };
  // Audio file playback members
  juce::AudioFormatManager formatManager;
  std::unique_ptr<juce::AudioFormatReader> formatReader;
  juce::AudioSampleBuffer fileBuffer;
  int position = 0;
  bool fileLoaded = false;
  bool playing = false;
  bool looping = false;
  double mySampleRate = 44100.0;
  
  void updatePeakFilter(const ChainSettings& chainSettings);
  using Coefficients = Filter::CoefficientsPtr;
  static void updateCoefficients(Coefficients& old, const Coefficients& replacements);
  template <int index, typename ChainType, typename CoefficientType>
  void update(ChainType& cutFilter, const CoefficientType& coefficients)
  {
    updateCoefficients(cutFilter.template get<index>().coefficients, coefficients[index]);
    cutFilter.template setBypassed<index>(false);
    cutFilter.template get<index>().coefficients = *coefficients[index];
  }
  // Update the cut filters for the left channel
  template <typename ChainType, typename CoefficientType>
  void updateCutFilters(ChainType& leftLowCut, 
                        const CoefficientType& cutCoefficients, 
                        const ChainSettings chainSettings )
  {
    leftLowCut.template setBypassed<0>(true);
    leftLowCut.template setBypassed<1>(true);
    leftLowCut.template setBypassed<2>(true);
    leftLowCut.template setBypassed<3>(true);

    switch (chainSettings.lowCutSlope)
    {
      case Slope_48:
        update<3>(leftLowCut, cutCoefficients);
        break;
      case Slope_36:
        update<2>(leftLowCut, cutCoefficients);
        break;
      case Slope_24:
        update<1>(leftLowCut, cutCoefficients);
        break;
      case Slope_12:
        update<0>(leftLowCut, cutCoefficients);
        break;
    }

    // switch (chainSettings.lowCutSlope)
    // {
    // case Slope_12:
    //   leftLowCut.template setBypassed<0>(false);
    //   leftLowCut.template get<0>().coefficients = *cutCoefficients[0];
    //   break;
    
    // case Slope_24:
    //   leftLowCut.template setBypassed<0>(false);
    //   leftLowCut.template get<0>().coefficients = *cutCoefficients[0];
    //   leftLowCut.template setBypassed<1>(false);
    //   leftLowCut.template get<1>().coefficients = *cutCoefficients[1];
    //   break;

    // case Slope_36:
    //   leftLowCut.template setBypassed<0>(false);
    //   leftLowCut.template get<0>().coefficients = *cutCoefficients[0];
    //   leftLowCut.template setBypassed<1>(false);
    //   leftLowCut.template get<1>().coefficients = *cutCoefficients[1];
    //   leftLowCut.template setBypassed<2>(false);
    //   leftLowCut.template get<2>().coefficients = *cutCoefficients[2];
    //   break;

    // case Slope_48:
    //   leftLowCut.template setBypassed<0>(false);
    //   leftLowCut.template get<0>().coefficients = *cutCoefficients[0];
    //   leftLowCut.template setBypassed<1>(false);
    //   leftLowCut.template get<1>().coefficients = *cutCoefficients[1];
    //   leftLowCut.template setBypassed<2>(false);
    //   leftLowCut.template get<2>().coefficients = *cutCoefficients[2];
    //   leftLowCut.template setBypassed<3>(false);
    //   leftLowCut.template get<3>().coefficients = *cutCoefficients[3];
    // }
  }                        
  
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessor)
};
}  // namespace audio_plugin
