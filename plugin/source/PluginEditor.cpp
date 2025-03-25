#include "SimpleEQ/PluginEditor.h"
#include "SimpleEQ/PluginProcessor.h"

namespace audio_plugin {


void LookAndFeel::drawRotarySlider(juce::Graphics& g,
                                  int x,
                                  int y,
                                  int width,
                                  int height, 
                                  float sliderPosProportional, 
                                  float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider) 
{
  using namespace juce;

  auto bounds = Rectangle<float>(static_cast<float>(x), static_cast<float>(y), static_cast<float>(width), static_cast<float>(height));
  g.setColour(Colours::red);
  g.fillEllipse(bounds);

  g.setColour(Colours::white);
  g.drawEllipse(bounds, 1.f);

  if(auto* rswl = dynamic_cast<RotarySliderWithLabels*>(&slider)) {

    auto center = bounds.getCentre();
    // A reference to position of a slider position object desining a tall and thin rectangle.
    Path p; 

    Rectangle<float> r;
    r.setLeft(center.getX() - 2);
    r.setRight(center.getX() + 2);
    r.setTop(bounds.getY());
    r.setBottom(center.getY() - static_cast<float>(rswl -> getTextHeight()) * 0.5f);

    p.addRoundedRectangle(r, 2.f);
    //Debugging tests.
    jassert(rotaryStartAngle < rotaryEndAngle);

    auto sliderAngRad = jmap(sliderPosProportional, 0.0f, 1.0f, rotaryStartAngle, rotaryEndAngle);

    p.applyTransform(AffineTransform().rotated(sliderAngRad, center.getX(), center.getY()));

    g.fillPath(p);

    g.setFont(static_cast<float>(rswl -> getTextHeight()));
    auto text = rswl -> getDisplayString();
    
    // Replace deprecated getStringWidth with GlyphArrangement
    juce::GlyphArrangement glyphs;
    glyphs.addFittedText(g.getCurrentFont(), text, 0.0f, 0.0f, 1000.0f, 100.0f, juce::Justification::left, 1);
    auto textWidth = glyphs.getBoundingBox(0, -1, true).getWidth();

    r.setSize(static_cast<float>(textWidth) + 4, static_cast<float>(rswl -> getTextHeight()) + 2);
    r.setCentre(bounds.getCentre());
    
    g.setColour(Colours::black);
    g.fillRect(r);

    g.setColour(Colours::white);
    g.drawFittedText(text, r.toNearestInt(), juce::Justification::centred, 1);
  }

  
  juce::ignoreUnused(sliderPosProportional, rotaryStartAngle, rotaryEndAngle, slider);
}


void RotarySliderWithLabels::paint(juce::Graphics& g) {
  using namespace juce;

  auto startAng = degreesToRadians(180.f + 45.f); 
  auto endAng = degreesToRadians(180.f - 45.f) + MathConstants<float>::twoPi;

  auto range = getRange();

  auto sliderBounds = getSliderBounds();

  g.setColour(Colours::red);
  g.drawRect(getLocalBounds()); 
  g.setColour(Colours::white);
  g.drawRect(sliderBounds);



  getLookAndFeel().drawRotarySlider(g, 
                                    sliderBounds.getX(), 
                                    sliderBounds.getY(), 
                                    sliderBounds.getWidth(), 
                                    sliderBounds.getHeight(), 
                                    static_cast<float>( jmap(getValue(), range.getStart(), range.getEnd(), 0.0, 1.0) ) , 
                                    startAng, 
                                    endAng, 
                                    *this);
}

juce::Rectangle<int> RotarySliderWithLabels::getSliderBounds() const
{
  //return getLocalBounds();
  auto bounds = getLocalBounds();
  auto size = juce::jmin(bounds.getWidth(), bounds.getHeight());

  size -= getTextHeight() * 2;
  juce::Rectangle<int> r;
  r.setSize(size, size);
  r.setCentre(bounds.getCentreX(), 0);

  r.setY(2); //To offset the text from the top of the slider.

  return r;
}

juce::String RotarySliderWithLabels::getDisplayString() const 
{
  return juce::String(getValue());
}

//============================================================================================================================
//This Component is a listener and Timer object.
ResponseCurveComponent::ResponseCurveComponent(AudioPluginAudioProcessor& p)
: processorRef(p)
{
  const auto& params = processorRef.getParameters();
  for(auto param : params) {
    param -> addListener(this);// To observe the changes in the parameters.
  }

  startTimerHz(60); //timerCallback function is called 60 times per second.
}
//This function is called when the parameter value changes.
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
  if(parametersChanged.compareAndSetBool(false, true)) {// If the parameter value has changed, then update the monochain.
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
    : juce::AudioProcessorEditor(&p), processorRef(p),
      peakFreqSlider(*processorRef.apvts.getParameter("Peak Freq"), "Hz"),
      peakGainSlider(*processorRef.apvts.getParameter("Peak Gain"), "dB"),
      peakQualitySlider(*processorRef.apvts.getParameter("Peak Quality"), ""),
      lowCutFreqSlider(*processorRef.apvts.getParameter("LowCut Freq"), "Hz"),
      highCutFreqSlider(*processorRef.apvts.getParameter("HighCut Freq"), "Hz"),
      lowCutSlopeSlider(*processorRef.apvts.getParameter("LowCut Slope"), "dB/Oct"),
      highCutSlopeSlider(*processorRef.apvts.getParameter("HighCut Slope"), "dB/Oct"),

      responseCurveComponent(p),
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

  setSize(600,400);
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
  //Taking 1/3 of the width of the bounds and removing it from the left
  auto lowCutArea = bounds.removeFromLeft(static_cast<int>(bounds.getWidth() * 0.33));
  //Taking 1/2 of the remaining width of the bounds and removing it from the right
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
  //each pointer is a reference to a component object ( Slider is a component )
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
