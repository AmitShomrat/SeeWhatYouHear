#include "SimpleEQ/PluginEditor.h"
#include "SimpleEQ/PluginProcessor.h"

namespace audio_plugin {

ResponseCurveComponent::ResponseCurveComponent(AudioPluginAudioProcessor& p)
: processorRef(p)
{
  const auto& params = processorRef.getParameters();
  for(auto param : params) {
    param -> addListener(this);
  }

  startTimerHz(60);
}

void ResponseCurveComponent::parameterValueChanged (int parameterIndex, float newValue) {
  juce::ignoreUnused(parameterIndex, newValue);
  parametersChanged.set(true);
}

ResponseCurveComponent::~ResponseCurveComponent() {
  const auto& params = processorRef.getParameters();
  for(auto param : params) {
    param -> removeListener(this);
  }
}

void ResponseCurveComponent::timerCallback() {
  if(parametersChanged.compareAndSetBool(false, true)) {
    DBG("Parameter changed");
    //update the monochain
    auto chainSettings = getChainSettings(processorRef.apvts);
    auto peakCoefficients = makePeakFilter(chainSettings, processorRef.getSampleRate());
    updateCoefficients(monoChain.get<ChainPositions::Peak>().coefficients, peakCoefficients);

    auto lowCutCoefficients = makeLowCutFilter(chainSettings, processorRef.getSampleRate());
    auto highCutCoefficients = makeHighCutFilter(chainSettings, processorRef.getSampleRate());

    updateCutFilters(monoChain.get<ChainPositions::LowCut>(), lowCutCoefficients, chainSettings.lowCutSlope);
    updateCutFilters(monoChain.get<ChainPositions::HighCut>(), highCutCoefficients, chainSettings.highCutSlope);

    repaint();
    //signal a repaint
  }
}

void ResponseCurveComponent::paint(juce::Graphics& g) {
  using namespace juce;
  g.fillAll(Colours::black);

  auto responseArea = getLocalBounds();
  
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
  auto map = [outputMin, outputMax](double input) {
    return juce::jmap(input, -24.0, 24.0, outputMin, outputMax);
  };

  responseCurve.startNewSubPath(static_cast<float>(responseArea.getX()), static_cast<float>(map(mags.front())));

  for (size_t i = 1; i < mags.size(); ++i) {
    responseCurve.lineTo(static_cast<float>(responseArea.getX() + i), static_cast<float>(map(mags[i])));
  }

  g.setColour(Colours::orange);
  g.drawRoundedRectangle(responseArea.toFloat(), 4.f, 1.f);
  g.setColour(Colours::white);
  g.strokePath(responseCurve, PathStrokeType(2.f));
}

AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor(AudioPluginAudioProcessor& p)
    : juce::AudioProcessorEditor(&p), 
      processorRef(p),
      responseCurveComponent(p),
      peakFreqSliderAttachment(p.apvts, "Peak Freq", peakFreqSlider),
      peakGainSliderAttachment(p.apvts, "Peak Gain", peakGainSlider),
      peakQualitySliderAttachment(p.apvts, "Peak Quality", peakQualitySlider),
      lowCutFreqSliderAttachment(p.apvts, "LowCut Freq", lowCutFreqSlider),
      highCutFreqSliderAttachment(p.apvts, "HighCut Freq", highCutFreqSlider),
      lowCutSlopeSliderAttachment(p.apvts, "LowCut Slope", lowCutSlopeSlider),
      highCutSlopeSliderAttachment(p.apvts, "HighCut Slope", highCutSlopeSlider)
{
  juce::ignoreUnused(processorRef);

  for(auto* comp : getComps()) {
    this -> addAndMakeVisible(comp);
  }

  setSize(600, 400);
}

AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor() 
{

}

void AudioPluginAudioProcessorEditor::paint(juce::Graphics& g) {
  using namespace juce;
  g.fillAll(Colours::black);
}

void AudioPluginAudioProcessorEditor::resized() {
  // This is generally where you'll want to lay out the positions of any
  // subcomponents in your editor..

  auto bounds = getLocalBounds();
  auto responseArea = bounds.removeFromTop(static_cast<int>(bounds.getHeight() * 0.33));

  responseCurveComponent.setBounds(responseArea);

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

std::vector<juce::Component*> AudioPluginAudioProcessorEditor::getComps() {
  //each pointer is a reference to a slider object
  return {
    &peakFreqSlider,
    &peakGainSlider,
    &peakQualitySlider,
    &highCutFreqSlider,
    &lowCutFreqSlider,
    &lowCutSlopeSlider,
    &highCutSlopeSlider,
    &responseCurveComponent
  };
}
}  // namespace audio_plugin
