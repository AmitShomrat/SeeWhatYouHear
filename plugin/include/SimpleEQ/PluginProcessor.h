#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_formats/juce_audio_formats.h>

#include <array>
namespace audio_plugin {
template <typename T>
struct Fifo
{
  void prepare(int numChannels, int numSamples)
  {
    static_assert(std::is_same_v<T, juce::AudioBuffer<float>>, "prepare(numChannels, numSamples) should only be used with juce::AudioBuffer<float>");
    for (auto& buffer : buffers)
    {
      buffer.setSize(numChannels,
                     numSamples,
                     false,  //clear everything?
                     true,   //including the extra space? 
                     true);  //avoid reallocating if you can?
      buffer.clear();
    }
  }

  void prepare(size_t numElements)
  {
    static_assert(std::is_same_v<T, std::vector<float>>, "prepare(numElements) should only be used with std::vector<float>");

    for (auto& buffer : buffers)
    {
      buffer.clear();
      buffer.resize(numElements, 0);
    }
  }

  bool push(const T& t)
  {
    auto write = fifo.write(1);
    if (write.blockSize1 > 0)
    {
      buffers[write.startIndex1] = t;
      return true;
    }
    return false;
  }

  bool pull(T& t)
  {
    auto read = fifo.read(1);
    if (read.blockSize1 > 0)
    {
      t = buffers[read.startIndex1];
      return true;
    }
    return false;
  }
  
  int getNumAvailableForReading() const 
  {
    return fifo.getNumReady();
  }
  
  private:
    static constexpr int Capacity = 30;
    std::array<T, Capacity> buffers;
    juce::AbstractFifo fifo {Capacity};
};

// Define common types before using them
using BlockType = juce::AudioBuffer<float>;

enum Channel
{
  Right, //effectively 0
  Left //effectively 1
};  

template <typename Type>
struct SingleChannelSampleFifo
{
  SingleChannelSampleFifo(Channel ch) : channelToUse(ch)
  {
    prepared.set(false);
  }

  void process(const juce::AudioBuffer<Type>& buffer)
  {
    jassert(prepared.get());
    jassert(buffer.getNumChannels() > channelToUse);
    auto* channelPtr = buffer.getReadPointer(channelToUse);

    for(int i = 0; i < buffer.getNumSamples(); ++i)
    {
      pushNextSampleIntoFifo(channelPtr[i]);
    }
  }
  
  void prepare(int bufferSize)
  {
    prepared.set(false);
    size.set(bufferSize);

    bufferToFill.setSize(1, 
                         bufferSize,
                         false, 
                         true, 
                         true);
    audioBufferFifo.prepare(1, bufferSize);
    fifoIndex = 0;
    prepared.set(true);
  }
  
  int getNumCompleteBuffersAvailable() const { return audioBufferFifo.getNumAvailableForReading(); }
  bool isPrepared() const { return prepared.get(); }
  int getSize() const { return size.get(); }
  bool getAudioBuffer(juce::AudioBuffer<Type>& buf) { return audioBufferFifo.pull(buf); }
  
private:
  Channel channelToUse;
  int fifoIndex = 0;
  Fifo<juce::AudioBuffer<Type>> audioBufferFifo;
  juce::AudioBuffer<Type> bufferToFill;
  juce::Atomic<bool> prepared {false};
  juce::Atomic<int> size = 0;

  void pushNextSampleIntoFifo(Type sample) 
  {
    if(fifoIndex == bufferToFill.getNumSamples())
    {
      auto ok = audioBufferFifo.push(bufferToFill);
      juce::ignoreUnused(ok);
      fifoIndex = 0;
    }
    bufferToFill.setSample(0, fifoIndex, sample);
    ++fifoIndex;
  }  
};

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
  Slope lowCutSlope{Slope::Slope_12}, highCutSlope{Slope::Slope_12};
};

ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts);

  //The using k.w is for aliasing.
  using Filter = juce::dsp::IIR::Filter<float>;
  //CutFilter is a chain of 4 filters because we have 4 bands. and then passing a processing context through eace member of the chain automatically.
  using CutFilter = juce::dsp::ProcessorChain<Filter, Filter, Filter, Filter>;
  //MonoChain is a chain of 3 filters (2 cut filters(Low and High) and 1 peaking filter).
  using MonoChain = juce::dsp::ProcessorChain<CutFilter, Filter, CutFilter>;

enum ChainPositions {
    LowCut,
    Peak,
    HighCut
  };

  using Coefficients = Filter::CoefficientsPtr;
  void updateCoefficients(Coefficients& old, const Coefficients& replacements);

  Coefficients makePeakFilter(const ChainSettings& chainSettings, double sampleRate);

  template <int index, typename ChainType, typename CoefficientType>
  void update(ChainType& cutFilter, const CoefficientType& coefficients)
  {
    updateCoefficients(cutFilter.template get<index>().coefficients, coefficients[index]);
    cutFilter.template setBypassed<index>(false);
    // cutFilter.template get<index>().coefficients = *coefficients[index];
  }

  template <typename ChainType, typename CoefficientType>
  void updateCutFilters(ChainType& monoCutFilter, 
                        const CoefficientType& cutCoefficients, 
                        const Slope& slope )
  {
    monoCutFilter.template setBypassed<0>(true);
    monoCutFilter.template setBypassed<1>(true);
    monoCutFilter.template setBypassed<2>(true);
    monoCutFilter.template setBypassed<3>(true);

    switch ( slope )
    {
      case Slope_48:
        update<3>(monoCutFilter, cutCoefficients);
      case Slope_36:
        update<2>(monoCutFilter, cutCoefficients);
      case Slope_24:
        update<1>(monoCutFilter, cutCoefficients);
      case Slope_12:
        update<0>(monoCutFilter, cutCoefficients);
    }
  }
  inline auto makeLowCutFilter(const ChainSettings& chainSettings, double sampleRate)
  {
    return juce::dsp::FilterDesign<float>::designIIRHighpassHighOrderButterworthMethod(chainSettings.lowCutFreq,
                                                                                                      sampleRate, 2 * (1 + chainSettings.lowCutSlope) );
  }

  inline auto makeHighCutFilter(const ChainSettings& chainSettings, double sampleRate)
  {
    return juce::dsp::FilterDesign<float>::designIIRLowpassHighOrderButterworthMethod(chainSettings.highCutFreq,
                                                                                                      sampleRate,2 * (1 + chainSettings.highCutSlope) );
  }

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

  // Use float instead of BlockType as the template parameter
  SingleChannelSampleFifo<float> leftChannelFifo {Channel::Left};                                        
  SingleChannelSampleFifo<float> rightChannelFifo {Channel::Right};

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
  double mySampleRate = 44100.0;
  
  void updatePeakFilter(const ChainSettings& chainSettings);   
  void updateLowCutFilters(const ChainSettings& chainSettings);
  void updateHighCutFilters(const ChainSettings& chainSettings);
  void updateFilters ();                     
  
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessor)
};
}  // namespace audio_plugin
