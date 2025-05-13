#include "SimpleEQ/FFTProcessor.h"
namespace audio_plugin {
void FFTProcessor::run() {
    while (!threadShouldExit()) {
        process();
        // Reduce update rate to 30Hz (33ms) to lower CPU usage
        wait(16);
    }
}

void FFTProcessor::process() {
    if (threadShouldExit()) return;
    
    juce::AudioBuffer<float> tempIncomingBuffer;
    while (monoChannelFifo->getNumCompleteBuffersAvailable() > 0) {
        if (monoChannelFifo->getAudioBuffer(tempIncomingBuffer)) {
            auto size = tempIncomingBuffer.getNumSamples();
            
            // Use a more efficient way to update the buffer
            const float* sourceData = tempIncomingBuffer.getReadPointer(0);
            float* destData = monoBuffer.getWritePointer(0);
            
            // Shift existing samples
            std::memmove(destData, 
                        destData + size, 
                        (monoBuffer.getNumSamples() - size) * sizeof(float));
            
            // Copy new samples
            std::memcpy(destData + (monoBuffer.getNumSamples() - size),
                       sourceData,
                       size * sizeof(float));
                       
            monoChannelFFTDataGenerator.produceFFTDataForRendering(monoBuffer, -48.f);
        }
    }

    // Color decision ML calculations using linear magnitudes
    std::vector<float> linearMagnitudes;
    if (monoChannelFFTDataGenerator.getLinearMagnitudes(linearMagnitudes)) {
        colorDecisionML.process(linearMagnitudes, static_cast<float>(sampleRate));
    }
}
} // namespace audio_plugin 