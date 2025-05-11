#pragma once

#include <juce_dsp/juce_dsp.h>
#include "CommonDef.h"
#include <vector>

namespace audio_plugin {

enum FFTOrder {
  order2048 = 11,
  order4096 = 12,
  order8192 = 13,
};

template <typename BlockType>
struct FFTDataGenerator
{
  /** 
   * produces the FFT data from an audio buffer 
   */ 
  void produceFFTDataForRendering(const juce::AudioBuffer<float>& audioData, const float negativeInfinity)
  {
    const auto fftSize = getFFTSize();
  
    fftData.assign(fftData.size(), 0.f);
    auto* readIndex = audioData.getReadPointer(0);
    std::copy(readIndex, readIndex + fftSize, fftData.begin());

    window->multiplyWithWindowingTable(fftData.data(), static_cast<size_t>(fftSize));

    forwardFFT->performFrequencyOnlyForwardTransform(fftData.data());

    int numBins = static_cast<int>(fftSize) / 2;

    for (int i = 0; i < numBins; ++i)
    {
      fftData[i] /= static_cast<float>(numBins);
    }

    for (int i = 0; i < numBins; ++i)
    {
      fftData[i] = juce::Decibels::gainToDecibels(fftData[i], negativeInfinity);
    }

    fftDataFifo.push(fftData);
  }

  void changeOrder(FFTOrder newOrder)
  {
    order = newOrder;
    auto fftSize = getFFTSize();

    forwardFFT = std::make_unique<juce::dsp::FFT>(order);
    window = std::make_unique<juce::dsp::WindowingFunction<float>>(fftSize, juce::dsp::WindowingFunction<float>::blackmanHarris);

    fftData.clear();
    fftData.resize(static_cast<size_t>(fftSize * 2), 0.f);
    fftDataFifo.prepare(fftData.size());
  }

  int getFFTSize() const { return 1 << order; }
  int getNumAvailableFFTDataBlocks() const { return fftDataFifo.getNumAvailableForReading(); }

  bool getFFTData(BlockType& returnedData)
  {
    return fftDataFifo.pull(returnedData);
  }

private:
  FFTOrder order;
  BlockType fftData;
  std::unique_ptr<juce::dsp::FFT> forwardFFT;
  std::unique_ptr<juce::dsp::WindowingFunction<float>> window;
  Fifo<BlockType> fftDataFifo;
};


class FFTProcessor : juce::Thread {
public:

    FFTProcessor(SingleChannelSampleFifo<float>& scsf) : juce::Thread("FFTProcessorThread"), 
    monoChannelFifo(&scsf){
        monoChannelFFTDataGenerator.changeOrder(FFTOrder::order2048);
        monoBuffer.setSize(1, monoChannelFFTDataGenerator.getFFTSize());
        startThread();
    }
    ~FFTProcessor(){};
    void prepare(double sampleRateFromProcessor){
        sampleRate = sampleRateFromProcessor;
    }
    double getFFTBinWidth() const {return sampleRate / static_cast<double>(monoChannelFFTDataGenerator.getFFTSize());}
    void process();
    void run() override;
    bool isAvailable() const {return monoChannelFFTDataGenerator.getNumAvailableFFTDataBlocks() > 0;}
    bool getLatestFFTData(std::vector<float>& fftData) {return monoChannelFFTDataGenerator.getFFTData(fftData);}
    int getFFTSize() const {return monoChannelFFTDataGenerator.getFFTSize();}
private:
    SingleChannelSampleFifo<float>* monoChannelFifo;
    FFTDataGenerator<std::vector<float>> monoChannelFFTDataGenerator;
    juce::AudioBuffer<float> monoBuffer;
    double sampleRate;

};
} // namespace audio_plugin 