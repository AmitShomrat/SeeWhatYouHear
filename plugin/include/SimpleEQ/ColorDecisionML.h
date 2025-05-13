#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "CommonDef.h"
#include "FFTProcessor.h"
#include <memory>
#include <vector>

namespace audio_plugin {

class ColorDecisionML {
public:
    ColorDecisionML(SingleChannelSampleFifo<float>& fifo);
    ~ColorDecisionML() = default;

    // Main processing function - now just processes the FIFO data
    void process(float sampleRate);

private:
    struct Features {
        float lowBandEnergy;    // 20Hz - 200Hz
        float midBandEnergy;    // 200Hz - 2kHz
        float highBandEnergy;   // 2kHz - 20kHz
        float spectralCentroid; // Brightness of sound
        float spectralSpread;   // Width of spectrum
        float spectralFlux;     // Rate of spectral change

        // Helper function to print features
        juce::String toString() const {
            return juce::String("Features:\n") +
                   "Low Band Energy: " + juce::String(lowBandEnergy) + "\n" +
                   "Mid Band Energy: " + juce::String(midBandEnergy) + "\n" +
                   "High Band Energy: " + juce::String(highBandEnergy) + "\n" +
                   "Spectral Centroid: " + juce::String(spectralCentroid) + "\n" +
                   "Spectral Spread: " + juce::String(spectralSpread) + "\n" +
                   "Spectral Flux: " + juce::String(spectralFlux);
        }
    };

    // Feature extraction methods
    Features extractFeatures(const FFTProcessor::FFTData& fftData);
    float calculateBandEnergy(const std::vector<float>& fftData, float lowFreq, float highFreq, float sampleRate);
    float calculateSpectralCentroid(const std::vector<float>& fftData, float sampleRate);
    float calculateSpectralSpread(const std::vector<float>& fftData, float centroid, float sampleRate);
    float calculateSpectralFlux(const std::vector<float>& currentFFT, const std::vector<float>& previousFFT);
    float freqToFFTBin(float freq, float sampleRate, int fftSize) const;

    // FFT processing members
    SingleChannelSampleFifo<float>& channelFifo;
    FFTProcessor fftProcessor;
    std::vector<float> previousFFTData;
};

} // namespace audio_plugin
