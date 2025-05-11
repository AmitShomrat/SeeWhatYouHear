// ColorDecisionML.cpp
#include "SimpleEQ/ColorDecisionML.h"

namespace audio_plugin {

ColorDecisionML::ColorDecisionML(SingleChannelSampleFifo<float>& fifo)
    : channelFifo(fifo)
    , fftProcessor(11)  // 2048 points
{
    DBG("ColorDecisionML constructor called");
    previousFFTData.resize(fftProcessor.getFFTSize(), 0.0f);
}

void ColorDecisionML::process(float sampleRate) {
    fftProcessor.processNextBuffer(&channelFifo, 
        [this](const FFTProcessor::FFTData& fftData) {
            // Extract features and log them
            Features features = extractFeatures(fftData);
            DBG(features.toString());

            // Store current FFT data for next flux calculation
            previousFFTData = fftData.fftData;
        },
        sampleRate
    );
}

ColorDecisionML::Features ColorDecisionML::extractFeatures(const FFTProcessor::FFTData& fftData) {
    Features features;
    
    features.lowBandEnergy = calculateBandEnergy(fftData.fftData, 20.0f, 200.0f, fftData.sampleRate);
    features.midBandEnergy = calculateBandEnergy(fftData.fftData, 200.0f, 2000.0f, fftData.sampleRate);
    features.highBandEnergy = calculateBandEnergy(fftData.fftData, 2000.0f, 20000.0f, fftData.sampleRate);
    features.spectralCentroid = calculateSpectralCentroid(fftData.fftData, fftData.sampleRate);
    features.spectralSpread = calculateSpectralSpread(fftData.fftData, features.spectralCentroid, fftData.sampleRate);
    features.spectralFlux = calculateSpectralFlux(fftData.fftData, previousFFTData);
    
    return features;
}

float ColorDecisionML::calculateBandEnergy(const std::vector<float>& fftData, 
                                         float lowFreq, 
                                         float highFreq,
                                         float sampleRate) {
    float energy = 0.0f;
    int lowBin = static_cast<int>(freqToFFTBin(lowFreq, sampleRate, static_cast<int>(fftData.size())));
    int highBin = static_cast<int>(freqToFFTBin(highFreq, sampleRate, static_cast<int>(fftData.size())));
    
    for (int bin = lowBin; bin <= highBin && bin < static_cast<int>(fftData.size()/2); ++bin) {
        energy += fftData[bin] * fftData[bin];
    }
    
    return std::sqrt(energy) / (highBin - lowBin + 1);
}

float ColorDecisionML::calculateSpectralCentroid(const std::vector<float>& fftData,
                                               float sampleRate) {
    float numerator = 0.0f;
    float denominator = 0.0f;
    
    for (size_t bin = 0; bin < fftData.size()/2; ++bin) {
        float magnitude = fftData[bin];
        float frequency = bin * sampleRate / fftData.size();
        numerator += frequency * magnitude;
        denominator += magnitude;
    }
    
    return denominator > 0.0f ? numerator / denominator : 0.0f;
}

float ColorDecisionML::calculateSpectralSpread(const std::vector<float>& fftData, 
                                             float centroid,
                                             float sampleRate) {
    float numerator = 0.0f;
    float denominator = 0.0f;
    
    for (size_t bin = 0; bin < fftData.size()/2; ++bin) {
        float magnitude = fftData[bin];
        float frequency = bin * sampleRate / fftData.size();
        float diff = frequency - centroid;
        numerator += diff * diff * magnitude;
        denominator += magnitude;
    }
    
    return denominator > 0.0f ? std::sqrt(numerator / denominator) : 0.0f;
}

float ColorDecisionML::calculateSpectralFlux(const std::vector<float>& currentFFT,
                                           const std::vector<float>& previousFFT) {
    float flux = 0.0f;
    
    for (size_t bin = 0; bin < currentFFT.size()/2 && bin < previousFFT.size()/2; ++bin) {
        float diff = currentFFT[bin] - previousFFT[bin];
        flux += diff * diff;
    }
    
    return std::sqrt(flux);
}

float ColorDecisionML::freqToFFTBin(float freq, float sampleRate, int fftSize) const {
    return freq * fftSize / sampleRate;
}
} // namespace audio_plugin