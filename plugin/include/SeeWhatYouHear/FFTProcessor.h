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
    FFTProcessor(SingleChannelSampleFifo<float>& leftScsf, 
                 SingleChannelSampleFifo<float>& rightScsf) 
        : juce::Thread("FFTProcessorThread"), 
          leftChannelFifo(&leftScsf), 
          rightChannelFifo(&rightScsf)
    {
        leftChannelFFTDataGenerator.changeOrder(FFTOrder::order2048);
        rightChannelFFTDataGenerator.changeOrder(FFTOrder::order2048); // Initialize for right channel too
        
        // Assuming FFT sizes are the same for left and right for the input buffer
        fftInputBuffer.setSize(1, leftChannelFFTDataGenerator.getFFTSize()); 

        // Initialize ColorDecisionML instances
        // The FFT size for ColorDecisionML might come from getFFTSize() which is 1 << order
        // For order2048, this is 1 << 11 = 2048.
        // The original ColorDecisionML constructor took 'fftSize' directly, which was linear (e.g. 2048), not the order.
        // Let's use the actual FFT size.
        int currentFFTSize = leftChannelFFTDataGenerator.getFFTSize();
        leftColorDecisionML = std::make_unique<ColorDecisionML>(currentFFTSize);
        rightColorDecisionML = std::make_unique<ColorDecisionML>(currentFFTSize);
        
        startThread();
    }
    ~FFTProcessor(){ stopThread(500); }; // Ensure thread is stopped cleanly
    void prepare(double sampleRateFromProcessor){
        sampleRate = static_cast<float>(sampleRateFromProcessor);
        // Pass sample rate to ColorDecisionML instances if they need it
        if (leftColorDecisionML) leftColorDecisionML->setSampleRate(sampleRate);
        if (rightColorDecisionML) rightColorDecisionML->setSampleRate(sampleRate);
    }
    double getFFTBinWidth() const {
        // Assuming left and right generators have the same FFT size and order
        return sampleRate / static_cast<double>(leftChannelFFTDataGenerator.getFFTSize());
    }
    void process();
    void run() override;
    
    // Adjusted to check left channel, assuming if one has data, we might want to process.
    // Or, a more complex logic might be needed depending on desired behavior.
    bool isChannelAvailable(Channel ch) const { return ch == Channel::Left ? leftChannelFFTDataGenerator.getNumAvailableFFTDataBlocks() > 0 : rightChannelFFTDataGenerator.getNumAvailableFFTDataBlocks() > 0;}

    bool getLatestFFTData(std::vector<float>& fftData, Channel ch) { return ch == Channel::Left ? leftChannelFFTDataGenerator.getFFTData(fftData) : rightChannelFFTDataGenerator.getFFTData(fftData);}
    
    int getFFTSize() const {
        // Assuming left and right generators have the same FFT size
        return leftChannelFFTDataGenerator.getFFTSize();
    }

    // Getter for ColorDecisionML (if needed externally, e.g. by PluginProcessor for editor)
    // This might be an argument against full encapsulation if editor needs direct access.
    // For now, let's assume they are internal. If editor needs RGB, FFTProcessor should provide methods.
    RGB getLeftRGB() const { return leftColorDecisionML ? leftColorDecisionML->getCurrentRGB() : RGB{0,0,0}; }
    RGB getRightRGB() const { return rightColorDecisionML ? rightColorDecisionML->getCurrentRGB() : RGB{0,0,0}; }


private:
    void processChannel(SingleChannelSampleFifo<float>* fifo, 
                        FFTDataGenerator<std::vector<float>>& fftDataGenerator, 
                        ColorDecisionML* colorDecisionMLInstance,
                        juce::AudioBuffer<float>& processingBuffer);

    SingleChannelSampleFifo<float>* leftChannelFifo;
    SingleChannelSampleFifo<float>* rightChannelFifo;
    
    FFTDataGenerator<std::vector<float>> leftChannelFFTDataGenerator;
    FFTDataGenerator<std::vector<float>> rightChannelFFTDataGenerator;
    
    juce::AudioBuffer<float> fftInputBuffer; // Renamed from monoBuffer, still single channel for processing

    std::unique_ptr<ColorDecisionML> leftColorDecisionML;
    std::unique_ptr<ColorDecisionML> rightColorDecisionML;
    
    float sampleRate = 44100.0f; // Default sample rate, to be updated by prepare()
};
} // namespace audio_plugin 