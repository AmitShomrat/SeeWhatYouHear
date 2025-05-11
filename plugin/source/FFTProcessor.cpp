#include "SimpleEQ/FFTProcessor.h"
namespace audio_plugin {
void FFTProcessor::run() {
    while (!threadShouldExit()) {
        process();

        wait(16); //equivalent to 60Hz.
    }
}

void FFTProcessor::process() {
    juce::AudioBuffer<float> tempIncomingBuffer;
    while (monoChannelFifo->getNumCompleteBuffersAvailable() > 0) {
        if (monoChannelFifo->getAudioBuffer(tempIncomingBuffer)) {
            auto size = tempIncomingBuffer.getNumSamples();
            //shift buffer to the left.
            juce::FloatVectorOperations::copy(monoBuffer.getWritePointer(0, 0),
                                              monoBuffer.getReadPointer(0, size),
                                              monoBuffer.getNumSamples() - size);
            //copy the new incoming buffer to the right.
            juce::FloatVectorOperations::copy(monoBuffer.getWritePointer(0, monoBuffer.getNumSamples() - size),
                                              tempIncomingBuffer.getReadPointer(0, 0),
                                              size);
            monoChannelFFTDataGenerator.produceFFTDataForRendering(monoBuffer, -48.f);
        }
    }
}
} // namespace audio_plugin 