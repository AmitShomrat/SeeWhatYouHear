#include <FastLED.h>
#include <iostream>

struct LEDColorRGB {
  unsigned char r;
  unsigned char g;
  unsigned char b;
};

LEDColorRGB RGBLeft;
LEDColorRGB RGBRight;

#define DATA_PIN 4
#define NUM_LEDS 300
CRGB leds[NUM_LEDS];

// LEDs spec
// Mode byte + 2 * Brightness byte + 2 * 3 Color byte.
const int expectedBytes = 1 + 2 + 2;
static byte buffer[NUM_LEDS * 3];
const int halfLeds = NUM_LEDS/2;
// LEDs state
int startByte;
unsigned char leftBrightness;
unsigned char rightBrightness;
// =============================================Preparation functions====================================================
void updateLEDs() {
  startByte = Serial.read(); // Expected values 0xFF/0xFE/0xFD/0xFC
  if(startByte == 0xFB) {
    leftBrightness = 0;
    rightBrightness = 0;
    RGBLeft = {0, 0, 0};
    RGBRight = {0, 0, 0};
  } else {
  leftBrightness = Serial.read();
  rightBrightness = Serial.read();
  RGBLeft.r = Serial.read();
  RGBLeft.g = Serial.read();
  RGBLeft.b = Serial.read();
  RGBRight.r = Serial.read();
  RGBRight.g = Serial.read();
  RGBRight.b = Serial.read();
  }
}

void waitAndRead(byte* buffer, int len) {
  int count = 0;
  while (count < len) {
    if (Serial.available()) {
      buffer[count++] = Serial.read();
    }
  }
}

void setup() {
  Serial.begin(115200);
  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
  FastLED.clear();
  FastLED.show();

}
// =============================================Mode functions===========================================================

void staticMode() {
      // Process left half
      for (int i = 0; i < halfLeds - 1; i++) {
        int rightIndex = i + halfLeds;
        leds[i] = CRGB(
          RGBLeft.b,  // B This is the Real order DO NOT TOUCH !
          RGBLeft.r,  // R
          RGBLeft.g   // G
        );
        leds[i].nscale8(leftBrightness);

        // Process right half
        
        leds[rightIndex] = CRGB(
          RGBRight.b,  // B This is the Real order DO NOT TOUCH !
          RGBRight.r,  // R  
          RGBRight.g   // G
        );
        leds[rightIndex].nscale8(rightBrightness);

      }
      FastLED.show();
}

void instantVUMeter() {
    // Map brightness to number of LEDs to light
    int leftTargetLed = map(leftBrightness, 0, 255, 0, halfLeds);
    int rightTargetLed = map(rightBrightness, 0, 255, halfLeds, NUM_LEDS);
    
    // Process left half (0 → 149)
    for(int i = 0; i < halfLeds; i++) {
        if(i < leftTargetLed) {
            leds[i] = CRGB(
                RGBLeft.b,  // B This is the Real order DO NOT TOUCH !
                RGBLeft.r,  // R
                RGBLeft.g   // G
            );
        } else {
            leds[i] = CRGB(0, 0, 0);
        }
    }
    
    // Process right half (150 → 300)
    for(int i = halfLeds; i < NUM_LEDS; i++) {
        if(i < rightTargetLed) {
            leds[i] = CRGB(
                RGBRight.b,  // B This is the Real order DO NOT TOUCH !
                RGBRight.r,  // R
                RGBRight.g   // G
            );
        } else {
            leds[i] = CRGB(0, 0, 0);
        }
    }
    
    FastLED.show();
}

void waveMode() {
    // Create wave position using sine wave
    static uint8_t wavePosition = 0;
    
    // Process left half (0 → 149)
    for(int i = 0; i < halfLeds; i++) {
        // Calculate wave intensity for this LED
        uint8_t intensity = sin8(wavePosition + (i * 256 / halfLeds));
        // Scale intensity by the master brightness
        intensity = map(intensity, 0, 255, 0, leftBrightness);
        
        leds[i] = CRGB(
            RGBLeft.b,  // B This is the Real order DO NOT TOUCH !
            RGBLeft.r,  // R
            RGBLeft.g   // G
        );
        leds[i].nscale8(intensity);
    }
    
    // Process right half (150 → 300)
    for(int i = halfLeds; i < NUM_LEDS; i++) {
        // Calculate wave intensity for this LED
        uint8_t intensity = sin8(wavePosition + ((i - halfLeds) * 256 / halfLeds));
        // Scale intensity by the master brightness
        intensity = map(intensity, 0, 255, 0, rightBrightness);
        
        leds[i] = CRGB(
            RGBRight.b,  // B This is the Real order DO NOT TOUCH !
            RGBRight.r,  // R
            RGBRight.g   // G
        );
        leds[i].nscale8(intensity);
    }
    
    // Move wave position
    wavePosition += 4; // Adjust speed by changing this value
    
    FastLED.show();
}

void sparkleMode() {
    static uint16_t sparklePos[2] = {0, halfLeds}; // Track sparkle positions for left and right
    static uint8_t fadeRate = 64;  // How quickly the sparkles fade out
    
    // Fade existing LEDs
    for(int i = 0; i < NUM_LEDS; i++) {
        leds[i].nscale8(fadeRate);
    }
    
    // Process left half (0 → 149)
    if(random8() < leftBrightness) { // More sparkles at higher brightness
        sparklePos[0] = random16(halfLeds);
        leds[sparklePos[0]] = CRGB(
            RGBLeft.b,  // B This is the Real order DO NOT TOUCH !
            RGBLeft.r,  // R
            RGBLeft.g   // G
        );
        // Create a small trail
        if(sparklePos[0] > 0) 
            leds[sparklePos[0]-1] = leds[sparklePos[0]].nscale8(128);
        if(sparklePos[0] < halfLeds-1) 
            leds[sparklePos[0]+1] = leds[sparklePos[0]].nscale8(128);
    }
    
    // Process right half (150 → 300)
    if(random8() < rightBrightness) { // More sparkles at higher brightness
        sparklePos[1] = random16(halfLeds) + halfLeds;
        leds[sparklePos[1]] = CRGB(
            RGBRight.b,  // B This is the Real order DO NOT TOUCH !
            RGBRight.r,  // R
            RGBRight.g   // G
        );
        // Create a small trail
        if(sparklePos[1] > halfLeds) 
            leds[sparklePos[1]-1] = leds[sparklePos[1]].nscale8(128);
        if(sparklePos[1] < NUM_LEDS-1) 
            leds[sparklePos[1]+1] = leds[sparklePos[1]].nscale8(128);
    }
    
    FastLED.show();
}
// =============================================loop function===========================================================
void loop() {
  if (Serial.available()) {
    updateLEDs();
    if (startByte == 0) staticMode();
    if (startByte == 1) instantVUMeter();
    if (startByte == 2) waveMode();
    if (startByte == 3) sparkleMode();
  }
}