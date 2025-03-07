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

<<MY README>>
1. Few targets we have to learn how to switch between these.
2. We starts the app by defining a AudioProcessorValueTreeState
3. We sets it default parameters and it is expecting to a layout of parameters. 
4. Audio Parameter class float types use with sliders and adjustable over a wide range.
5. Since the plugin is Stereo (has 2 channels) each of the signal processing of the class dsp is set up to process over a single channle (mono) unless it declares as a stereo on the documentation thus it means we have to duplicate the processors in order to assign them for both channels.
6. "Using" key for alias and structure types of complex objects to simple reference name.
7. In order to declare a processingChain for left MonoChannel and left MonoChannel and 
8. A processingChain needs a processing Context to be passed for each member.
Prepare to Playback definition; In audio processing, "playback" refers to the actual process of playing or processing audio in real-time. When we say "prepare for playback", it means setting up all the necessary components before audio processing begins, such as:
Setting the sample rate (how many audio samples per second, e.g., 44.1kHz), Setting the block size (how many samples to process at once), Allocating memory for buffers
Initializing filters and other processors, Setting up internal states of audio processors.




MODIFIED YOUR SOURCE ? CTRL + S -> cmake --build vs-build --config Debug -> cmake --build .\vs-build\ --target debug_with_host