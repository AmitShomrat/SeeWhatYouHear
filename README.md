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
1. Since it is a Stereo plugin ( has 2 channels ) each signal processing (class dsp) affect the process over a single channel (mono), unless it declared as a stereo on the documentation. It means that we have to duplicate the processors in order to assign them for both channels.


2. Within 'PluginProcessor.h' the 'using' keyword is for aliasing types of objects and give them simple reference name (e.g a juce::dsp::IIR::Filter<float> to Filter ).

ProcessorChain<p1,p2, ... > for complex processors (e.g CutFilter's are complexed they have 4 options of cut 12,24,36,48 db/Oct, using 4 Filters).

Complex MonoChain is a CutFilter, Filter, CutFilter corresponding to (LowCut, Peak, HighCut).
Declare two of these (Left/right) Processors. 


DEFINITION: 
Prepare to Playback In audio processing, "playback" refers to the actual process of playing or processing audio in real-time. When we say "prepare for playback", it means setting up all the necessary components before audio processing begins, such as, sample rate (how many audio samples per second, e.g., 44.1kHz), Setting the block size (how many samples to process at once), Allocating memory for buffers
Initializing filters and other processors, Setting up internal states of audio processors.

prepareToPlay(double sampleRate, int samplesPerBlock) used for that matter, 'ProcessSpec' of the dsp class has all of the definitions of "Preparing" the Process later ProcessChain.prepare(spec).




3. A processingChain needs a Process_Context s.t the signal flows through each Processor member (Filters).
( I don't realize why he is defining these out of our 'AudioPluginAudioProcessor' class )
'struct' Obj_name used for defining a data structure called 'ChainSettings' which contains all of the params actual values and a getChainSettings(juce::AudioProcessorValueTreeState& apvts) that retrieves a ChainSettings.
Each parameter is assigned to the settings using apvts.getRawParameterValue(Param_stringREF) which returns a smart pointer to the value of the parameter ( NOT the normalized but the TRUE ). that smart pointer has a load() function which is Thread_Safe way to acquire Parameter value (Multiple threads asking this).


4. PROCESS - CONTEXT: 
first thing first use the getChainSettings(apvts) retrieves the current state of the sliders (parameters).
'auto' obj_name is for defining the type of an object based on the retrieved value has few advantages.

Peak context - A peak defined by its peakFreq, peakQuality and peakGain. so we need to declare a peakCoefficients using juce::dsp::IIR::Coefficients<float>::makePeakFilter (pass the sample rate and peak values out of our ChainSettings) the peakGain converted to decibels using juce::Decibels::decibelsToGain(peakGain). 
Use a enum ChainPositions {LowCut, Peak, HighCut} declared ahead inside 'PluginProcessor.h' in order to assign peakCoefficients to our left/right MonoChains [ The structure is get function of ProcessChain that retrieves the desired Filter in that case 'peak' and access coefficients field both makePeakFilter and get allocate the coefficients over the heap so we need to dereference them for the assignment]

  *leftChain.get<ChainPositions::Peak>().coefficients = *peakCoefficients;
  *rightChain.get<ChainPositions::Peak>().coefficients = *peakCoefficients;

LowCut/HighCut contexts - ..


DEFINITION: 
In JUCE audio plugin development, processBlock is a crucial virtual method that every AudioProcessor subclass must implement. It's where the actual audio processing happens for each block of audio that passes through your plugin.

processBlock - Simply after the context has defined we have to process them :) to do so we wrapping the AudioBuffer& with 'AudiBlock', a dsp class, in order to get left/right blocks using getSingleChannelBlock (#Channel_Number) which correspond to 0,1 respectively. next, create a ProcessContextReplacing<float>(block_obj), finally, invoke 
processChain.process(context_obj), for both channels.


SUMMARIZE CONVENTION processChain ( ProcessContext ( block -> buffer ) )  .




MODIFIED YOUR SOURCE ? CTRL + S -> cmake --build vs-build --config Debug -> cmake --build .\vs-build\ --target debug_with_host