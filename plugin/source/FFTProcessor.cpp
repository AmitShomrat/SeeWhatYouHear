#include "SeeWhatYouHear/FFTProcessor.h"
namespace audio_plugin {
void FFTProcessor::run() {
    while (!threadShouldExit()) {
        process();
        // Reduce update rate to 30Hz (33ms) to lower CPU usage
        wait(16);
    }
}


// Helper function to process a single channel
void FFTProcessor::processChannel(SingleChannelSampleFifo<float>* fifo, 
                                  FFTDataGenerator<std::vector<float>>& fftDataGenerator, 
                                  ColorDecisionML* colorDecisionMLInstance,
                                  juce::AudioBuffer<float>& processingBuffer) // This is fftInputBuffer
{
    if (!fifo) return;

    juce::AudioBuffer<float> tempIncomingBuffer;
    while (fifo->getNumCompleteBuffersAvailable() > 0) {
        if (fifo->getAudioBuffer(tempIncomingBuffer)) {
            auto size = tempIncomingBuffer.getNumSamples();
            
            // Ensure processingBuffer is large enough (should be sized in constructor)
            // This assertion can help catch issues if fftInputBuffer wasn't sized correctly.
            jassert(processingBuffer.getNumSamples() >= size);

            const float* sourceData = tempIncomingBuffer.getReadPointer(0);
            float* destData = processingBuffer.getWritePointer(0);
            
            // Shift existing samples
            std::memmove(destData, 
                         destData + size, 
                         (processingBuffer.getNumSamples() - size) * sizeof(float));
            
            // Copy new samples
            std::memcpy(destData + (processingBuffer.getNumSamples() - size),
                        sourceData,
                        size * sizeof(float));
           
            fftDataGenerator.produceFFTDataForRendering(processingBuffer, -48.f);

            if (colorDecisionMLInstance) {
                std::vector<float> linearMagnitudes;
                // Assuming getLinearMagnitudes is part of FFTDataGenerator
                if (fftDataGenerator.getLinearMagnitudes(linearMagnitudes)) {
                    colorDecisionMLInstance->process(linearMagnitudes);
                }
            }
        }
    }
}

void FFTProcessor::process() {
    if (threadShouldExit()) return;

    // Process Left Channel
    processChannel(leftChannelFifo, 
                   leftChannelFFTDataGenerator, 
                   leftColorDecisionML.get(), // .get() for unique_ptr
                   fftInputBuffer);

    // Process Right Channel
    processChannel(rightChannelFifo, 
                   rightChannelFFTDataGenerator, 
                   rightColorDecisionML.get(), // .get() for unique_ptr
                   fftInputBuffer); // Reuse the same input buffer sequentially
}
} // namespace audio_plugin 