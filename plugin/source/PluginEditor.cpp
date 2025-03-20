#include "SimpleEQ/PluginEditor.h"
#include "SimpleEQ/PluginProcessor.h"

namespace audio_plugin {
AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor( AudioPluginAudioProcessor& p )
 : AudioProcessorEditor(&p), processorRef(p),
peakFreqSliderAttachment(processorRef.apvts, "Peak Freq", peakFreqSlider),
peakGainSliderAttachment(processorRef.apvts, "Peak Gain", peakGainSlider),
peakQualitySliderAttachment(processorRef.apvts, "Peak Quality", peakQualitySlider),
lowCutFreqSliderAttachment(processorRef.apvts, "LowCut Freq", lowCutFreqSlider),
highCutFreqSliderAttachment(processorRef.apvts, "HighCut Freq", highCutFreqSlider),
lowCutSlopeSliderAttachment(processorRef.apvts, "LowCut Slope", lowCutSlopeSlider),
highCutSlopeSliderAttachment(processorRef.apvts, "HighCut Slope", highCutSlopeSlider)
{
  juce::ignoreUnused(processorRef);

  for(auto* comp : getComps()) {
    this -> addAndMakeVisible(comp);
  }

  const auto& params = processorRef.getParameters();
  for(auto param : params) {
    param -> addListener(this);
  }

  startTimerHz(60);

  // Make sure that before the constructor has finished, you've set the
  // editor's size to whatever you need it to be.
  setSize(600, 400);
}

AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor() 
{
  const auto& params = processorRef.getParameters();
  for(auto param : params) {
    param -> removeListener(this);
  }
}

void AudioPluginAudioProcessorEditor::paint(juce::Graphics& g) {
  using namespace juce;
  g.fillAll(Colours::black);

  auto bounds = getLocalBounds();
  auto responseArea = bounds.removeFromTop(static_cast<int>(bounds.getHeight() * 0.33));

  auto w = responseArea.getWidth();

  auto& lowCut = monoChain.get<ChainPositions::LowCut>();
  auto& highCut = monoChain.get<ChainPositions::HighCut>();
  auto& peak = monoChain.get<ChainPositions::Peak>();

  auto sampleRate = processorRef.getSampleRate();

  std::vector<double> mags;
  mags.resize(w);
  for(int i = 0; i < w; ++i) {
    double mag = 1.f;
    auto freq = mapToLog10(double(i) / double(w), 20.0, 20000.0);

    if(! monoChain.isBypassed<ChainPositions::Peak>()) 
      mag *= peak.coefficients->getMagnitudeForFrequency(freq, sampleRate);

    if(! lowCut.isBypassed<0>()) 
      mag *= lowCut.get<0>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
    if(! lowCut.isBypassed<1>()) 
      mag *= lowCut.get<1>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
    if(! lowCut.isBypassed<2>()) 
      mag *= lowCut.get<2>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
    if(! lowCut.isBypassed<3>()) 
      mag *= lowCut.get<3>().coefficients->getMagnitudeForFrequency(freq, sampleRate);


    if(! highCut.isBypassed<0>()) 
      mag *= highCut.get<0>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
    if(! highCut.isBypassed<1>()) 
      mag *= highCut.get<1>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
    if(! highCut.isBypassed<2>()) 
      mag *= highCut.get<2>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
    if(! highCut.isBypassed<3>()) 
      mag *= highCut.get<3>().coefficients->getMagnitudeForFrequency(freq, sampleRate);

    mags[i] = juce::Decibels::gainToDecibels(mag);
  }

  Path responseCurve;

  const double outputMin = responseArea.getBottom();
  const double outputMax = responseArea.getY();
  auto map = [outputMin, outputMax](double input)
   {
    return juce::jmap(input, -24.0, 24.0, outputMin, outputMax);
  };

  responseCurve.startNewSubPath(static_cast<float>(responseArea.getX()), static_cast<float>(map(mags.front())));

  for ( size_t i = 1; i < mags.size(); ++i)
  {
    responseCurve.lineTo(static_cast<float>(responseArea.getX() + i), static_cast<float>(map(mags[i])));
  }

  g.setColour(Colours::orange);
  g.drawRoundedRectangle(responseArea.toFloat(), 4.f, 1.f);
  g.setColour(Colours::white);
  g.strokePath(responseCurve, PathStrokeType(2.f));
}

void AudioPluginAudioProcessorEditor::resized() {
  // This is generally where you'll want to lay out the positions of any
  // subcomponents in your editor..

  auto bounds = getLocalBounds();
  auto responseArea = bounds.removeFromTop(static_cast<int>(bounds.getHeight() * 0.33));

  auto lowCutArea = bounds.removeFromLeft(static_cast<int>(bounds.getWidth() * 0.33));
  auto highCutArea = bounds.removeFromRight(static_cast<int>(bounds.getWidth() * 0.5));
  
  lowCutFreqSlider.setBounds(lowCutArea.removeFromTop(static_cast<int>(lowCutArea.getHeight() * 0.5)));
  lowCutSlopeSlider.setBounds(lowCutArea);
  highCutFreqSlider.setBounds(highCutArea.removeFromTop(static_cast<int>(highCutArea.getHeight() * 0.5)));
  highCutSlopeSlider.setBounds(highCutArea);

  peakFreqSlider.setBounds(bounds.removeFromTop(static_cast<int>(bounds.getHeight() * 0.33)));
  peakGainSlider.setBounds(bounds.removeFromTop(static_cast<int>(bounds.getHeight() * 0.5)));
  peakQualitySlider.setBounds(bounds);
}

void AudioPluginAudioProcessorEditor::parameterValueChanged (int parameterIndex, float newValue) {
  juce::ignoreUnused(parameterIndex, newValue);
  parametersChanged.set(true);
}

void AudioPluginAudioProcessorEditor::timerCallback() {
  if(parametersChanged.compareAndSetBool(false, true)) {
    DBG("Parameter changed");
    //update the monochain
    auto chainSettings = getChainSettings(processorRef.apvts);
    auto peakCoefficients = makePeakFilter(chainSettings, processorRef.getSampleRate());
    updateCoefficients(monoChain.get<ChainPositions::Peak>().coefficients, peakCoefficients);
    repaint();
    //signal a repaint
  }
}

std::vector<juce::Component*> AudioPluginAudioProcessorEditor::getComps() {
  //each pointer is a reference to a slider object
  return {
    &peakFreqSlider,
    &peakGainSlider,
    &peakQualitySlider,
    &highCutFreqSlider,
    &lowCutFreqSlider,
    &lowCutSlopeSlider,
    &highCutSlopeSlider
  };
}
}  // namespace audio_plugin
