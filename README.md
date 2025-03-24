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

Abstract:
The class pluginEditor is a Component class, thus it inherits Methods such as paint(), resized(), getLocalBounds() and all the layout graphics and drawing related functions.
This concept is crutial to understand, by default nothing will conenct nthe context of the AudioProcessor class, we need to create its own behaveiour which will seems (Fast enough) to be perfect correlated with the processor.

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
Each parameter from our apvts is connected for each slider wwe are performing that as part of the initial line (editor constructor). 




A responseCurve is a line that changing and bending as the filters changed to visualize the Filters action on the current Freq image, or block .. 
5. Our next goal is to display the response curve of our filters, to do so, we need to give the editor its own instance of monoChain we aren't performing a DSP but we need to clone the same behaviour (we need the magnitudes of Freq that correspond each hight fot each pixel that belongs to the width of the responseArea). Need to make all MonoChain definisions a public/"Free" ( move its using stuff and the enam outside of the class, within the processor ) 
Then define MonoChain monoChain used as a private member of pluginEditor.

6. 
Nevigate paint function inside PluginEditor.cpp. since we've allocated the responseArea in the resized now we have to focus on the drawing structure and logic. first thing is to color the background with black colour through g the Graphic pointer then we store a pointer that hold the width of the respone area (use later) also create pointers for each process filter from our monoChai.
The vector mags is used to store the magnitudes. run over all width values with a for loop and compete for each value its corresponding Freq normlized between 20 to 20000 using the mapToLog10(fruction between 0 - 1, startRange, endRange).
at the begining of iter the mag is 1.f means that filter isn't yet affects this current freq.
then we have to check for each filter if not bypassed then computes its cofficient magnitudes and update mag to multiplication by previous mag. do the same for lowcut highcut all slops options. At the end of the for we have to store the gainToDecibeld value inside the mags[i] .

Now we need to edit the Path object (A sequence of lines) the idea is to gives it a starting point and then the point of the next dot corresponding the mags vector that repreenting the hight. 

  const double outputMin = responseArea.getBottom();
  const double outputMax = responseArea.getY();

these are the maximum and minimum the Path can reach. 

The map is an funcional object that needs a value input and returns the corresponding double value for a normelized values between (-24 , 24 our plugins dynamic range this is the mags decibels range) to pixels values. 

With this we will define the start point of Path using the StartNewSubPath () pass (x,y) of the beginning using the left most area responseArea.getX() and the value outcom of map(mags.front()) front() is a vector member function that returns the first object of a vector.

finelly, run over all mags values and define each pixel's hight using lineTo (simply drawing line between dots) using the (x,y) again but adding i value as x and the current y value as map(mags[i]). all of the castings are used as a demend of these types of function.

setColour is used before a drawing. 
draw a rectangle that represent the responseArea by passing a rectangle and settings of corner and thickness
set the line to white and then draw the path using the stroklePath by passing a Path obj and a pathType (float thikcness value).


7. Listener and Timer !
In order to get a real time response curve we need to adjust a mechanism of listeners timers. the concept is to connect each parameter of our process to a listener s.t any parameter modification will make an atomic flag to change its state otherwise the cached values are just fine. 
On the other hand, a timer will check for every period if the flag has changed. 
To do that we just have to inhert this juce classes and implement their pure virtuals functions.
The piece of code of our Component-Listener-Timer object.
First we are adding for each parameter the listener to its listeners lists, getParameters() retrives the list of pointers for all the processor params and we simply assign our Listener-Component using for each and addListener(this).
Then we defin that a Timer with the startTimerHz(60) that will invoke the function timercallback 60 time a second which is conciderd as fast enough to update the responseCurve for Humens.

Noitce that on the destructor we have to unsigned our Listener-Component since his life cycle hase done.
Its the same code but the use of RemoveLisenter(this).
NOW OUR COMPONENT LISTENS TO ALL PARAMS CHANGES .

Implement the funcion that change the Atomic flag, parameterValueChanged (int parameterIndex, float newValue) this is a Listener member pure virtual. winthin this we will use the parameterChanged.set(true) used as an indicator.

Implementing the function timerCallback () the pure virtual of Timer. if the flage is true invoke and set to false; first we have to refactor our processor and create the makePeakFilter makeLowCutFilter makeHighCutFilter a free functions in order to retrive our actual process coefficients. we also  make the updateCoefficients free function as well to update monoChain (of the Component) Cofficient correlated to the processor's monoChain Coefficients, finelly we just have to call the function repaint().

STEPS 6 and 7 ARE related to Components which means we will use it again with the simulation :) 




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
