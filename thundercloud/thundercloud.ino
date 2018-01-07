#include <Adafruit_NeoPixel.h>

// PINS
const int LED_PIN = 6;
const int MICRO_PIN = 7;
const int BUTTON_5V_PIN = 11;
const int BUTTON_PIN = 12;

// DIGITAL VALUES
boolean BUTTON_INPUT = 0;

// ANALOG VALUES
int MICRO_INPUT = 0;

// MISC
int CLOUD_MODE = 1;   // 0 = All on; 1 = Thunder blue; 2 = Thunder orange; 3 = Party
const int NUM_LEDS = 75;
const int CLOUD_MODE_MAX = 4;
const double MICRO_THRESHOLD_MIN = 0.30;
const double MICRO_THRESHOLD_MAX = 0.70;
const double LED_INTENSITY_MIN = 0.30;
const double LED_INTENSITY_MAX = 1.00;
const int sampleWindow = 50;  // Sample window width in mS (50 mS = 20Hz)

// Create neopixel stripe
// Parameter 1 = number of pixels in strip
// Parameter 2 = pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

void setup() {

  // Neopixel setup
  strip.begin();
  strip.show(); // Initialize all pixels to 'off'

  // Microphone
  pinMode(MICRO_PIN, INPUT);
  Serial.begin (9600);

  // Button
  pinMode(BUTTON_PIN, INPUT);
  pinMode(BUTTON_5V_PIN, OUTPUT);
  digitalWrite(BUTTON_5V_PIN, true);
}

void loop() {

  // -----------------------------------------------
  // Toggle cloud mode + reset led strip
  BUTTON_INPUT = digitalRead(BUTTON_PIN);
  if (BUTTON_INPUT) {
    turnAllPixelsOff();
    CLOUD_MODE++;

    if (CLOUD_MODE >= CLOUD_MODE_MAX) {
      CLOUD_MODE = 0;
    }
    delay(500);
  }

  // -----------------------------------------------
  unsigned long startMillis = millis(); // Start of sample window
  unsigned int peakToPeak = 0;   // peak-to-peak level
  double intensity = 0;

  unsigned int signalMax = 0;
  unsigned int signalMin = 1024;

  // collect data for 50 mS
  while (millis() - startMillis < sampleWindow) {
    MICRO_INPUT = analogRead(MICRO_PIN);
    if (MICRO_INPUT < 1024) {  // toss out spurious readings
      if (MICRO_INPUT > signalMax) {
        signalMax = MICRO_INPUT;  // save just the max levels
      }
      else if (MICRO_INPUT < signalMin) {
        signalMin = MICRO_INPUT;  // save just the min levels
      }
    }
  }
  peakToPeak = signalMax - signalMin;  // max - min = peak-peak amplitude
  intensity = ((peakToPeak * 5.0) / 1024);  // convert to volts

   // -----------------------------------------------

  getItOn(intensity);
}

 // -----------------------------------------------

void getItOn(double intensity){

  // -----------------------------------------------
  // ALL ON
  if (CLOUD_MODE == 0) {
    turnAllPixelsOn();
  }

  // -----------------------------------------------
  // THUNDER BLUE = 1 / ORANGE = 2
  if (CLOUD_MODE == 1 || CLOUD_MODE == 2) {
      Serial.print(intensity, 3);
      Serial.print(" - ");
      Serial.print(intensity > MICRO_THRESHOLD_MIN);
      Serial.print(" - ");
      Serial.println(MICRO_THRESHOLD_MIN);

      if (intensity > MICRO_THRESHOLD_MIN) {
        int led = random(NUM_LEDS);

        for (int i = 0; i < 10; i++) {
          lightningStrike(random(NUM_LEDS), intensity);
        }
      }
  }

  // -----------------------------------------------
  // PARTY MODE
  if (CLOUD_MODE == 3) {
    lightningStrike(random(NUM_LEDS), random(0, 1));
  }

  delay(150);
}

// -------------------------------------------------

void lightningStrike(int pixel, double intensity) {

  int rColor = 255;
  int gColor = 255;
  int bColor = 255;

  if (CLOUD_MODE == 2) {
    rColor = 255;
    gColor = 69;
    bColor = 0;
  }

  if (CLOUD_MODE == 3) {
    rColor = random(0, 255);
    gColor = random(0, 255);
    bColor = random(0, 255);
  }

  double tmpMax = MICRO_THRESHOLD_MAX - MICRO_THRESHOLD_MIN;
  double tmpCur = intensity - MICRO_THRESHOLD_MIN;
  double percent = 0 / tmpMax;
  double tmpLedMax = LED_INTENSITY_MAX - LED_INTENSITY_MIN;
  double newLedIntensity = (tmpLedMax * percent) + LED_INTENSITY_MIN;

  int newRColor = (int)(rColor * newLedIntensity);
  int newGColor = (int)(gColor * newLedIntensity);
  int newBColor = (int)(bColor * newLedIntensity);

  strip.setPixelColor(pixel, strip.Color(newRColor, newGColor, newBColor));
  strip.show();
  // delay(random(100, 1000));

  turnAllPixelsOff();
}

// -------------------------------------------------

void turnAllPixelsOn() {
  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, 255, 255, 255);
  }
  strip.show();
}

// -------------------------------------------------

void turnAllPixelsOff() {
  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, 0);
  }
  strip.show();
}

