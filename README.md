# Audio Visualization Plugin

## Overview
This audio plugin provides real-time visualization of audio signals through WS2812B LED strips, combining FFT analysis with color decision making. The plugin processes stereo audio input and generates corresponding visual outputs for each channel.

## Features
- Real-time FFT analysis of audio signals
- Independent processing of left and right channels
- Dynamic color decision making based on spectral analysis
- LED strip control for visual output
- Response curve visualization
- Configurable brightness control

## Technical Details

### Audio Processing Pipeline
1. **Input Processing**
   - Stereo audio input (2 channels)
   - Independent channel level detection
   - Sample rate: 44.1kHz/48kHz
   - Buffer size: 2048 samples

2. **FFT Analysis**
   - FFT size: 2048 points
   - Window type: Blackman-Harris
   - Update rate: 120Hz
   - Spectral resolution: ~21.5Hz per bin
   - Frequency range: 20Hz - 20kHz

3. **Spectral Analysis**
   - Linear magnitude calculation
   - Band energy analysis:
     - Low band: 20Hz - 200Hz
     - Mid band: 200Hz - 2kHz
     - High band: 2kHz - 10kHz
   - Spectral centroid calculation
   - Spectral spread analysis
   - Spectral flux measurement

4. **Color Decision System**
   - Feature extraction from FFT data
   - Energy-based color mapping
   - Spectral centroid influence
   - Temporal smoothing
   - Independent processing for each channel

5. **LED Control**
   - WS2812B LED strip support
   - 300 LEDs per strip
   - GRB color order
   - Serial communication at 115200 baud
   - Brightness control range: 0-255

### Threading Architecture
- Main audio thread: Real-time audio processing
- FFT threads: Independent processing for each channel
- LED communication thread: Serial data transmission
- Thread synchronization using atomic operations

## Setup Instructions

### Development Environment
1. **Required Software**
   - Visual Studio 2022
   - JUCE Framework
   - Arduino IDE
   - Git

2. **JUCE Setup**
   ```bash
   # Clone JUCE
   git clone https://github.com/juce-framework/JUCE.git
   
   # Set up Projucer
   # Configure paths in Visual Studio
   ```

3. **Project Setup**
   ```bash
   # Clone the repository
   git clone https://github.com/AmitShomrat/You-see-what-you-hear.git
   
   # Open in Visual Studio
   # Build the solution
   ```

### Hardware Setup
1. **LED Strip Configuration**
   - WS2812B LED strip
   - 300 LEDs per strip
   - 5V power supply
   - Data pin: 4 (configurable)

2. **Arduino Setup**
   ```cpp
   // Required libraries
   #include <FastLED.h>
   
   // Configuration
   #define DATA_PIN 4
   #define NUM_LEDS 300
   #define BRIGHTNESS 255
   ```

3. **Serial Communication**
   - Baud rate: 115200
   - Data format: 8N1
   - Buffer size: 300 * 3 + 3 bytes

### Plugin Installation
1. **Windows**
   - Build the solution in Visual Studio
   - Copy the .dll to your VST3 folder
   - Restart your DAW

2. **Plugin Configuration**
   - Set COM port in plugin settings
   - Configure LED strip parameters
   - Adjust brightness control

### Usage
1. **DAW Integration**
   - Load the plugin on a stereo track
   - Ensure proper routing
   - Monitor CPU usage

2. **LED Control**
   - Connect LED strips
   - Verify serial communication
   - Adjust brightness as needed

3. **Troubleshooting**
   - Check COM port settings
   - Verify LED strip connections
   - Monitor CPU usage
   - Check buffer settings

## Credits
- Original infrastructure based on MatKat Music's SimpleEQ
- JUCE Framework for audio processing
- FastLED library for LED control

## Acknowledgments
Special thanks to [@matkatmusic](https://github.com/matkatmusic) for providing the SimpleEQ infrastructure that served as the foundation for this project.
