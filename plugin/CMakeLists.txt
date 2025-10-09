cmake_minimum_required(VERSION 3.22)

# Version is needed by JUCE.
project(SeeWhatYouHear VERSION 0.1.0)

# --------------------------------------ChatGPT suggestions
# Prefer Boost package config (vcpkg/modern distros). Fallback to legacy FindBoost if needed.
set(Boost_USE_STATIC_LIBS        OFF)
set(Boost_USE_MULTITHREADED      ON)
set(Boost_USE_STATIC_RUNTIME     OFF)

# Try CONFIG mode (provides Boost::headers, Boost::system targets)
find_package(Boost 1.70 CONFIG QUIET COMPONENTS headers system)

if(NOT Boost_FOUND)
  # Fallback to legacy FindBoost (MODULE). CMake 3.31+ deprecates this, but we use it only if CONFIG isn't available.
  find_package(Boost 1.70 REQUIRED COMPONENTS system)  # 'headers' isn't a component in MODULE mode
  set(FOUND_LEGACY_BOOST_MODULE TRUE)
endif()

# Link/include depending on what we found
if(TARGET Boost::headers OR TARGET Boost::system)
  target_link_libraries(${PROJECT_NAME} PRIVATE Boost::headers Boost::system)
else()
  # Legacy MODULE mode: use variables exposed by FindBoost
  target_include_directories(${PROJECT_NAME} PRIVATE ${Boost_INCLUDE_DIRS})
  target_link_libraries(${PROJECT_NAME} PRIVATE ${Boost_LIBRARIES})
endif()
# --------------------------------------ChatGPT suggestions



# CMake 3.31 deprecates FindBoost; prefer CONFIG packages
if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()

find_package(Boost REQUIRED)

# Adding a directory with the library/application name as a subfolder of the
# include folder is a good practice. It helps avoid name clashes later on.
set(INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include/SeeWhatYouHear")

# Adds a plugin target (that's basically what the Projucer does).
juce_add_plugin(${PROJECT_NAME}
    COMPANY_NAME ShomratDigitals # change this
    IS_SYNTH FALSE # may change this
    NEEDS_MIDI_INPUT FALSE # may change this
    NEEDS_MIDI_OUTPUT FALSE # may change this
    PLUGIN_MANUFACTURER_CODE MCMP # change this
    PLUGIN_CODE SWYH # change this to match new name
    FORMATS Standalone VST3 # may change this
    PRODUCT_NAME "SeeWhatYouHear" # change this
    VST3_COPY_DIR "$ENV{HOME}/vst3" # Use user's VST3 directory on Linux
    COPY_PLUGIN_AFTER_BUILD $<BOOL:$ENV{CI}>
)

# Sets the source files of the plugin project.
target_sources(${PROJECT_NAME}
    PRIVATE
        source/ColorDecisionML.cpp
        source/FFTProcessor.cpp
        source/LEDCommunication.cpp
        source/PluginEditor.cpp
        source/PluginProcessor.cpp
        ${INCLUDE_DIR}/PluginEditor.h
        ${INCLUDE_DIR}/PluginProcessor.h
        ${INCLUDE_DIR}/LEDCommunication.h
        ${INCLUDE_DIR}/CommonDef.h
        ${INCLUDE_DIR}/FFTProcessor.h
        ${INCLUDE_DIR}/ColorDecisionML.h
)

# Sets the include directories of the plugin project.
target_include_directories(${PROJECT_NAME}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Links to all necessary dependencies. The present ones are recommended by JUCE.
# If you use one of the additional modules, like the DSP module, you need to specify it here.
target_link_libraries(${PROJECT_NAME}
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_dsp
        juce::juce_audio_formats
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)


# Link atomic library only on non-Windows platforms
if(NOT WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE atomic)
endif()

# These definitions are recommended by JUCE.
target_compile_definitions(${PROJECT_NAME}
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

# Enables all warnings and treats warnings as errors.
# This needs to be set up only for your projects, not 3rd party
if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /WX)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()

# In Visual Studio this command provides a nice grouping of source files in "filters".
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/..)
