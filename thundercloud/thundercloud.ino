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

// MICROPHONE
int INPUT_MAX = 0;
int INPUT_MIN = 1024;
double THRESHOLD = 0.8; // do not set below 0.5 because of noise floor

// PIXEL STRIPES
int NUM_LEDS = 75;

// MISC
int CLOUD_MODE = 1;   // 0 = All on; 1 = Thunder blue; 2 = Thunder orange; 3 = Party
int CLOUD_MODE_MAX = 4;
int COUNTER = 0; // Counter for decreasing max value
int DECREASE_CHECK = 60000; // Time to check decrease input max value
int LAST_STRIKE = 0;
boolean BUTTON_PRESSED = false;
int BUTTON_RELEASE = 500; // Time to release button before next switch


Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// -------------------------------------------------

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

// -------------------------------------------------

void loop() {
  int microSignal = analogRead(MICRO_PIN);
  boolean buttonInput = digitalRead(BUTTON_PIN);

  COUNTER++;

  // Decrease input max to normalize the max peaks if no light flashes
  // checkForInputMaxDecrease();

  // Check for a button press
  if (buttonInput && BUTTON_PRESSED == false) {
    changeMode();
  }

  // Let's do some magic!
  run(getIntensity(microSignal));

  delay(1); // delay in between reads for stability
}

// -------------------------------------------------

void run(double intensity) {
  
  // All pixels on
  if (CLOUD_MODE == 0) {
    turnAllPixelsOn();
  }

  // Thunder blue mode
  else if (CLOUD_MODE == 1) {
    if (intensity >= THRESHOLD) {
      int color[3] = {255, 255, 255};
      lightningStrike(random(NUM_LEDS), intensity, color);
    }
  }

  // Thunder purple mode
  else if (CLOUD_MODE == 2) {
    if (intensity > THRESHOLD) {
      int color[3] = {255, 69, 0};
      lightningStrike(random(NUM_LEDS), intensity, color);
    }
  }

  // Party mode
  else if (CLOUD_MODE == 3) {
    int color[3] = {random(0, 255), random(0, 255), random(0, 255)};
    double randomIntensity = random(0, 100) / 100.0;
    lightningStrike(random(NUM_LEDS), randomIntensity, color);
  }
}

// -------------------------------------------------

void lightningStrike(int pixel, double intensity, int color[]) {
  LAST_STRIKE = COUNTER;
  
  // create color from intensity
  int rColor = (int)(color[0] * intensity);
  int gColor = (int)(color[1] * intensity);
  int bColor = (int)(color[2] * intensity);

  // light pixel
  strip.setPixelColor(pixel, strip.Color(rColor, gColor, bColor));
  strip.show();

  // let it glow for some time
  delay(random(0, 300));

  turnAllPixelsOff();
}

// -------------------------------------------------

double getIntensity(int microSignal) {
  double peakToPeak = 0;
  double peakToPeakPercent = 0; // peak-to-peak current in percent 0-1

  if (microSignal > INPUT_MAX) {
    INPUT_MAX = microSignal;  // save just the max levels
  }
  else if (microSignal < INPUT_MIN) {
    INPUT_MIN = microSignal;  // save just the min levels
  }

  peakToPeak = INPUT_MAX - INPUT_MIN;  // max - min = peak-peak amplitude
  peakToPeakPercent = (microSignal - INPUT_MIN) / peakToPeak;

  return peakToPeakPercent;
}

// -------------------------------------------------

void changeMode() {
  BUTTON_PRESSED = true;
  CLOUD_MODE++;

  if (CLOUD_MODE >= CLOUD_MODE_MAX) {
    CLOUD_MODE = 0;
  }

  turnAllPixelsOff();
  
  delay(BUTTON_RELEASE);

  BUTTON_PRESSED = false;
}

// -------------------------------------------------


void checkForInputMaxDecrease() {
  // Decrease input max for one fourth if last strike is more than DECREASE_CHECK away
  if (COUNTER % DECREASE_CHECK == 0 && COUNTER - DECREASE_CHECK < LAST_STRIKE && INPUT_MAX > 0) {
    INPUT_MAX = INPUT_MAX - abs(INPUT_MAX / 4);
  }
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
