#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>

namespace audio_plugin {

enum Color { 
  Red,
  Blue,
  Green,
  Yellow
};
struct RGB {
  int r {0};
  int g {0};
  int b {0};

  bool operator==(const RGB& other) const {
    return r == other.r && g == other.g && b == other.b;
  }

  bool operator!=(const RGB& other) const {
    return !(*this == other);
  }
};

template <typename T>
struct Fifo
{
  void prepare(int numChannels, int numSamples)
  {
    static_assert(std::is_same_v<T, juce::AudioBuffer<float>>, "prepare(numChannels, numSamples) should only be used with juce::AudioBuffer<float>");
    for (auto& buffer : buffers)
    {
      buffer.setSize(numChannels,
                     numSamples,
                     false,  //clear everything?
                     true,   //including the extra space? 
                     true);  //avoid reallocating if you can?
      buffer.clear();
    }
  }

  void prepare(size_t numElements)
  {
    static_assert(std::is_same_v<T, std::vector<float>>, "prepare(numElements) should only be used with std::vector<float>");

    for (auto& buffer : buffers)
    {
      buffer.clear();
      buffer.resize(numElements, 0);
    }
  }

  bool push(const T& t)
  {
    auto write = fifo.write(1);
    if (write.blockSize1 > 0)
    {
      buffers[write.startIndex1] = t;
      return true;
    }
    return false;
  }

  bool pull(T& t)
  {
    auto read = fifo.read(1);
    if (read.blockSize1 > 0)
    {
      t = buffers[read.startIndex1];
      return true;
    }
    return false;
  }
  
  int getNumAvailableForReading() const 
  {
    return fifo.getNumReady();
  }
  
  private:
    static constexpr int Capacity = 30;
    std::array<T, Capacity> buffers;
    juce::AbstractFifo fifo {Capacity};
};

enum Channel
{
  Right, //effectively 0
  Left //effectively 1
};  

template <typename Type>
struct SingleChannelSampleFifo
{
  SingleChannelSampleFifo(Channel ch) : channelToUse(ch)
  {
    prepared.set(false);
  }

  void process(const juce::AudioBuffer<Type>& buffer)
  {
    jassert(prepared.get());
    jassert(buffer.getNumChannels() > channelToUse);
    auto* channelPtr = buffer.getReadPointer(channelToUse);

    for(int i = 0; i < buffer.getNumSamples(); ++i)
    {
      pushNextSampleIntoFifo(channelPtr[i]);
    }
  }
  
  void prepare(int bufferSize)
  {
    prepared.set(false);
    size.set(bufferSize);

    bufferToFill.setSize(1, 
                         bufferSize,
                         false, 
                         true, 
                         true);
    audioBufferFifo.prepare(1, bufferSize);
    fifoIndex = 0;
    prepared.set(true);
  }
  Channel getChannel() const { return channelToUse; }
  int getNumCompleteBuffersAvailable() const { return audioBufferFifo.getNumAvailableForReading(); }
  bool isPrepared() const { return prepared.get(); }
  int getSize() const { return size.get(); }
  bool getAudioBuffer(juce::AudioBuffer<Type>& buf) { return audioBufferFifo.pull(buf); }
  
private:
  Channel channelToUse;
  int fifoIndex = 0;
  Fifo<juce::AudioBuffer<Type>> audioBufferFifo;
  juce::AudioBuffer<Type> bufferToFill;
  juce::Atomic<bool> prepared {false};
  juce::Atomic<int> size = 0;
 
  void pushNextSampleIntoFifo(Type sample) 
  {
    if(fifoIndex == bufferToFill.getNumSamples())
    {
      auto ok = audioBufferFifo.push(bufferToFill);
      juce::ignoreUnused(ok);
      fifoIndex = 0;
    }
    bufferToFill.setSample(0, fifoIndex, sample);
    ++fifoIndex;
  }  
};

struct BrightnessDecision
{
    juce::Atomic<float> getLeftBrightness() const { return leftBrightness.get(); }
    juce::Atomic<float> getRightBrightness() const { return rightBrightness.get(); }
  
    void computeBrightness(float leftLevel, float rightLevel, float userBrightness){
            // Apply smoothing to levels
      const float smoothingCoeff = 0.9f; //Higher = Fast response, Lower = Slow response.
      leftBrightness.set(leftBrightness.get() + smoothingCoeff * (leftLevel - leftBrightness.get()));
      rightBrightness.set(rightBrightness.get() + smoothingCoeff * (rightLevel - rightBrightness.get()));

      // Only bypass step 0
      float userBrightnessScale = 0.0f;
      if (userBrightness >= 0.015f) {  // Start from step 1
          // Rescale to ensure step 1 is visible
          userBrightnessScale = juce::jmap(userBrightness, 
                                    0.015f, 1.0f,  // Input range: from step 1 to max
                                    0.2f, 1.0f);     // Output range: start at 20% brightness
      }

      // Apply brightness scaling to smoothed levels
      leftBrightness.set(leftBrightness.get() * userBrightnessScale);
      rightBrightness.set(rightBrightness.get() * userBrightnessScale);
      // std::cout << "leftBrightness: " << leftBrightness.get() << " rightBrightness: " << rightBrightness.get() << std::endl;
    }
    
  private:
    // The returned values.
    juce::Atomic<float> leftBrightness{0.0f};
    juce::Atomic<float> rightBrightness{0.0f};
};

} // namespace audio_plugin