#include <Adafruit_NeoPixel.h>

// PINS
const int LED_PIN = 6;                  // LED pin on board
const int MICRO_PIN = 7;                // Mircophone pin number on board
const int BUTTON_5V_PIN = 11;           // 5V out pin of the mode button
const int BUTTON_PIN = 12;              // Data pin of the mode button

// MICROPHONE
double THRESHOLD = 0.8;                 // Do not set below 0.5 because of noise floor
const int INPUT_READINGS = 10;          // Number of reading to build an average - Has to be a constant!
int INPUT_MAX[INPUT_READINGS];          // The readings from the analog input
int INPUT_MAX_INDEX = -1;               // The index of the current reading, -1 because is incremented before first read
double INPUT_MAX_AVERAGE = 0;           // The average max input level
int INPUT_MIN = 1024;

// MISC
int CLOUD_MODE = 1;                     // 0 = All on; 1 = Thunder blue; 2 = Thunder orange; 3 = Party
int CLOUD_MODE_MAX = 4;                 // Max number of modes
int NUM_LEDS = 75;                      // Number of LEDs on the pixel stripe
int COUNTER = 0;                        // Counter for decreasing max value
int INPUT_MAX_NORMALIZE_CHECK = 100;    // Time to check decrease input max value
int LAST_STRIKE = 0;                    // Time of last lightning strike
int BUTTON_RELEASE = 500;               // Time to release button before next possible activation
boolean BUTTON_PRESSED = false;         // Bool to prevent multiple button activations

// Create neopixel stripe
// Parameter 1 = number of pixels in strip
// Parameter 2 = pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
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

  // Initialize all the input max readings to 0
  for (int reading = 0; reading < INPUT_READINGS; reading++) {
    INPUT_MAX[reading] = 0;
  }
}

// -------------------------------------------------

void loop() {
  int microSignal = analogRead(MICRO_PIN);
  boolean buttonInput = digitalRead(BUTTON_PIN);

  COUNTER++;

  // Normalize input max average every INPUT_MAX_NORMALIZE_CHECK if there wasn't a strike within the last loops
  if (COUNTER % INPUT_MAX_NORMALIZE_CHECK == 0 && COUNTER - INPUT_MAX_NORMALIZE_CHECK < LAST_STRIKE) {
    normalizeInputMaxAverage();
  }

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
      long color[3] = {255, 255, 255};
      lightningStrike(random(NUM_LEDS), intensity, color);
    }
  }

  // Thunder purple mode
  else if (CLOUD_MODE == 2) {
    if (intensity > THRESHOLD) {
      long color[3] = {255, 69, 0};
      lightningStrike(random(NUM_LEDS), intensity, color);
    }
  }

  // Party mode
  else if (CLOUD_MODE == 3) {
    long color[3] = {random(0, 255), random(0, 255), random(0, 255)};
    double randomIntensity = random(0, 100) / 100.0;
    lightningStrike(random(NUM_LEDS), randomIntensity, color);
  }
}

// -------------------------------------------------

void lightningStrike(int pixel, double intensity, long color[]) {
  LAST_STRIKE = COUNTER;

  // Create color from intensity
  int rColor = (int)(color[0] * intensity);
  int gColor = (int)(color[1] * intensity);
  int bColor = (int)(color[2] * intensity);

  // Light pixel
  strip.setPixelColor(pixel, strip.Color(rColor, gColor, bColor));
  strip.show();

  // Let it glow for some time
  delay(random(0, 300));

  turnAllPixelsOff();
}

// -------------------------------------------------

double getIntensity(int microSignal) {
  double peakToPeak = 0;
  double peakToPeakPercent = 0; // peak-to-peak current in percent 0-1
  double inputMaxSummed = 0;

  if (microSignal > INPUT_MAX_AVERAGE) {

    // Check if array index is greater than max input readings
    if(INPUT_MAX_INDEX >= INPUT_READINGS){
      INPUT_MAX_INDEX = 0;
    }
    else{
      INPUT_MAX_INDEX++;
    }

    // Save micro signal
    INPUT_MAX[INPUT_MAX_INDEX] = microSignal;

    // Create new average input level max
    for (int reading = 0; reading < INPUT_READINGS; reading++) {
      inputMaxSummed = inputMaxSummed + INPUT_MAX[reading];
    }

    // Calculate the average reading
    INPUT_MAX_AVERAGE = inputMaxSummed / INPUT_READINGS;
  }
  else if (microSignal < INPUT_MIN) {
    INPUT_MIN = microSignal;  // save just the min levels
  }

  // Slow down the micro signal on its peak. The high values is includes in the average max readings for the next calculation.
  if(microSignal > INPUT_MAX_AVERAGE){
    microSignal = INPUT_MAX_AVERAGE;
  }

  peakToPeak = INPUT_MAX_AVERAGE - INPUT_MIN;  // max - min = peak-peak amplitude
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

void normalizeInputMaxAverage() {
  int maxReadingIndex = 0;

  for (int reading = 0; reading < INPUT_READINGS; reading++) {
    // Save index of the highest input max reading
    if(INPUT_MAX[reading] > INPUT_MAX[maxReadingIndex]){
      maxReadingIndex = reading;
    }
  }

  // Decline max peak to average
  INPUT_MAX[maxReadingIndex] = INPUT_MAX_AVERAGE;
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