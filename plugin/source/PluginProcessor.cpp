#include "SeeWhatYouHear/PluginProcessor.h"
#include "SeeWhatYouHear/PluginEditor.h"
#include "SeeWhatYouHear/LEDCommunication.h"
#include <juce_dsp/juce_dsp.h>

namespace audio_plugin {
AudioPluginAudioProcessor::AudioPluginAudioProcessor()
    : AudioProcessor(
          BusesProperties()
#if !JucePlugin_IsMidiEffect
#if !JucePlugin_IsSynth
              .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
              .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
      ),
      leftColorDecisionML(1 << order2048),
      rightColorDecisionML(1 << order2048),
      ledComm(std::make_shared<audio_plugin::LEDCommunication>("COM3", leftColorDecisionML, rightColorDecisionML)),
      leftChannelFFTProcessor(std::make_shared<FFTProcessor>(leftChannelFifo, leftColorDecisionML)),
      rightChannelFFTProcessor(std::make_shared<FFTProcessor>(rightChannelFifo, rightColorDecisionML)) {
    // Setup debug logging
    #if JUCE_DEBUG
        // Create a log file in the user's documents directory
        auto logFile = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                        .getChildFile("SeeWhatYouHearDebug.log");
        
        // Create or clear the log file
        logFile.deleteFile();
        logFile.create();

        // Create a FileLogger and make it the current logger
        fileLogger.reset(new juce::FileLogger(logFile, "SeeWhatYouHear Debug Log"));
        juce::Logger::setCurrentLogger(fileLogger.get());
        
        DBG("SeeWhatYouHear Plugin: Constructor Called - Debug Logging Initialized");
    #endif
}

AudioPluginAudioProcessor::~AudioPluginAudioProcessor() {}

const juce::String AudioPluginAudioProcessor::getName() const {
  return "SeeWhatYouHear";
}

bool AudioPluginAudioProcessor::acceptsMidi() const {
#if JucePlugin_WantsMidiInput
  return true;
#else
  return false;
#endif
}

bool AudioPluginAudioProcessor::producesMidi() const {
#if JucePlugin_ProducesMidiOutput
  return true;
#else
  return false;
#endif
}

bool AudioPluginAudioProcessor::isMidiEffect() const {
#if JucePlugin_IsMidiEffect
  return true;
#else
  return false;
#endif
}

double AudioPluginAudioProcessor::getTailLengthSeconds() const {
  return 0.0;
}

int AudioPluginAudioProcessor::getNumPrograms() {
  return 1;  // NB: some hosts don't cope very well if you tell them there are 0
             // programs, so this should be at least 1, even if you're not
             // really implementing programs.
}

int AudioPluginAudioProcessor::getCurrentProgram() {
  return 0;
}

void AudioPluginAudioProcessor::setCurrentProgram(int index) {
  juce::ignoreUnused(index);
}

const juce::String AudioPluginAudioProcessor::getProgramName(int index) {
  juce::ignoreUnused(index);
  return {};
}

void AudioPluginAudioProcessor::changeProgramName(int index, const juce::String& newName) {
  juce::ignoreUnused(index, newName);
}

void AudioPluginAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
  // Use this method as the place to do any pre-playback
  // initialisation that you need..
  
  // Store the sample rate
  mySampleRate = sampleRate;

  juce::dsp::ProcessSpec spec;
  spec.maximumBlockSize = samplesPerBlock;
  spec.numChannels = 1;
  spec.sampleRate = sampleRate;

  leftChannelFifo.prepare(samplesPerBlock);
  rightChannelFifo.prepare(samplesPerBlock);

  leftChannelFFTProcessor->prepare(sampleRate);
  rightChannelFFTProcessor->prepare(sampleRate);

  // Initialize oscillators with 0.5 amplitude (multiply sin(x) by 0.5)
  leftOsc.initialise([](float x) { return std::sin(x); }, 128);
  rightOsc.initialise([](float x) { return std::sin(x); }, 128);
  
  spec.numChannels = 1; // Set to 1 for mono processing
  leftOsc.prepare(spec);
  rightOsc.prepare(spec);
  
  leftOsc.setFrequency(freq);  // Set left to base frequency
  rightOsc.setFrequency(freq);  // Set right slightly higher for stereo width
}

void AudioPluginAudioProcessor::releaseResources() {
  // When playback stops, you can use this as an opportunity to free up any
  // spare memory, etc.
}


bool AudioPluginAudioProcessor::isBusesLayoutSupported(
    const BusesLayout& layouts) const {
#if JucePlugin_IsMidiEffect
  juce::ignoreUnused(layouts);
  return true;
#else
  // This is the place where you check if the layout is supported.
  // In this template code we only support mono or stereo.
  // Some plugin hosts, such as certain GarageBand versions, will only
  // load plugins that support stereo bus layouts.
  if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono() &&
      layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
    return false;

    // This checks if the input layout matches the output layout
#if !JucePlugin_IsSynth
  if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
    return false;
#endif

  return true;
#endif
}

void AudioPluginAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    juce::ignoreUnused(midiMessages);

    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, we'll clear any output
    // channels that didn't contain input data
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i) {
        buffer.clear(i, 0, buffer.getNumSamples());
    }

    // juce::dsp::AudioBlock<float> block(buffer);

    // ======================================Check freqs with osc======================================
    // buffer.clear();
    
    // // Process each channel independently
    // auto leftBlock = block.getSingleChannelBlock(0);
    // auto rightBlock = block.getSingleChannelBlock(1);

    // juce::dsp::ProcessContextReplacing<float> leftContext(leftBlock);
    // juce::dsp::ProcessContextReplacing<float> rightContext(rightBlock);

    // leftOsc.process(leftContext);
    // rightOsc.process(rightContext);

    // // Update frequency if needed
    // float newFreq = JUCE_LIVE_CONSTANT(60.0f);
    // if (freq != newFreq) {
    //     freq = newFreq;
    //     leftOsc.setFrequency(freq);
    //     rightOsc.setFrequency(freq /* 1.01f*/);
    // }

    // ======================================Check freqs with osc======================================

    // Update the channel levels
    if (totalNumInputChannels >= 1)
        leftChannelLevel.set(calculateChannelLevel(buffer, 0));
    if (totalNumInputChannels >= 2)
        rightChannelLevel.set(calculateChannelLevel(buffer, 1));

    // Update LED communication with current levels
    if (ledComm) {
        ledComm->setBrightness(leftChannelLevel.get(), rightChannelLevel.get(), apvts.getRawParameterValue("Brightness")->load());
    }

    brightnessDecision.computeBrightness(leftChannelLevel.get(), rightChannelLevel.get(), apvts.getRawParameterValue("Brightness")->load());

    leftChannelFifo.process(buffer);
    rightChannelFifo.process(buffer);
}

void AudioPluginAudioProcessor::updateLEDs(float leftLevel, float rightLevel) {
    if (ledComm) {
        ledComm->setBrightness(leftLevel, rightLevel, apvts.getRawParameterValue("Brightness")->load());
    }
}

bool AudioPluginAudioProcessor::hasEditor() const {
  return true;  // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* AudioPluginAudioProcessor::createEditor() {
  return new AudioPluginAudioProcessorEditor(*this);
}

void AudioPluginAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
  // You should use this method to store your parameters in the memory block.
  // You could do that either as raw data, or use the XML or ValueTree classes
  // as intermediaries to make it easy to save and load complex data.
  juce::MemoryOutputStream mos(destData, true);
  apvts.state.writeToStream(mos);
  // juce::ignoreUnused(destData);
}

void AudioPluginAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    auto tree = juce::ValueTree::readFromData(data, sizeInBytes);
    if (tree.isValid())
    {
        apvts.replaceState(tree);
    }
}


juce::AudioProcessorValueTreeState::ParameterLayout
AudioPluginAudioProcessor::createParameterLayout() {
  juce::AudioProcessorValueTreeState::ParameterLayout layout;


  layout.add(std::make_unique<juce::AudioParameterFloat>(
      "Brightness", "Brightness", juce::NormalisableRange<float>
      (0.f, 0.135f, 0.019f, 0.5f), 0.135f));

  juce::Array<juce::String> stringArray = {"red", "blue", "green", "yellow"};
  layout.add(std::make_unique<juce::AudioParameterChoice>(
      "Color", "Color", stringArray, 3));

  return layout;
}

float AudioPluginAudioProcessor::calculateChannelLevel(const juce::AudioBuffer<float>& buffer, int channel)
{
    if (channel >= buffer.getNumChannels())
        return 0.0f;
        
    auto* channelData = buffer.getReadPointer(channel);
    float sum = 0.0f;
    float peakLevel = 0.0f;
    
    // Calculate both RMS and peak levels
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        float sample = std::abs(channelData[i]);
        sum += sample * sample;
        peakLevel = std::max(peakLevel, sample);
    }
    
    float rms = std::sqrt(sum / buffer.getNumSamples());
    
    // For a sine wave at 0dB (peak = 1.0), RMS is approximately 0.707 (1/√2)
    // We'll normalize RMS relative to this value
    float normalizedRMS = rms / 0.707f;
    
    // Use the maximum of normalized RMS and peak for the final level
    float level = std::max(normalizedRMS, peakLevel);

    // Ensure we don't exceed 1.0
    // std::cout << "level: " << juce::jlimit(0.0f, 1.0f, level) << std::endl;
    return juce::jlimit(0.0f, 1.0f, level);
}

}  // namespace audio_plugin

// This creates new instances of the plugin.
// This function definition must be in the global namespace.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
  return new audio_plugin::AudioPluginAudioProcessor();
}
