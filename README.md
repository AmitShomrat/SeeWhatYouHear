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

<---------------------------------------------SIMPLE_EQ--------------------------------------------->
PARAMETERS:
1. We starts the app by defining a AudioProcessorValueTreeState inside our 'PluginProcessor.h' This class contains a ValueTree that is used to manage an AudioProcessor's entire state. each APVTS should be attached to only one processor.

We sets its default parameters which one of them expecting of a parameters layout, A class to contain a set of RangedAudioParameters and AudioProcessorParameterGroups containing RangedAudioParameters.
thus declare a function that retrieves a parameterLayout ( createParameterLayout() ).


3. AudioParameterFloat, subclass of AudioProcessorParmeter, which is used among sliders and adjustable over a wide a range of values. implementation of 'createParameterLayout()' is simply layoutObj.add(AudioProcessorParmeter) one by one we have to normalisableRange<float> each param to set the Start, end range (e.g a slider that represents a range from 20hz to 20000hz ), its intervalValue as steps for the increasing/decreasing movements finally defining the skew ( e.g 70% of the slider could represent the half of the range and the 30% remain the next half ). 


DSP:
1. Since it is a Stereo plugin ( has 2 channels ) each signal processing ( class dsp ) affect the process over a single channel (mono), unless it declared as a stereo on the documentation. It means that we have to duplicate the processors in order to assign them for both channels.


2. Within 'PluginProcessor.h' the 'using' keyword is for aliasing types of objects and give them simple reference name (e.g a juce::dsp::IIR::Filter<float> to Filter ).

ProcessorChain<p1,p2, ... > for complex processors (e.g CutFilter's are complexed they have 4 options of cut 12,24,36,48 db/Oct, using 4 Filters).

Complex MonoChain is a CutFilter, Filter, CutFilter corresponding to (LowCut, Peak, HighCut).
Declare two of these (Left/right) Processors. 


DEFINITION: 
Prepare to Playback In audio processing, "playback" refers to the actual process of playing or processing audio in real-time. When we say "prepare for playback", it means setting up all the necessary components before audio processing begins, such as, sample rate (how many audio samples per second, e.g., 44.1kHz), Setting the block size (how many samples to process at once), Allocating memory for buffers
Initializing filters and other processors, Setting up internal states of audio processors.

prepareToPlay(double sampleRate, int samplesPerBlock) used for that matter, 'ProcessSpec' of the dsp class has all of the definitions of "Preparing" the Process later ProcessChain.prepare(spec).


3. A processingChain needs a Process_Context s.t the signal flows through each Processor member (Filters).
( I have'nt realized why is he defining these out of our 'AudioPluginAudioProcessor' class ) ->
'struct' Obj_name used for defining a data structure called 'ChainSettings' which contains all of the params actual values and a getChainSettings(juce::AudioProcessorValueTreeState& apvts) that retrieves a ChainSettings.
Each parameter is assigned to the settings using apvts.getRawParameterValue(Param_stringREF) which returns a smart pointer to the value of the parameter ( NOT the normalized but the TRUE ). that smart pointer has a load() function which is Thread_Safe way to acquire Parameter value (Multiple threads asking this).


4. Setting MonoChain coefficients:
first thing first use the getChainSettings(apvts) retrieves the current state of the sliders (parameters).
'auto' obj_name is for defining the type of an object based on the retrieved value has few advantages.

Peak coefficients - A peak defined by its peakFreq, peakQuality and peakGain. so we need to declare a peakCoefficients using juce::dsp::IIR::Coefficients<float>::makePeakFilter (pass the sample rate and peak values out of our ChainSettings) the peakGain converted to decibels using juce::Decibels::decibelsToGain(peakGain). 
Use a enum ChainPositions {LowCut, Peak, HighCut} declared ahead inside 'PluginProcessor.h' in order to assign peakCoefficients to our left/right MonoChains [ The structure is get function of ProcessChain that retrieves the desired Filter in that case 'peak' and access coefficients field both makePeakFilter and get allocate the coefficients over the heap so we need to dereference them for the assignment]

  *leftChain.get<ChainPositions::Peak>().coefficients = *peakCoefficients;
  *rightChain.get<ChainPositions::Peak>().coefficients = *peakCoefficients;

Setting the LowCut/HighCut filter coefficients - The choice of cut slope is dependant by its order s.t 12 db/oct is using a single filter, 24 db/oct using two filters ( the previous and the next to it ) and so on.. the juce::dsp::FilterDesign<float>::designIIRHighpassHighOrderButterworthMethod(Freq, sampleRate, order ) retrives array of IIR::Cofficient objects one for each order = 2. Since we have 4 choices (0, 1, 2, 3) we need to add 1 and multiply by 2 to get the right orders (2, 4, 6, 8).
as we did with the peak; assign a reference to the get 'LowCut' function of the MonoChain, and then setBypassed all 4 'Filters' of the 'CutFilter' by passing the position of it in the Chain. finaly a switch with chainSettings.lowCutSlope will define the choice of the user and respond by setBypassed the right Filters and assign their coeficients to our LowCut Processor. ( Duplications, in advance refactoring )


5. PROCESS - CONTEXT: 
DEFINITION: 
In JUCE audio plugin development, processBlock is a crucial virtual method that every AudioProcessor subclass must implement. It's where the actual audio processing happens for each block of audio that passes through your plugin.

processBlock - Simply after the cofficients has defined we have to process them :) to do so we wrapping the AudioBuffer& with 'AudiBlock', a dsp class, in order to get left/right blocks using getSingleChannelBlock (#Channel_Number) which correspond to 0,1 respectively. next, create a ProcessContextReplacing<float>(block_obj), finally, invoke 
processChain.process(context_obj), for both channels.
SUMMARIZE CONVENTION processChain ( ProcessContext ( block -> buffer ) )  .


STORING AND RESTORING A VALUE STATES:
1. The function getStateInformation(juce::MemoryBlock& destData) used by the host (DAW) to store the current plugin state s.t it will restoe them between sessions. by constructing an MemoryOuputStream and pass it the empty memoryBlock which was provided by the host as well. then we will invoke the function writeToStream by the state ValueTree attribute of our apvts practically saves the current state.

2. Similarly the function setStateInformation(const void* data, int sizeInBytes) invoked by the host when the user is restoring the project, pass a pointer to the binary stored data. Create a treeValue obj to replace it with the default apvts state att, tree.isValid() is use to check the format.. and then updateFilters() resulting the same state of the plugin. 

(Your Plugin Code → MemoryOutputStream → MemoryBlock → Host DAW)
 
GUI:
We are about to connect our parameters to a the GUI sliders for this part will use the stand_alone target instead of the host to confirm the positions and look of our plugin we will change the createEditor function to return new AudioPluginAudioProcessorEditor(*this) instead of a generic as we used before.
1. Go to the pluginEditor.cpp and inside of the constructor set the size with setSize(600, 400) to get a bigger window, declare of a new struct class called CustomRotarySlider inside pluginEditor.h:

struct CustomRotarySlider : juce::Slider {
  CustomRotarySlider() : juce::Slider(juce::Slider::SliderStyle::RotaryHorizontalVerticalDrag,
  juce::Slider::TextEntryBoxPosition::NoTextBox)
  {

  }

CustomRotarySlider inherits from JUCE's Slider class, allowing you to customize its appearance and behavior.
Sets Slider Style: In the constructor, it initializes the base Slider with:
SliderStyle::RotaryHorizontalVerticalDrag: Creates a circular knob that users can drag in both horizontal and vertical directions
TextEntryBoxPosition::NoTextBox: Removes the default text box that would show numerical values
Custom UI Element: This creates specialized rotary knobs commonly used in audio plugins for parameters like:
Frequency controls
Gain/volume adjustments
Q/resonance settings
Filter slope selection
The empty constructor body { } means you haven't added any custom behavior yet, but you could extend this class to add custom drawing, tooltips, or other UI enhancements.
This is a standard approach in JUCE audio plugin development to create specialized UI controls that match the conventions of professional audio software.

2. Declare private slides for each of our parameters from the type of CustomRotarySlider. A private function that returns a vector of juce::components* pointers, simply returns references of our sliders objects we've just declared, we are using it to pass each slider to the addAndMakeVisible(comp) function inside of our editor constructor.

3. Thus the sliders are visible and we able to position their lay out. This is heppening inside of the resized funcion:
  First, we will use the bounds = getLocalBounds(); this function returns a Rectangle<int> obj that represents the dimentions we've setted in the constructor ( e.g the start position is (0,0) the right to corner is (0,400) the left down (600,0) and the right down is (600,400) ).
  The concept is to first edit the bounds and then set them for each slider.. the responseArea is a placeHolder 1/3 from the top (later used for the analyzer) auto responseArea = bounds.removeFromTop(static_cast<int>(bounds.getHeight() * 0.33)) this a Rectangle<int> positioned on the top 33% from the top of bounds dimention its importent to noitce that the left size of bounds now is 2/3, because it is preserving the proportions, now if we will take a 0.5 from the hight of bounds it will take a half size from the remaining 2/3 and so on ..

4. Attachments - A SliderAttachment (formally juce::AudioProcessorValueTreeState::SliderAttachment) is a specialized class in JUCE that creates and manages a connection between:
- A UI element (a Slider component)
- An underlying parameter in your audio plugin (stored in the AudioProcessorValueTreeState)
We each parameter an attachment and initialize them in the initilize line (editor constructor). 

5. Our next goal is to display the response curve of our filters, to do so, we need to give the editor its own instance of monoChain to do that we need to make all the stuff that defines MonoChain public ( move its using stuff and the enam outside of the class, within the processor ) and define MonoChain monoChain a private member of pluginEditor.
Nevigate paint function inside PluginEditor.cpp








MODIFIED YOUR SOURCE ? CTRL + S -> cmake --build vs-build --config Debug -> cmake --build .\vs-build\ --target debug_with_host

MAN Examples:
git --help 
cmake /?

GIT BASICS:
git add [target files changes] - indexing or staging the changes one step from commit.
git reset [target files changes] - Unstaged a file from index. (opposite of git add). 
git commit -m "commit massege"
git log - discover all commits and their branch.
git diff - discover all changes from your last commit.



Create a simple configuration that builds the project, PluginHost and AudioFilePlayer + manage debug_with_host target to work without filtergraph host-preset if isn't exists one else take the last modified file, all of that in a single build and upload the project to git .
