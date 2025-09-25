#include "SeeWhatYouHear/PluginEditor.h"
#include "SeeWhatYouHear/PluginProcessor.h"
#include "SeeWhatYouHear/LEDCommunication.h"
#include <juce_gui_extra/juce_gui_extra.h>

namespace audio_plugin {


float getTextWidth(const juce::Font& font, const juce::String& text)
{
    juce::GlyphArrangement glyphs;
    glyphs.addFittedText(font, text, 0.0f, 0.0f, 1000.0f, 100.0f, juce::Justification::left, 1, 0.f);
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

  auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y), static_cast<float>(width), static_cast<float>(height));
  g.setColour(Colours::red);
  g.fillEllipse(bounds);

  g.setColour(Colours::white);
  g.drawEllipse(bounds, 1.f);

  if(auto* rswl = dynamic_cast<RotarySliderWithLabels*>(&slider)) {

    auto center = bounds.getCentre();
    Path p; 

    juce::Rectangle<float> r;
    r.setLeft(center.getX() - 2);
    r.setRight(center.getX() + 2);
    r.setTop(bounds.getY());
    r.setBottom(center.getY() - static_cast<float>(14.0f) * 0.5f);

    p.addRoundedRectangle(r, 2.f);

    jassert(rotaryStartAngle < rotaryEndAngle);

    auto sliderAngRad = jmap(sliderPosProportional, 0.0f, 1.0f, rotaryStartAngle, rotaryEndAngle);

    p.applyTransform(AffineTransform().rotated(sliderAngRad, center.getX(), center.getY()));

    // Debugging: Check the path bounds and angle
    DBG("Path bounds: " << p.getBounds().toString());
    DBG("Slider angle (radians): " << sliderAngRad);

    g.fillPath(p);

    // Adding text of value to the slider.
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

    juce::Rectangle<float> r;
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
: processorRef(p), leftPathProducer(p, Channel::Left), rightPathProducer(p, Channel::Right)
{  
  startTimerHz(60); //timerCallback function is called 60 times per second.
}
//This function is called when the parameter value changes.
void ResponseCurveComponent::parameterValueChanged (int parameterIndex, float newValue) {
  juce::ignoreUnused(parameterIndex, newValue);
  parametersChanged.set(true);
}

ResponseCurveComponent::~ResponseCurveComponent() {}
void PathProducer::process(juce::Rectangle<float> fftBounds) 
{
    while(processorRef.fftProcessor-> isChannelAvailable(channel))//Consuming FFTData blocks in order to generate a path.
    {
      std::vector<float> fftData;
      auto fftSize = processorRef.fftProcessor -> getFFTSize();
      auto binWidth = processorRef.fftProcessor -> getFFTBinWidth();
      if(processorRef.fftProcessor-> getLatestFFTData(fftData, channel))
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
      pathProducer.getPath(channelFFTPath);
    }
}

void ResponseCurveComponent::timerCallback() {
  
  auto fftBounds = getAnalysisArea().toFloat();

  leftPathProducer.process(fftBounds);
  rightPathProducer.process(fftBounds);

  repaint();
}
void ResponseCurveComponent::paint(juce::Graphics& g) {
  using namespace juce;
  g.fillAll(Colours::black);

  g.drawImage(background, getLocalBounds().toFloat());

  // auto responseArea = getLocalBounds();
  auto responseArea = getAnalysisArea();
  
  auto leftChannelFFTPath = leftPathProducer.getPath();
  leftChannelFFTPath.applyTransform(AffineTransform().translation(static_cast<float>(responseArea.getX()), static_cast<float>(responseArea.getY())));

  g.setColour(Colours::skyblue);
  g.strokePath(leftChannelFFTPath, PathStrokeType(1.f));

  auto rightChannelFFTPath = rightPathProducer.getPath();
  rightChannelFFTPath.applyTransform(AffineTransform().translation(static_cast<float>(responseArea.getX()), static_cast<float>(responseArea.getY())));

  g.setColour(Colours::lightyellow);
  g.strokePath(rightChannelFFTPath, PathStrokeType(1.f));

  g.setColour(Colours::orange);
  g.drawRoundedRectangle(getRenderArea().toFloat(), 4.f, 1.f);
  // g.setColour(Colours::white);
  // g.strokePath(responseCurve, PathStrokeType(2.f));
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

    juce::Rectangle<int> r;
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

    juce::Rectangle<int> r;
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

LEDSimulator::LEDSimulator(AudioPluginAudioProcessor& p)
    : processorRef(p)
{
    startTimerHz(60);
}

LEDSimulator::~LEDSimulator()
{
    stopTimer();
}

void LEDSimulator::paint(juce::Graphics& g)
{
    auto bounds = getLEDArea();

    // g.setColour(juce::Colours::red);
    // g.drawRect(getLocalBounds());
    // g.setColour(juce::Colours::white);
    // g.drawRect(bounds);
    // Draw background
    g.setColour(juce::Colours::black);
    g.fillRect(bounds);
    
    // Calculate narrower LED areas (40% width each) with space for labels
    float ledWidth = bounds.getWidth() * 0.35f; // Make LEDs narrower
    float labelWidth = bounds.getWidth() * 0.10f; // Space for labels
    
    // Create label areas at edges
    auto leftLabelArea = bounds.withWidth(labelWidth);
    auto rightLabelArea = bounds.withX(bounds.getRight() - labelWidth).withWidth(labelWidth);
    
    // Create LED areas
    auto leftLedArea = bounds.withX(leftLabelArea.getRight()).withWidth(ledWidth);
    auto rightLedArea = bounds.withX(bounds.getRight() - labelWidth - ledWidth).withWidth(ledWidth);
    
    // Create LED colors with proper uint8_t casting
    juce::Colour leftLedColor = juce::Colour::fromRGB(static_cast<uint8_t>(currentLeftRGB.r), static_cast<uint8_t>(currentLeftRGB.g), static_cast<uint8_t>(currentLeftRGB.b));
    juce::Colour rightLedColor = juce::Colour::fromRGB(static_cast<uint8_t>(currentRightRGB.r), static_cast<uint8_t>(currentRightRGB.g), static_cast<uint8_t>(currentRightRGB.b));

    // std::cout << "leftRGB: " << static_cast<int>(leftLedColor.getRed()) << " " << static_cast<int>(leftLedColor.getGreen()) << " " << static_cast<int>(leftLedColor.getBlue()) << std::endl;
    // std::cout << "rightRGB: " << static_cast<int>(rightLedColor.getRed()) << " " << static_cast<int>(rightLedColor.getGreen()) << " " << static_cast<int>(rightLedColor.getBlue()) << std::endl;
    // Draw LEDs
    drawLED(g, leftLedArea, leftChannelLevel, leftLedColor);
    drawLED(g, rightLedArea, rightChannelLevel, rightLedColor);

    // Draw labels
    // Use a simpler font approach
    g.setFont(bounds.getHeight() * 0.3f);
    g.setColour(juce::Colours::white);
    
    // Left "L" label
    g.drawText("L", leftLabelArea, juce::Justification::centred);
    
    // Right "R" label
    g.drawText("R", rightLabelArea, juce::Justification::centred);
}

void LEDSimulator::resized()
{
    // Nothing specific needed here
}

void LEDSimulator::timerCallback()
{
    auto newLeftBrightness = processorRef.getBrightnessDecision().getLeftBrightness();
    auto newRightBrightness = processorRef.getBrightnessDecision().getRightBrightness();
    
    RGB leftRGB = processorRef.fftProcessor->getLeftRGB();
    RGB rightRGB = processorRef.fftProcessor->getRightRGB();
    
    // Check for any changes in brightness or color
    bool needsRepaint = false;

    if (std::abs(newLeftBrightness.get() - leftChannelLevel) > 0.01f || std::abs(newRightBrightness.get() - rightChannelLevel) > 0.01f  
                                                               || currentLeftRGB != leftRGB || currentRightRGB != rightRGB) 
    {
        needsRepaint = true;
    }

    if (needsRepaint) {
        leftChannelLevel = newLeftBrightness.get();
        rightChannelLevel = newRightBrightness.get();
        currentLeftRGB = leftRGB;
        currentRightRGB = rightRGB;
        repaint();
    }
}

juce::Rectangle<float> LEDSimulator::getLEDArea()
{
  auto bounds = getLocalBounds().toFloat();
  bounds.removeFromTop(4);
  bounds.removeFromBottom(4);
  bounds.removeFromLeft(4);
  bounds.removeFromRight(4);
  return bounds;
}

void LEDSimulator::drawLED(juce::Graphics& g, juce::Rectangle<float> bounds, float brightness, juce::Colour color)
{
    // Scale brightness with more aggressive non-linear curve for better dynamic range
    // This will keep low levels very dark and make high levels pop
    float scaledBrightness = std::pow(brightness, 0.5f); // Less aggressive curve (0.5 instead of 0.7)
    
    auto center = bounds.getCentre();
    
    // When brightness is extremely low, keep it almost completely black
    if (scaledBrightness < 0.05f) {
        g.setColour(juce::Colours::black);
        g.fillRect(bounds);
        return; // Skip the rest of the drawing for very low levels
    }
    
    // 1. Draw background that's darker at low levels
    g.setColour(juce::Colours::black);
    g.fillRoundedRectangle(bounds, 10.0f);
    
    // 2. Draw main LED body with opacity directly tied to level
    // Use a brighter base color for higher maximum brightness
    auto mainColor = color.brighter(0.2f).withAlpha(juce::jmap(scaledBrightness, 0.0f, 1.0f, 0.0f, 0.9f));
    g.setColour(mainColor);
    g.fillRoundedRectangle(bounds, 10.0f);
    
    // 3. Draw gradient overlay with enhanced brightness at higher levels
    juce::ColourGradient gradient;
    gradient.point1 = center;
    gradient.point2 = juce::Point<float>(bounds.getRight(), bounds.getBottom());
    
    // Make high brightness levels much more intense
    auto intensityFactor = juce::jmap(scaledBrightness, 0.0f, 1.0f, 0.0f, 1.2f);
    
    // Add color stops for realistic glow with better dynamic range
    gradient.addColour(0.0, color.brighter(0.8f * intensityFactor).withAlpha(scaledBrightness * 0.9f));
    gradient.addColour(0.5, color.brighter(0.2f * intensityFactor).withAlpha(scaledBrightness * 0.7f));
    gradient.addColour(1.0, color.darker(0.1f).withAlpha(scaledBrightness * 0.5f));
    
    g.setGradientFill(gradient);
    g.fillRoundedRectangle(bounds, 10.0f);
    
    // 4. Add highlighted center that gets more intense with level
    if (scaledBrightness > 0.2f) { // Only show highlight when brightness is sufficient
        float highlightWidth = bounds.getWidth() * 0.4f; // Smaller highlight
        float highlightHeight = bounds.getHeight() * 0.2f; // Smaller highlight
        
        // Scale highlight size with brightness for more dramatic effect
        highlightWidth *= juce::jmap(scaledBrightness, 0.2f, 1.0f, 0.5f, 1.0f);
        highlightHeight *= juce::jmap(scaledBrightness, 0.2f, 1.0f, 0.5f, 1.0f);
        
        juce::Rectangle<float> highlightBounds(
            center.getX() - (highlightWidth * 0.5f),
            center.getY() - (highlightHeight * 0.5f),
            highlightWidth,
            highlightHeight
        );
        
        // Make highlight more intense at higher levels
        float highlightAlpha = juce::jmap(scaledBrightness, 0.2f, 1.0f, 0.1f, 0.7f);
        g.setColour(juce::Colours::white.withAlpha(highlightAlpha));
        g.fillRoundedRectangle(highlightBounds, 10.0f);
    }
    
    // 5. Add border between the two halves
    g.setColour(juce::Colours::black.withAlpha(0.5f)); // More visible divider
    if (bounds.getX() > 1.0f) { // Only draw on the right half
        g.drawLine(bounds.getX(), bounds.getY(), bounds.getX(), bounds.getBottom(), 1.5f); // Slightly thicker
    }
}

AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor(AudioPluginAudioProcessor& p)
    : juce::AudioProcessorEditor(&p), 
      processorRef(p),
      brightnessSlider(*processorRef.apvts.getParameter("Brightness"), ""),
      modeSlider(*processorRef.apvts.getParameter("Mode"), ""),
      responseCurveComponent(p),
      ledSimulator(p),
      brightnessSliderAttachment(processorRef.apvts, "Brightness", brightnessSlider),
      modeSliderAttachment(processorRef.apvts, "Mode", modeSlider)
{
  for(auto* comp : getComps()) {
    this -> addAndMakeVisible(comp);
  }
  setSize(600,400);
}

AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor() {}

void AudioPluginAudioProcessorEditor::paint(juce::Graphics& g) {
  using namespace juce;
  g.fillAll(Colours::black);
}

void AudioPluginAudioProcessorEditor::resized() {
  auto bounds = getLocalBounds();
  
  // Allocate space for response curve component (top 30%)
  float responseCurveRatio = 30 / 100.f;
  auto responseArea = bounds.removeFromTop(static_cast<int>(bounds.getHeight() * responseCurveRatio));
  responseCurveComponent.setBounds(responseArea);
  
  // Allocate space for LED simulator (next 30%)
  float ledSimulatorRatio = 30 / 100.f;
  auto ledArea = bounds.removeFromTop(static_cast<int>(bounds.getHeight() * ledSimulatorRatio));
  ledArea.setCentre(ledArea.getCentreX(), ledArea.getCentreY() + 10);
  ledSimulator.setBounds(ledArea);


  auto brightnessSliderArea = bounds.removeFromLeft(static_cast<int>(bounds.getWidth() * 0.33));
  brightnessSliderArea.removeFromTop(static_cast<int>(bounds.getHeight()* 0.10));
  brightnessSlider.setBounds(brightnessSliderArea.removeFromTop(static_cast<int>(bounds.getHeight()* 3)));

  auto colorSliderArea = bounds.removeFromRight(static_cast<int>(bounds.getWidth() * 0.5));
  colorSliderArea.removeFromTop(static_cast<int>(bounds.getHeight()* 0.10));
  modeSlider.setBounds(colorSliderArea.removeFromTop(static_cast<int>(bounds.getHeight()* 3)));

}

std::vector<juce::Component*> AudioPluginAudioProcessorEditor::getComps() {
  // Commented out all sliders as requested
  return {
    &modeSlider,
    &brightnessSlider,
    &responseCurveComponent,
    &ledSimulator
  };
}

}  // namespace audio_plugin

