#pragma once

#include <juce_dsp/juce_dsp.h>
#include "CommonDef.h"
#include "ColorDecisionML.h"
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
    linearMagnitudes.assign(fftData.size() / 2, 0.f);
    
    auto* readIndex = audioData.getReadPointer(0);
    std::copy(readIndex, readIndex + fftSize, fftData.begin());

    window->multiplyWithWindowingTable(fftData.data(), static_cast<size_t>(fftSize));
    forwardFFT->performFrequencyOnlyForwardTransform(fftData.data());

    int numBins = static_cast<int>(fftSize) / 2;

    // Store linear magnitudes before decibel conversion
    for (int i = 0; i < numBins; ++i)
    {
      fftData[i] /= static_cast<float>(numBins);
      linearMagnitudes[i] = fftData[i];
    }

    // Convert to decibels for visualization
    for (int i = 0; i < numBins; ++i)
    {
      fftData[i] = juce::Decibels::gainToDecibels(fftData[i], negativeInfinity);
    }

    fftDataFifo.push(fftData);
    linearMagnitudesFifo.push(linearMagnitudes);
  }

  void changeOrder(FFTOrder newOrder)
  {
    order = newOrder;
    auto fftSize = getFFTSize();

    forwardFFT = std::make_unique<juce::dsp::FFT>(order);
    window = std::make_unique<juce::dsp::WindowingFunction<float>>(fftSize, juce::dsp::WindowingFunction<float>::blackmanHarris);

    fftData.clear();
    fftData.resize(static_cast<size_t>(fftSize * 2), 0.f);
    linearMagnitudes.resize(static_cast<size_t>(fftSize), 0.f);
    
    fftDataFifo.prepare(fftData.size());
    linearMagnitudesFifo.prepare(linearMagnitudes.size());
  }

  int getFFTSize() const { return 1 << order; }
  int getNumAvailableFFTDataBlocks() const { return fftDataFifo.getNumAvailableForReading(); }

  bool getFFTData(BlockType& returnedData) { return fftDataFifo.pull(returnedData); }
  bool getLinearMagnitudes(std::vector<float>& returnedMagnitudes) { return linearMagnitudesFifo.pull(returnedMagnitudes); }

private:
  FFTOrder order;
  BlockType fftData;
  std::vector<float> linearMagnitudes;
  std::unique_ptr<juce::dsp::FFT> forwardFFT;
  std::unique_ptr<juce::dsp::WindowingFunction<float>> window;
  Fifo<BlockType> fftDataFifo;
  Fifo<std::vector<float>> linearMagnitudesFifo;
};


class FFTProcessor : juce::Thread {
public:
    //TODO: modify the constructor and appropriate initilize the FFT from audioProcessor.
    FFTProcessor(SingleChannelSampleFifo<float>& scsf, ColorDecisionML& colorDecisionML) 
        : juce::Thread("FFTProcessorThread"), monoChannelFifo(&scsf), colorDecisionML(colorDecisionML)
    {
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
    //TODO: Add get methods for colorDecisionML both channels.
private:
    //TODO: Double for both channels all members. colorDecisionML should be unique_ptr.
    SingleChannelSampleFifo<float>* monoChannelFifo;
    FFTDataGenerator<std::vector<float>> monoChannelFFTDataGenerator;
    juce::AudioBuffer<float> monoBuffer;
    ColorDecisionML& colorDecisionML;
    double sampleRate;

};
} // namespace audio_plugin 