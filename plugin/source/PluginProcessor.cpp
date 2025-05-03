#include "SimpleEQ/PluginProcessor.h"
#include "SimpleEQ/PluginEditor.h"
#include "SimpleEQ/LEDCommunication.h"
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
      ledComm(std::make_shared<LEDCommunication>("COM3")) {
    // Setup debug logging
    #if JUCE_DEBUG
        // Create a log file in the user's documents directory
        auto logFile = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                        .getChildFile("SimpleEQDebug.log");
        
        // Create or clear the log file
        logFile.deleteFile();
        logFile.create();
        
        // Create a FileLogger and make it the current logger
        fileLogger.reset(new juce::FileLogger(logFile, "SimpleEQ Debug Log"));
        juce::Logger::setCurrentLogger(fileLogger.get());
        
        DBG("SimpleEQ Plugin: Constructor Called - Debug Logging Initialized");
    #endif
}

AudioPluginAudioProcessor::~AudioPluginAudioProcessor() {}

const juce::String AudioPluginAudioProcessor::getName() const {
  return JucePlugin_Name;
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
  // leftChain.prepare(spec);
  // rightChain.prepare(spec);

  // updateFilters();

  leftChannelFifo.prepare(samplesPerBlock);
  rightChannelFifo.prepare(samplesPerBlock);

  // osc.initialise([](float x) { return std::sin(x); });
  // spec.numChannels = getTotalNumOutputChannels();
  // osc.prepare(spec);
  // osc.setFrequency(1000);
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
  // channels that didn't contain input data, (because these aren't
  // guaranteed to be empty - they may contain garbage).
  // This is here to avoid people getting screaming feedback
  // when they first compile a plugin, but obviously you don't need to keep
  // this code if your algorithm always overwrites all the output channels.
  for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i) {
    buffer.clear(i, 0, buffer.getNumSamples());
  }

  // Update the channel levels
  if (totalNumInputChannels >= 1)
    leftChannelLevel.set(calculateChannelLevel(buffer, 0));
  if (totalNumInputChannels >= 2)
    rightChannelLevel.set(calculateChannelLevel(buffer, 1));

  // updateFilters();
  updateLEDs(leftChannelLevel.get(), rightChannelLevel.get());

  juce::dsp::AudioBlock<float> block(buffer);
  
  //======================================Check freqs with osc======================================
  // buffer.clear();

  // juce::dsp::ProcessContextReplacing<float> StereoContext(block);
  // osc.process(StereoContext);
  //======================================Check freqs with osc======================================

  auto leftBlock = block.getSingleChannelBlock(0);
  auto rightBlock = block.getSingleChannelBlock(1);

  juce::dsp::ProcessContextReplacing<float> leftContext(leftBlock);
  juce::dsp::ProcessContextReplacing<float> rightContext(rightBlock);

  // leftChain.process(leftContext);
  // rightChain.process(rightContext);

  leftChannelFifo.process(buffer);
  rightChannelFifo.process(buffer);
}

void AudioPluginAudioProcessor::updateLEDs(float leftLevel, float rightLevel) {
    auto leftLevelScaled = juce::jlimit(0.f, 1.f, leftLevel);
    auto skewedleftLevelScale = std::pow(leftLevelScaled, 5.f);
  
    auto rightLevelScaled = juce::jlimit(0.f, 1.f, rightLevel);
    auto skewedrightLevelScale = std::pow(rightLevelScaled, 5.f);

    ledComm -> setBrightness(juce::jlimit(0.f, 1.f, skewedleftLevelScale ) * 255.f, juce::jlimit(0.f, 1.f, skewedrightLevelScale ) * 255.f );
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
      (0.f, 0.3f, 0.01f), 0.f));

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
    
    // Calculate RMS (Root Mean Square) level
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        float sample = channelData[i];
        sum += sample * sample;
    }
    
    float rms = std::sqrt(sum / buffer.getNumSamples());
    
    // Convert to decibels and normalize to 0.0-1.0 range for visualization
    // -60dB to 0dB mapped to 0.0 to 1.0
    float db = juce::Decibels::gainToDecibels(rms, -60.0f);
    return juce::jlimit(0.0f, 1.0f, (db + 60.0f) / 60.0f);
}

}  // namespace audio_plugin

// This creates new instances of the plugin.
// This function definition must be in the global namespace.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
  return new audio_plugin::AudioPluginAudioProcessor();
}
