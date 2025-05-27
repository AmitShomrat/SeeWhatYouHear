# 🐺 WolfSound's Audio Plugin Template

![Cmake workflow success badge](https://github.com/JanWilczek/audio-plugin-template/actions/workflows/cmake.yml/badge.svg)

Want to create an audio plugin (e.g., a VST3 plugin) with C++ but don't know how to go about?

Heard about the [JUCE C++ framework](https://github.com/juce-framework/JUCE) but not sure how to start a JUCE project?

Want to use CMake with JUCE but don't know how?

Want to be able to easily integrate third-party C++ libraries to your project?

Want to unit test your audio plugin?

Want to ensure maximum safety of your software?

And all this with a click of a button?

Well, this template allows you to immediately start your JUCE C++ framework audio plugin project with a CMake-based project structure. It involves

* clear repo structure
* C++ 23 standard
* effortless handling of third-party dependencies with the CPM package manager; use the C++ libraries you want together with JUCE
* highest warning level and "treat warnings as errors"
* ready-to-go unit test project with GoogleTest

Additionally

* continuous integration made easy with Github actions: build and run tests on the main branch and on every pull request
* automatic clang-format on C++ files run on every commit; don't worry about code formatting anymore!

I am personally using this template all the time.

Feel free to propose suggestions 😉

## Usage

This is a template repository which means you can right click "Use this template" on GitHub and create your own repo out of it.

After cloning it locally, you can proceed with the usual CMake workflow.

In the main repo directory execute

```bash
$ cmake -S . -B build
$ cmake --build build
```

The first run will take the most time because the dependencies (CPM, JUCE, and googletest) need to be downloaded.

Alternatively, you can use bundled CMake presets:

```bash
$ cmake --preset default # uses the Ninja build system
$ cmake --build build
$ ctest --preset default
```

Existing presets are `default`, `release`, and `Xcode`.

To run clang-format on every commit, in the main directory execute

```bash
pre-commit install
```

(for this you may need to install `pre-commit` with `pip`: `pip install pre-commit`).

Don't forget to change "YourPluginName" to, well, your plugin name everywhere 😉

## Testing with AudioPluginHost

You can easily test your plugin using JUCE's AudioPluginHost. The following build presets are available:

```bash
# Using Ninja build system
$ cmake --preset default
$ cmake --build --preset run-with-host      # Builds and runs AudioPluginHost
$ cmake --build --preset run-with-plugin    # Copies the plugin to a known location and runs AudioPluginHost

# Using Visual Studio
$ cmake --preset vs
$ cmake --build --preset vs-run-with-host   # Builds and runs AudioPluginHost in Visual Studio
$ cmake --build --preset vs-run-with-plugin # Copies the plugin to a known location and runs AudioPluginHost
```

The AudioPluginHost will be built as part of the build process, and your plugin will be automatically copied to the right location for testing.

## How was this template built?

See how I create this template step by step in this video:

[![Audio plugin template tutorial video](http://img.youtube.com/vi/Uq7Hwt18s3s/0.jpg)](https://www.youtube.com/watch?v=Uq7Hwt18s3s "Audio plugin template tutorial video")

---

**<---------------------------------------------SIMPLE_EQ--------------------------------------------->**

*This EQ is made by MatkatMusic and rebuilt on the purpose of learning DSP Programming. I will use this plugin skeleton as the DSP module in the Sound-Leds project* 
# 🎛️ SIMPLE EQ PLUGIN IMPLEMENTATION GUIDE

## PARAMETERS

### 1. Value Tree State Management

We start the app by defining an `AudioProcessorValueTreeState` inside our 'PluginProcessor.h':

```cpp
class AudioPluginAudioProcessor : public juce::AudioProcessor
{
public:
    // ...
    juce::AudioProcessorValueTreeState apvts;
    
private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    // ...
};
```

This class contains a `ValueTree` that manages an `AudioProcessor`'s entire state. Each APVTS should be attached to only one processor. We set its default parameters using a layout containing `RangedAudioParameters` and `AudioProcessorParameterGroups`.

### 2. Parameter Definition

The `AudioParameterFloat` is a subclass of `AudioProcessorParameter` used for sliders adjustable over a range of values:

```cpp
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    
    // Frequency parameter with logarithmic skew
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "Peak Freq", 
        "Peak Freq",
        juce::NormalisableRange<float>(20.f, 20000.f, 1.f, 0.25f), 
        750.f
    ));
    
    // Gain parameter with linear response
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "Peak Gain",
        "Peak Gain", 
        juce::NormalisableRange<float>(-24.f, 24.f, 0.1f, 1.f),
        0.0f
    ));
    
    // More parameters...
    
    return layout;
}
```

For each parameter, we use `NormalisableRange<float>` to set:
- Start/end range (e.g., 20Hz to 20000Hz)
- Interval value (step size)
- Skew factor (e.g., 0.25 makes 70% of the slider represent the first half of the range)

## DSP IMPLEMENTATION

### 1. Stereo Processing

Since this is a stereo plugin (2 channels), each signal processing class in the `dsp` namespace affects only a single channel (mono). We must duplicate processors for both channels:

```cpp
// Type aliases for cleaner code
using Filter = juce::dsp::IIR::Filter<float>;
using CutFilter = juce::dsp::ProcessorChain<Filter, Filter, Filter, Filter>;
using MonoChain = juce::dsp::ProcessorChain<CutFilter, Filter, CutFilter>;

// Create two chains - one for each channel
MonoChain leftChain, rightChain;
```

### 2. Filter Chain Structure

The complex `MonoChain` consists of three parts:
- `CutFilter` (LowCut) - High-pass filter with selectable slope
- `Filter` (Peak) - Parametric EQ band
- `CutFilter` (HighCut) - Low-pass filter with selectable slope

```cpp
enum ChainPositions {
    LowCut,
    Peak,
    HighCut
};
```

### 3. Preparing for Playback

Before processing audio, we must set up the necessary components:

```cpp
void prepareToPlay(double sampleRate, int samplesPerBlock) override
{
    // Configure processing specifications
    juce::dsp::ProcessSpec spec;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = 1;  // Each chain processes one channel
    spec.sampleRate = sampleRate;
    
    leftChain.prepare(spec);
    rightChain.prepare(spec);
    
    updateFilters();
}
```

The `ProcessSpec` defines preparation parameters such as sample rate, block size, and channel count.

### 4. Chain Settings and Parameters

We use a `ChainSettings` structure to store all parameter values:

```cpp
struct ChainSettings {
    float peakFreq { 0 };
    float peakGainInDecibels { 0 };
    float peakQuality { 1.f };
    float lowCutFreq { 0 };
    float highCutFreq { 0 };
    int lowCutSlope { 0 };
    int highCutSlope { 0 };
};

ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts)
{
    ChainSettings settings;
    
    // Thread-safe parameter access
    settings.peakFreq = apvts.getRawParameterValue("Peak Freq")->load();
    settings.peakGainInDecibels = apvts.getRawParameterValue("Peak Gain")->load();
    settings.peakQuality = apvts.getRawParameterValue("Peak Quality")->load();
    // More parameters...
    
    return settings;
}
```

Parameters are accessed using `getRawParameterValue()`, which returns a smart pointer to the actual (non-normalized) value. We use the `load()` function for thread-safe parameter access.

### 5. Setting Filter Coefficients

#### Peak Filter

```cpp
auto peakCoefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
    sampleRate,
    chainSettings.peakFreq,
    chainSettings.peakQuality,
    juce::Decibels::decibelsToGain(chainSettings.peakGainInDecibels)
);

// Update both channels
*leftChain.get<ChainPositions::Peak>().coefficients = *peakCoefficients;
*rightChain.get<ChainPositions::Peak>().coefficients = *peakCoefficients;
```

#### Cut Filters (Low/High)

```cpp
// Create coefficients for the desired filter
auto cutCoefficients = juce::dsp::FilterDesign<float>::designIIRHighpassHighOrderButterworthMethod(
    chainSettings.lowCutFreq,
    sampleRate,
    (chainSettings.lowCutSlope + 1) * 2
);

// Get the filter chain
auto& leftLowCut = leftChain.get<ChainPositions::LowCut>();

// Set bypass states based on slope
leftLowCut.setBypassed<0>(true);
leftLowCut.setBypassed<1>(true);
leftLowCut.setBypassed<2>(true);
leftLowCut.setBypassed<3>(true);

// Enable the appropriate filters
switch(chainSettings.lowCutSlope)
{
    case Slope_48:
        *leftLowCut.get<3>().coefficients = *cutCoefficients[3];
        leftLowCut.setBypassed<3>(false);
        // Fall through to enable lower stages
    case Slope_36:
        *leftLowCut.get<2>().coefficients = *cutCoefficients[2];
        leftLowCut.setBypassed<2>(false);
        // Continue for other slopes...
}
```

The cut slope options (12, 24, 36, 48 dB/Oct) determine how many filters to enable.

### 6. Audio Processing

```cpp
void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override
{
    // Wrap buffer in a DSP-compatible format
    juce::dsp::AudioBlock<float> block(buffer);
    
    // Extract individual channels
    auto leftBlock = block.getSingleChannelBlock(0);
    auto rightBlock = block.getSingleChannelBlock(1);
    
    // Create process contexts
    juce::dsp::ProcessContextReplacing<float> leftContext(leftBlock);
    juce::dsp::ProcessContextReplacing<float> rightContext(rightBlock);
    
    // Process each channel
    leftChain.process(leftContext);
    rightChain.process(rightContext);
}
```

The processing flow follows: `processChain(ProcessContext(block → buffer))`.

### 7. State Storage and Restoration

```cpp
void getStateInformation(juce::MemoryBlock& destData) override
{
    juce::MemoryOutputStream mos(destData, true);
    apvts.state.writeToStream(mos);
}

void setStateInformation(const void* data, int sizeInBytes) override
{
    auto tree = juce::ValueTree::readFromData(data, sizeInBytes);
    if (tree.isValid())
    {
        apvts.replaceState(tree);
        updateFilters();
    }
}
```

The state is saved/loaded using JUCE's serialization methods: 
`Plugin Code → MemoryOutputStream → MemoryBlock → Host DAW`

## GUI IMPLEMENTATION

### 1. Editor Component Setup

The plugin editor is a `Component` class that inherits methods like `paint()`, `resized()`, and `getLocalBounds()`:

```cpp
class AudioPluginAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    AudioPluginAudioProcessorEditor(AudioPluginAudioProcessor& p)
        : AudioProcessorEditor(&p), processorRef(p)
    {
        setSize(600, 400);
        
        // Make all components visible
        for (auto* comp : getComps())
            addAndMakeVisible(comp);
    }
    
    // Other methods...
};
```

### 2. Custom Slider Implementation

We create a `CustomRotarySlider` that extends `juce::Slider`:

```cpp
struct CustomRotarySlider : juce::Slider 
{
    CustomRotarySlider() 
        : juce::Slider(
            juce::Slider::SliderStyle::RotaryHorizontalVerticalDrag,
            juce::Slider::TextEntryBoxPosition::NoTextBox
          )
    {
        // Custom initialization
    }
};
```

This creates a circular knob with no text box, which is ideal for audio plugin parameters.

### 3. Layout and Positioning

In the `resized()` method:

```cpp
void resized() override
{
    auto bounds = getLocalBounds();
    
    // Reserve top third for response curve
    auto responseArea = bounds.removeFromTop(static_cast<int>(bounds.getHeight() * 0.33));
    responseCurveComponent.setBounds(responseArea);
    
    // Divide remaining area for controls
    auto lowCutArea = bounds.removeFromLeft(static_cast<int>(bounds.getWidth() * 0.33));
    auto highCutArea = bounds.removeFromRight(static_cast<int>(bounds.getWidth() * 0.5));
    
    // Position individual sliders
    lowCutFreqSlider.setBounds(lowCutArea.removeFromTop(static_cast<int>(lowCutArea.getHeight() * 0.5)));
    lowCutSlopeSlider.setBounds(lowCutArea);
    // More sliders...
}
```

We use `getBounds()` to get a rectangle representing our component dimensions, then use `removeFromTop()`, `removeFromLeft()`, etc. to carve out spaces for each UI element.

### 4. Parameter Attachments

We connect each slider to its parameter using `SliderAttachment`:

```cpp
AudioPluginAudioProcessorEditor(AudioPluginAudioProcessor& p)
    : AudioProcessorEditor(&p), 
      processorRef(p),
      // Initialize sliders with parameters
      peakFreqSlider(*processorRef.apvts.getParameter("Peak Freq"), "Hz"),
      peakGainSlider(*processorRef.apvts.getParameter("Peak Gain"), "dB"),
      // More sliders...
      
      // Create attachments
      peakFreqSliderAttachment(processorRef.apvts, "Peak Freq", peakFreqSlider),
      peakGainSliderAttachment(processorRef.apvts, "Peak Gain", peakGainSlider)
      // More attachments...
{
    // Rest of constructor...
}
```

## RESPONSE CURVE VISUALIZATION

### 1. Component Structure

The `ResponseCurveComponent` inherits from:
- `juce::Component` for UI rendering
- `juce::AudioProcessorParameter::Listener` to receive parameter changes 
- `juce::Timer` for efficient UI updates

```cpp
struct ResponseCurveComponent : juce::Component,
                               juce::AudioProcessorParameter::Listener,
                               juce::Timer
{
    // Implementation...
};
```

### 2. Parameter Change Monitoring

In the constructor:

```cpp
ResponseCurveComponent(AudioPluginAudioProcessor& p)
    : processorRef(p)
{
    // Register as listener for all parameters
    const auto& params = processorRef.getParameters();
    for (auto param : params)
        param->addListener(this);
    
    // Start timer for UI updates (60 frames per second)
    startTimerHz(60);
}
```

In the parameter change handler:

```cpp
void parameterValueChanged(int parameterIndex, float newValue) override
{
    // Set flag for pending update
    parametersChanged.set(true);
}
```

### 3. Timer-Based Updates

```cpp
void timerCallback() override
{
    // Only update if parameters have changed
    if (parametersChanged.compareAndSetBool(false, true))
    {
        // Update filter coefficients
        auto chainSettings = getChainSettings(processorRef.apvts);
        auto peakCoefficients = makePeakFilter(chainSettings, processorRef.getSampleRate());
        updateCoefficients(monoChain.get<ChainPositions::Peak>().coefficients, peakCoefficients);
        
        // Update other filters...
        
        // Request redraw
        repaint();
    }
}
```

### 4. Response Curve Drawing

```cpp
void paint(juce::Graphics& g) override
{
    // Set background
    g.fillAll(juce::Colours::black);
    
    auto responseArea = getLocalBounds();
    auto w = responseArea.getWidth();
    
    // Calculate magnitude response across frequency spectrum
    std::vector<double> mags;
    mags.resize(w);
    
    for (int i = 0; i < w; ++i)
    {
        double mag = 1.0;
        // Map pixel position to frequency (logarithmic)
        auto freq = juce::mapToLog10(double(i) / double(w), 20.0, 20000.0);
        
        // Apply each filter's response
        if (!monoChain.isBypassed<ChainPositions::Peak>())
            mag *= monoChain.get<ChainPositions::Peak>().coefficients->getMagnitudeForFrequency(freq, processorRef.getSampleRate());
        
        // Apply other filters...
        
        // Convert gain to decibels
        mags[i] = juce::Decibels::gainToDecibels(mag);
    }
    
    // Drawing setup
    const double outputMin = responseArea.getBottom();
    const double outputMax = responseArea.getY();
    
    // Mapping function from decibels to pixels
    auto map = [outputMin, outputMax](double input) {
        return juce::jmap(input, -24.0, 24.0, outputMin, outputMax);
    };
    
    // Create path for the curve
    juce::Path responseCurve;
    
    // Start path at leftmost point
    responseCurve.startNewSubPath(
        responseArea.getX(), 
        map(mags.front())
    );
    
    // Add line segments for each frequency point
    for (int i = 1; i < w; ++i)
    {
        responseCurve.lineTo(
            responseArea.getX() + i, 
            map(mags[i])
        );
    }
    
    // Draw container
    g.setColour(juce::Colours::orange);
    g.drawRoundedRectangle(responseArea.toFloat(), 4.f, 1.f);
    
    // Draw response curve
    g.setColour(juce::Colours::white);
    g.strokePath(responseCurve, juce::PathStrokeType(2.f));
}
```

## UI REFINEMENT AND OPTIMIZATION

### 1. Custom Rotary Slider Labels

Our `RotarySliderWithLabels` displays min/max values around the slider:

```cpp
void paint(juce::Graphics& g) override
{
    // Define rotation angles (225° arc)
    auto startAng = juce::degreesToRadians(180.f + 45.f); 
    auto endAng = juce::degreesToRadians(180.f - 45.f) + juce::MathConstants<float>::twoPi;
    
    // Get slider geometry
    auto sliderBounds = getSliderBounds();
    auto center = sliderBounds.toFloat().getCentre();
    auto radius = sliderBounds.getWidth() * 0.5f;
    
    // Draw labels around the circumference
    for (int i = 0; i < labels.size(); ++i)
    {
        auto pos = labels[i].pos;  // Normalized position (0-1)
        
        // Convert to angle
        auto ang = juce::jmap(pos, 0.f, 1.f, startAng, endAng);
        
        // Calculate point on circumference
        auto c = center.getPointOnCircumference(
            radius + getTextHeight() * 0.8f,  // Distance from edge
            ang
        );
        
        // Measure text using GlyphArrangement for accuracy
        juce::Rectangle<float> r;
        auto str = labels[i].label;
        
        juce::GlyphArrangement glyphs;
        glyphs.addFittedText(
            g.getCurrentFont(), 
            str, 
            0.0f, 0.0f, 
            200.0f, 
            static_cast<float>(getTextHeight()), 
            juce::Justification::left, 
            1
        );
        auto textWidth = glyphs.getBoundingBox(0, -1, true).getWidth();
        
        // Optimize text width for compact display
        textWidth *= 0.7f;
        
        // Position text rectangle
        r.setSize(textWidth, static_cast<float>(getTextHeight()));
        r.setCentre(c);
        
        // Adjust vertical position based on angle
        if (ang > juce::MathConstants<float>::pi * 1.5f && 
            ang < juce::MathConstants<float>::pi * 2.5f)
            r.setY(r.getY() + getTextHeight());  // Bottom half
        else
            r.setY(r.getY() - getTextHeight() * 0.5f);  // Top half
        
        // Draw the text
        g.setColour(juce::Colours::white);
        g.drawFittedText(str, r.toNearestInt(), juce::Justification::centred, 1);
    }
}
```

### 2. Slider Bounds Calculation

We centralize the sizing and positioning logic:

```cpp
juce::Rectangle<int> getSliderBounds() const
{
    auto bounds = getLocalBounds();
    
    // Determine size based on available space
    auto size = juce::jmin(bounds.getWidth(), bounds.getHeight());
    
    // Reserve space for labels
    size -= getTextHeight() * 2.5;
    
    // Create centered square
    juce::Rectangle<int> r;
    r.setSize(size, size);
    r.setCentre(bounds.getCentreX(), bounds.getCentreY());
    
    // Fine-tune vertical position
    r.setY(r.getY() - getTextHeight() / 2);
    
    return r;
}
```

### 3. Debugging Techniques

We use visual debugging to verify layouts:

```cpp
// Show component boundaries
g.setColour(juce::Colours::red);
g.drawRect(getLocalBounds());

// Log measurements
DBG("textWidth: " + juce::String(textWidth));
```

## BUILD WORKFLOW AND TESTING

### Efficient Development Cycle

```bash
# Save changes and build
CTRL + S -> cmake --build vs-build --config Debug 

# Run with plugin host for testing
cmake --build .\AudioFilePlayer\build\ --target run_with_plugin
```

### AudioFilePlayer Integration

The AudioFilePlayer utility helps with testing:
- Provides consistent audio input
- Allows testing with pre-recorded files
- Enables verification of DSP behavior

---

*TODO: Create a single-build configuration for the project, PluginHost, and AudioFilePlayer with intelligent host-preset handling.*
