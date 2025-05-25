// ColorDecisionML.cpp
#include "SeeWhatYouHear/ColorDecisionML.h"

namespace audio_plugin {

ColorDecisionML::ColorDecisionML(const int fftSize)
{
    DBG("ColorDecisionML constructor called");
    previousFFTData.resize(fftSize, 0.0f);
    fftData.resize(fftSize, 0.0f);
}

void ColorDecisionML::process(std::vector<float>& newFftData, const float newSampleRate) {
    fftData = newFftData;
    sampleRate = newSampleRate;
    currentFeatures = extractFeatures(fftData, sampleRate);
    
    // Get new RGB values from feature mapping
    RGB newRGB = mapFeaturesToRGB(currentFeatures);
    
    // Convert to normalized float values (0-1)
    float r = newRGB.r / 255.0f;
    float g = newRGB.g / 255.0f;
    float b = newRGB.b / 255.0f;

    // Get current RGB values using atomic load
    auto prevRGB = currentRGB.load();

    // Apply temporal smoothing
    const float smoothingFactor = 0.3f;
    float smoothedR = prevRGB[0] * (1.0f - smoothingFactor) + r * smoothingFactor;
    float smoothedG = prevRGB[1] * (1.0f - smoothingFactor) + g * smoothingFactor;
    float smoothedB = prevRGB[2] * (1.0f - smoothingFactor) + b * smoothingFactor;

    // Update RGB values using atomic store
    setRGB(smoothedR, smoothedG, smoothedB);

    // Store current FFT data for next flux calculation
    previousFFTData = fftData;
}

RGB ColorDecisionML::getCurrentRGB() const {
    // Get current RGB values using atomic load
    auto rgbValues = currentRGB.load();
    
    // Convert normalized float RGB (0-1) to integer RGB (0-255)
    RGB rgb;
    rgb.r = static_cast<int>(rgbValues[0] * 255.0f);
    rgb.g = static_cast<int>(rgbValues[1] * 255.0f);
    rgb.b = static_cast<int>(rgbValues[2] * 255.0f);
    
    // Ensure values are within valid range
    rgb.r = juce::jlimit(0, 255, rgb.r);
    rgb.g = juce::jlimit(0, 255, rgb.g);
    rgb.b = juce::jlimit(0, 255, rgb.b);
    
    return rgb;
}

float ColorDecisionML::findPeakFrequency(const std::vector<float>& inputData, float inputSampleRate) {
    int maxBin = 0;
    float maxMagnitude = 0.0f;
    
    // Only look at the meaningful part of the spectrum (up to Nyquist frequency)
    for (int i = 0; i < inputData.size() / 2; ++i) {
        if (inputData[i] > maxMagnitude) {
            maxMagnitude = inputData[i];
            maxBin = i;
        }
    }
    
    return (maxBin * inputSampleRate) / (inputData.size());
}

float ColorDecisionML::calculateBandEnergy(const std::vector<float>& inputData, 
                                         float lowFreq, 
                                         float highFreq,
                                         float inputSampleRate) {
    float energy = 0.0f;
    int lowBin = static_cast<int>(freqToFFTBin(lowFreq, inputSampleRate, static_cast<int>(inputData.size())));
    int highBin = static_cast<int>(freqToFFTBin(highFreq, inputSampleRate, static_cast<int>(inputData.size())));
    
    // Ensure bins are within valid range
    lowBin = std::max(0, std::min(lowBin, static_cast<int>(inputData.size())));
    highBin = std::max(0, std::min(highBin, static_cast<int>(inputData.size())));
    
    // Calculate energy using linear magnitudes (already squared)
    for (int bin = lowBin; bin <= highBin && bin < static_cast<int>(inputData.size()); ++bin) {
        energy += inputData[bin];
    }
    
    // Normalize by number of bins to get average energy in band (Low range has less bins then it leads to higher energy in low range)
    // int numBins = highBin - lowBin + 1;
    // if (numBins > 0) {
    //     energy /= numBins;
    // }
    
    return energy;
}

float ColorDecisionML::calculateSpectralCentroid(const std::vector<float>& inputData,
                                               float inputSampleRate) {
    float numerator = 0.0f;
    float denominator = 0.0f;
    
    for (size_t bin = 0; bin < inputData.size()/2; ++bin) {
        float magnitude = inputData[bin];
        float frequency = bin * inputSampleRate / inputData.size();
        numerator += frequency * magnitude;
        denominator += magnitude;
    }
    
    return denominator > 0.0f ? numerator / denominator : 0.0f;
}

float ColorDecisionML::calculateSpectralSpread(const std::vector<float>& inputData, 
                                             float centroid,
                                             float inputSampleRate) {
    float numerator = 0.0f;
    float denominator = 0.0f;
    
    for (size_t bin = 0; bin < inputData.size()/2; ++bin) {
        float magnitude = inputData[bin];
        float frequency = bin * inputSampleRate / inputData.size();
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

float ColorDecisionML::freqToFFTBin(float freq, float inputSampleRate, int fftSize) const {
    return freq * fftSize / inputSampleRate;
}

ColorDecisionML::Features ColorDecisionML::extractFeatures(std::vector<float>& inputFFTData, const float inputSampleRate) {
    Features features;
    
    features.lowBandEnergy = calculateBandEnergy(inputFFTData, 20.0f, 200.0f, inputSampleRate);
    features.midBandEnergy = calculateBandEnergy(inputFFTData, 200.0f, 2000.0f, inputSampleRate);
    features.highBandEnergy = calculateBandEnergy(inputFFTData, 2000.0f, 10000.0f, inputSampleRate);
    features.spectralCentroid = calculateSpectralCentroid(inputFFTData, inputSampleRate);
    features.spectralSpread = calculateSpectralSpread(inputFFTData, features.spectralCentroid, inputSampleRate);
    features.spectralFlux = calculateSpectralFlux(inputFFTData, previousFFTData);
    // std::cout << "features: " << features.lowBandEnergy << " " << features.midBandEnergy << " " << features.highBandEnergy << std::endl;
    return features;
}

RGB ColorDecisionML::mapFeaturesToRGB(const Features& features) const {
    RGB rgb{0, 0, 0}; // Initialize with black
    
    // Calculate total energy across all bands
    float totalEnergy = features.lowBandEnergy + features.midBandEnergy + features.highBandEnergy;
    
    // Only calculate colors if there's significant energy
    const float energyThreshold = 0.001f; // Increased threshold for better low-level behavior
    if (totalEnergy > energyThreshold) {
        // Normalize band energies relative to total energy
        float normalizedLow = features.lowBandEnergy / totalEnergy;
        float normalizedMid = features.midBandEnergy / totalEnergy;
        float normalizedHigh = features.highBandEnergy / totalEnergy;
        
        // Apply non-linear scaling to enhance color response
        float normalizedFlux = normalizeValue(features.spectralFlux, 0.0f, 1.0f);
        float normalizedCentroid = normalizeValue(features.spectralCentroid, 20.0f, 20000.0f);
        float normalizedSpread = normalizeValue(features.spectralSpread, 0.0f, 5000.0f);
        
        // Scale the energy relative to the threshold for smoother fade to black
        float energyScale = juce::jmap(totalEnergy, energyThreshold, energyThreshold * 10.0f, 0.0f, 1.0f);
        energyScale = juce::jlimit(0.0f, 1.0f, energyScale);
        
        // Calculate RGB components with enhanced weighting
        rgb.r = static_cast<int>(255 * energyScale * (normalizedLow + normalizedFlux));
        rgb.g = static_cast<int>(255 * energyScale * (normalizedMid * 0.5f + normalizedCentroid));
        rgb.b = static_cast<int>(255 * energyScale * (normalizedHigh * 0.5f + normalizedSpread));
        
        // Ensure values are within valid range
        rgb.r = juce::jlimit(0, 255, rgb.r);
        rgb.g = juce::jlimit(0, 255, rgb.g);
        rgb.b = juce::jlimit(0, 255, rgb.b);
    }
    
    return rgb;
}

float ColorDecisionML::normalizeValue(float value, float min, float max) const {
    if (max == min) return 0.0f;
    float normalized = (value - min) / (max - min);
    return juce::jlimit(0.0f, 1.0f, normalized);
}

} // namespace audio_plugin