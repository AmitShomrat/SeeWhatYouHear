#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "CommonDef.h"
#include <memory>
#include <vector>
#include <array>
#include <atomic>

namespace audio_plugin {

class ColorDecisionML {
public:
    ColorDecisionML(const int fftSize);
    ~ColorDecisionML() = default;

    // Main processing function - now just processes the FIFO data
    void process(std::vector<float>& newFftData, const float newSampleRate);
    
    // Get the current RGB color based on the latest features
    RGB getCurrentRGB() const;

    void setFFTData(const std::vector<float>& newData) { fftData = newData; }
    void setSampleRate(float newSampleRate) { sampleRate = newSampleRate; }
    
    // Thread-safe setters for RGB values
    void setRGB(float r, float g, float b) {
        std::array<float, 3> newRGB{r, g, b};
        currentRGB.store(newRGB);
    }
    
    // Thread-safe getters for raw RGB values
    std::array<float, 3> getRawRGB() const {
        return currentRGB.load();
    }

private:
    struct Features {
        float lowBandEnergy = 0.0f;    // 20Hz - 200Hz
        float midBandEnergy = 0.0f;    // 200Hz - 2kHz
        float highBandEnergy = 0.0f;   // 2kHz - 20kHz
        float spectralCentroid = 0.0f; // Brightness of sound
        float spectralSpread = 0.0f;   // Width of spectrum
        float spectralFlux = 0.0f;     // Rate of spectral change

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

    Features extractFeatures(std::vector<float>& inputFFTData, const float inputSampleRate);
    float calculateBandEnergy(const std::vector<float>& inputData, float lowFreq, float highFreq, float sr);
    float calculateSpectralCentroid(const std::vector<float>& inputData, float sr);
    float calculateSpectralSpread(const std::vector<float>& inputData, float centroid, float sr);
    float calculateSpectralFlux(const std::vector<float>& currentData, const std::vector<float>& prevData);
    float freqToFFTBin(float freq, float sr, int fftSize) const;
    float findPeakFrequency(const std::vector<float>& inputData, float sr);
    
    // Helper function to determine RGB based on features
    RGB determineRGB(const Features& features) const;

    // Helper functions for color mapping
    float normalizeValue(float value, float min, float max) const;
    
    std::vector<float> previousFFTData;
    Features currentFeatures;  // Store the current features
    std::vector<float> fftData;
    float sampleRate{44100.0f};
    
    // Use atomic array for thread-safe RGB values
    std::atomic<std::array<float, 3>> currentRGB{std::array<float, 3>{0.0f, 0.0f, 0.0f}};

    RGB mapFeaturesToRGB(const Features& features) const;
};

} // namespace audio_plugin
