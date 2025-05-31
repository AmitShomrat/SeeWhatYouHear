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

  // FestLED.setMAxRefreshRate(60);
}
// =============================================Preparation functions====================================================

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
// =============================================Mode functions===========================================================
void loop() {
  if (Serial.available()) {
    updateLEDs();
    if (startByte == 0xFF) {
      // // Process left half
      // for (int i = 0; i < halfLeds - 1; i++) {
      //   int rightIndex = i + halfLeds;
      //   leds[i] = CRGB(
      //     RGBLeft.b,  // B This is the Real order DO NOT TOUCH !
      //     RGBLeft.r,  // R
      //     RGBLeft.g   // G
      //   );
      //   leds[i].nscale8(leftBrightness);

      //   // Process right half
        
      //   leds[rightIndex] = CRGB(
      //     RGBRight.b,  // B This is the Real order DO NOT TOUCH !
      //     RGBRight.r,  // R  
      //     RGBRight.g   // G
      //   );
      //   leds[rightIndex].nscale8(rightBrightness);

      // }
      // FastLED.show();
      staticMode();
    }
  }
}