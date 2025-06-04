# Audio Visualization Plugin

## Overview
This audio plugin provides real-time visualization of audio signals through WS2812B LED strips, combining FFT analysis with color decision making. The plugin processes stereo audio input and generates corresponding visual outputs for each channel.

## Features
- Real-time FFT analysis of audio signals
- Independent processing of left and right channels
- Dynamic color decision making based on spectral analysis
- Multiple LED visualization modes
- Response curve visualization
- Configurable brightness control

## LED Visualization Modes

### 1. Static Mode
Basic static lighting mode where both LED strips maintain constant colors and brightness.
- Left strip (0-149): Controlled by RGBLeft and leftBrightness
- Right strip (150-300): Controlled by RGBRight and rightBrightness
- Colors remain solid and stable
- Perfect for ambient lighting or testing

### 2. VU Meter Mode
Professional audio level meter visualization.
- Instantly responds to audio levels
- Left channel: LEDs light up from 0→149 based on intensity
- Right channel: LEDs light up from 150→300 based on intensity
- Higher audio levels = more LEDs illuminate
- Perfect for real-time audio monitoring

### 3. Wave Mode
Smooth, flowing wave animation through both LED strips.
- Creates a continuous sine wave pattern
- Separate colors for left and right channels
- Wave intensity controlled by brightness levels
- Smooth transitions and fluid movement
- Ideal for ambient visualization

### 4. Fire Sparkle Mode
Dynamic sparkling effect with trailing glow.
- Random LED sparkles with color trails
- Brightness controls sparkle frequency
- Separate left/right color effects
- Fade-out animation for smooth transitions
- Creates an energetic, music-responsive display

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
   - 300 LEDs total (150 per channel)
   - BRG color order (Important!)
   - Serial communication at 115200 baud
   - Brightness control range: 0-255
   - Independent left/right channel control
   - Multiple visualization modes with real-time switching

### Color Order Note
Important: The LED color order is BRG (Blue, Red, Green). This must be maintained for proper color reproduction:
```cpp
CRGB(
    value.b,  // Blue
    value.r,  // Red
    value.g   // Green
)
```

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
   
   # Install Required Arduino Libraries
   1. Open Arduino IDE
   2. Go to Tools > Manage Libraries
   3. Search for and install "FastLED"
   
   # Note: Arduino libraries are not included in the repository
   # They are listed in .gitignore and must be installed separately
   
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
   - Select desired visualization mode
   - Adjust brightness as needed
   - Switch between modes in real-time

3. **Mode Selection**
   - Static Mode: For testing and ambient lighting
   - VU Meter: For accurate audio level monitoring
   - Wave Mode: For smooth, flowing visualizations
   - Fire Sparkle: For dynamic, energetic displays

4. **Troubleshooting**
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
