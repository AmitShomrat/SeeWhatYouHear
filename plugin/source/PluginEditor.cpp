#include "SimpleEQ/PluginEditor.h"
#include "SimpleEQ/PluginProcessor.h"

namespace audio_plugin {

// Add utility function at the top of the namespace
float getTextWidth(const juce::Font& font, const juce::String& text)
{
    juce::GlyphArrangement glyphs;
    glyphs.addFittedText(font, text, 0.0f, 0.0f, 1000.0f, 100.0f, juce::Justification::left, 1);
    return glyphs.getBoundingBox(0, -1, true).getWidth();
}

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
    // A reference to the slider position object designing a tall and thin rectangle.
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

    //Adding text of value to the slider.
    g.setFont(static_cast<float>(rswl -> getTextHeight()));
    auto text = rswl -> getDisplayString();

    // Use utility function instead of duplicated code
    auto textWidth = getTextWidth(g.getCurrentFont(), text);

    r.setSize(textWidth + 4, static_cast<float>(rswl -> getTextHeight()) + 2);
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

  // g.setColour(Colours::red);
  // g.drawRect(getLocalBounds()); //Debugging tests.
  // g.setColour(Colours::white);
  // g.drawRect(sliderBounds); //Debugging tests.

  getLookAndFeel().drawRotarySlider(g, 
                                    sliderBounds.getX(), 
                                    sliderBounds.getY(), 
                                    sliderBounds.getWidth(), 
                                    sliderBounds.getHeight(), 
                                    static_cast<float>( jmap(getValue(), range.getStart(), range.getEnd(), 0.0, 1.0) ) , 
                                    startAng, 
                                    endAng, 
                                    *this);

                                    
  auto center = sliderBounds.toFloat().getCentre();
  auto radius = sliderBounds.getWidth() * 0.5;


  auto numChoices = labels.size(); 
  for(int i = 0; i < numChoices; ++i )
  {
    auto pos = labels[i].pos;
    jassert(0.f <= pos);
    jassert(pos <= 1.f);

    auto ang = jmap(pos, 0.f, 1.f, startAng, endAng);
    auto c = center.getPointOnCircumference(static_cast<float>(radius) + getTextHeight() * 0.5f + 1, ang);

    Rectangle<float> r;
    auto str = labels[i].label;
    
    // Use utility function instead of duplicated code
    auto textWidth = getTextWidth(g.getCurrentFont(), str);

    r.setSize(static_cast<float>(textWidth), static_cast<float>(getTextHeight()));
    r.setCentre(c);
    r.setY(r.getY() + getTextHeight());

    // g.setColour(Colours::white);
    // g.drawRect(r); //Debugging tests.

    g.setColour(juce::Colours::red) ;
    g.setFont(static_cast<float>(getTextHeight()));
    g.drawFittedText(str, r.toNearestInt(), juce::Justification::centred, 1);
  }                                  
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
  if (auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*>(param)) 
    return choiceParam -> getCurrentChoiceName();
  
  juce::String str;
  bool addK = false;
  
  if(auto* floatParam = dynamic_cast<juce::AudioParameterFloat*>(param)) {
    float val = floatParam -> get();

    if(val > 999.f) {
      val /= 1000.f;
      addK = true;
    }
    str = juce::String(val, (addK ? 2 : 0));
  }

  else 
  {
    jassertfalse; //This should never happen.
  }

  if(suffix.isNotEmpty()) {
    str << " " ;
    if(addK) str << "k";
    str << suffix;
  }
 return str;
}

//============================================================================================================================
//This Component is a listener and Timer object.
ResponseCurveComponent::ResponseCurveComponent(AudioPluginAudioProcessor& p)
: processorRef(p), 
leftChannelFifo(&processorRef.leftChannelFifo)
{  

  const auto& params = processorRef.getParameters();
  for(auto param : params) {
    param -> addListener(this);// To observe the changes in the parameters.
  }

  leftChannelFFTDataGenerator.changeOrder(FFTOrder::order2048);
  monoBuffer.setSize(1, leftChannelFFTDataGenerator.getFFTSize());
  updateChain();
  
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
  juce::AudioBuffer<float> tempIncomingBuffer;

  while(leftChannelFifo->getNumCompleteBuffersAvailable() > 0)
  {
    if(leftChannelFifo->getAudioBuffer(tempIncomingBuffer))
    {
      auto size = tempIncomingBuffer.getNumSamples();

      juce::FloatVectorOperations::copy(monoBuffer.getWritePointer(0, 0), 
                                        monoBuffer.getReadPointer(0, size), 
                                        monoBuffer.getNumSamples() - size);

      juce::FloatVectorOperations::copy(monoBuffer.getWritePointer(0, monoBuffer.getNumSamples() - size),
                                        tempIncomingBuffer.getReadPointer(0, 0), 
                                        size);

      leftChannelFFTDataGenerator.produceFFTDataForRendering(monoBuffer, -48.f);                                      
    }
  }

  /*
  if there are FFT data buffer to pull.
    if we can pull a buffer, generate a path 
  */
  const auto fftBounds = getAnalysisArea().toFloat();
  const auto fftSize = leftChannelFFTDataGenerator.getFFTSize();
  /*
  48000 / 2048 = 23Hz <- this is the bin width.
  */
  const auto binWidth = processorRef.getSampleRate() / static_cast<double>(fftSize);

  while(leftChannelFFTDataGenerator.getNumAvailableFFTDataBlocks() > 0)
  {
    std::vector<float> fftData;
    if(leftChannelFFTDataGenerator.getFFTData(fftData))
    {
      pathProducer.generatePath(fftData, fftBounds, fftSize, static_cast<float>(binWidth), -48.f);
    }
  }

  /*
  while there are path producer available, 
      pull as many as you can 
            display the most recent one.
  */

  while(pathProducer.getNumPathsAvailable() > 0)
  {
    pathProducer.getPath(leftChannelFFTPath );
  }
  


  if(parametersChanged.compareAndSetBool(false, true)) {// If the parameter value has changed, then update the monochain.
    DBG("Parameter changed");
    //update the monochain
    //signal a repaint
    updateChain();
  }
  repaint();
}

void ResponseCurveComponent::updateChain() 
{

    auto chainSettings = getChainSettings(processorRef.apvts);
    auto peakCoefficients = makePeakFilter(chainSettings, processorRef.getSampleRate());
    updateCoefficients(monoChain.get<ChainPositions::Peak>().coefficients, peakCoefficients);

    auto lowCutCoefficients = makeLowCutFilter(chainSettings, processorRef.getSampleRate());
    auto highCutCoefficients = makeHighCutFilter(chainSettings, processorRef.getSampleRate());

    updateCutFilters(monoChain.get<ChainPositions::LowCut>(), lowCutCoefficients, chainSettings.lowCutSlope);
    updateCutFilters(monoChain.get<ChainPositions::HighCut>(), highCutCoefficients, chainSettings.highCutSlope);

}
void ResponseCurveComponent::paint(juce::Graphics& g) {
  using namespace juce;
  g.fillAll(Colours::black);

  g.drawImage(background, getLocalBounds().toFloat());

  // auto responseArea = getLocalBounds();
  auto responseArea = getAnalysisArea();
  
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

  g.setColour(Colours::blue);
  g.strokePath(leftChannelFFTPath, PathStrokeType(1.f));

  g.setColour(Colours::orange);
  g.drawRoundedRectangle(getRenderArea().toFloat(), 4.f, 1.f);
  g.setColour(Colours::white);
  g.strokePath(responseCurve, PathStrokeType(2.f));
}

void ResponseCurveComponent::resized() 
{
  using namespace juce;
  background = Image(Image::PixelFormat::RGB, getWidth(), getHeight(), true);

  Graphics g(background);
  g.fillAll(Colours::black);
  Array<float> freqs
  {
    20.0f, /*30.0f, 40.0f,*/ 50.0f, 100.0f,
    200.0f, /*300.0f, 400.0f,*/ 500.0f, 1000.0f, 
    2000.0f, /*3000.0f, 4000.0f,*/ 5000.0f, 10000.0f, 
    20000.0f
  };

  auto renderArea = getAnalysisArea();
  auto left = renderArea.getX();
  auto right = renderArea.getRight();
  auto top = renderArea.getY();
  auto bottom = renderArea.getBottom();
  auto width = renderArea.getWidth();

  Array<float> xs;
  for(auto f : freqs)
  {
    auto normX = mapFromLog10<float>(f, 20.0f, 20000.0f);
    xs.add(left + width * normX);
  }

  g.setColour(Colours::dimgrey);
  for ( auto x : xs)
  {
    // auto normX = mapFromLog10<float>(f, 20.0f, 20000.0f);
    // juce::ignoreUnused(normX);

    g.drawVerticalLine(static_cast<int>(x), static_cast<float>(top), static_cast<float>(bottom));
  }
  
  Array<float> gains
  {
    -24.0f, -12.0f, 0.0f, 12.0f, 24.0f
  };

  for ( auto gDb :gains)
  {
    auto y = jmap(gDb, -24.0f, 24.0f, float(bottom), float(top));

    g.setColour(gDb == 0.f ? Colours::red : Colours::darkgrey);
    g.drawHorizontalLine(static_cast<int>(y), static_cast<float>(left), static_cast<float>(right));

  }

  // g.drawRect(getAnalysisArea());

  g.setColour(Colours::lightgrey);
  const int fontHeight = 10;
  g.setFont(fontHeight);

  for(int i = 0; i < freqs.size(); ++i)
  {
    auto f = freqs[i];
    auto x = xs[i];

    bool addK = false;
    String str;
    if(f > 999.f)
    {
      addK = true;
      f /= 1000.f;
    }

    str << f;
    if(addK) 
      str << "K";
    str << "Hz";

    // Use utility function instead of duplicated code
    auto textWidth = getTextWidth(g.getCurrentFont(), str);

    Rectangle<int> r;
    r.setSize(static_cast<int>(textWidth), fontHeight);
    r.setCentre(static_cast<int>(x), 0);
    r.setY(1);

    g.drawFittedText(str, r, juce::Justification::centred, 1);
  }

  
  
  for ( auto gDb : gains)
  {
    auto y = jmap(gDb, -24.0f, 24.0f, float(bottom), float(top));

    String str;
    if (gDb > 0)
      str << "+";
    str << gDb;

    auto textWidth = getTextWidth(g.getCurrentFont(), str);

    Rectangle<int> r;
    r.setSize(static_cast<int>(textWidth), fontHeight);
    r.setX(static_cast<int>(getWidth() - textWidth));
    r.setCentre(r.getCentreX(), static_cast<int>(y));

    g.setColour(gDb == 0.f ? Colours::red : Colours::lightgrey);

    g.drawFittedText(str, r, juce::Justification::centred, 1);

    str.clear();
    str << (gDb - 24.0f);

    r.setX(1);
    r.setSize(static_cast<int>(textWidth), fontHeight);
    g.setColour(Colours::lightgrey);
    g.drawFittedText(str, r, juce::Justification::centred, 1);
  }
}

juce::Rectangle<int> ResponseCurveComponent::getRenderArea()
{
  auto bounds = getLocalBounds();
  // bounds.reduce(10 ,/*JUCE_LIVE_CONSTANT(5)*/
                //  8 /*JUCE_LIVE_CONSTANT(5)*/);
  bounds.removeFromTop(12);
  bounds.removeFromBottom(2);
  bounds.removeFromLeft(20);
  bounds.removeFromRight(20);
  return bounds;
}

juce::Rectangle<int> ResponseCurveComponent::getAnalysisArea()
{
  auto bounds = getRenderArea();
  bounds.removeFromTop(4);
  bounds.removeFromBottom(4);
  return bounds;
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
  // Add labels to all sliders
  peakFreqSlider.labels.add({0.f, "20Hz"});
  peakFreqSlider.labels.add({1.f, "20kHz"});
  lowCutFreqSlider.labels.add({0.f, "20Hz"});
  lowCutFreqSlider.labels.add({1.f, "20kHz"});
  highCutFreqSlider.labels.add({0.f, "20Hz"});
  highCutFreqSlider.labels.add({1.f, "20kHz"});
  peakGainSlider.labels.add({0.f, "-24dB"});
  peakGainSlider.labels.add({1.f, "+24dB"});
  peakQualitySlider.labels.add({0.f, "0.1"});
  peakQualitySlider.labels.add({1.f, "10.0"});
  lowCutSlopeSlider.labels.add({0.f, "12"});
  lowCutSlopeSlider.labels.add({1.f, "48"});
  highCutSlopeSlider.labels.add({0.f, "12"});
  highCutSlopeSlider.labels.add({1.f, "48"});

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
  float hRatio = 30 / 100.f; //JUCE_LIVE_CONSTANT(33) / 100.f;
  auto responseArea = bounds.removeFromTop(static_cast<int>(bounds.getHeight() * hRatio));

  responseCurveComponent.setBounds(responseArea);
  bounds.removeFromTop(5);
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
