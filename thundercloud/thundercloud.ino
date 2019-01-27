// https://learn.adafruit.com/adafruit-neopixel-uberguide/arduino-library-use
#include <Adafruit_NeoPixel.h>
// https://www.pjrc.com/teensy/td_libs_AltSoftSerial.html
#include <AltSoftSerial.h>
// https://github.com/NitrofMtl/TimeOut
#include <TimeOut.h>

// -------------------------------------------------

/*
 * Bluetooth signal map
 *
 * 0: dark: everything off, reset thresholds
 * 1: light: everything on
 * 2: color 1: custom color lightning
 * 3: color 2: white lightning
 * 4: color 3: red lightning
 * 5: color 4: green lightning
 * 6: color 5: blue lightning
 * 7: threshold +: increase the threshold
 * 8: threshold -: decrease the threshold
 * 9: party mode
 */

// -------------------------------------------------

const boolean IS_DEBUG = false;

// PINS
const int LED_PIN = 6;                  // LED pin on board
const int MICRO_PIN = 7;                // Microphone pin number on board
const int BUTTON_5V_PIN = 11;           // 5V out pin of the mode button
const int BUTTON_PIN = 12;              // Data pin of the mode button

// MICROPHONE
const int INPUT_READINGS = 10;          // Number of reading to build an average - Has to be a constant!
int VOLTAGE_READINGS[INPUT_READINGS];
int CURRENT_READING;
double THRESHOLD;                       // Volt threshold in percent 0 to 100

// LEDs
const int NUMBER_OF_LEDS = 75;          // Number of LEDs on the pixel stripe
const long LED_COLORS[][3] = {          // Color map
  {0, 0, 0},                            // Placeholder
  {255, 255, 255},                      // White - for all pixels on mode
  {160, 200, 255},                      // Mode 2: custom color
  {255, 255, 255},                      // Mode 3: white
  {255, 0, 0},                          // Mode 4: red
  {0, 255, 0},                          // Mode 5: green
  {0, 0, 255}                           // Mode 6: blue

};
double MAX_INTENSITY;                   // Volt threshold in percent 0 to 100

// BLUETOOTH
char COMMAND;

// MISC
const int CLOUD_MODE_MAX = 9;           // Max number of modes
int CLOUD_MODE;                         // 0 = All on; 1 = Thunder color 1; 2 = Thunder color 2; 3 = Party

Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUMBER_OF_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
AltSoftSerial BTserial;
TimeOut lightningTimeout;
Interval manualLoopInterval;

// -------------------------------------------------

void initialize() {
  THRESHOLD = 10;
  CURRENT_READING = 0;
  MAX_INTENSITY = 0;

  COMMAND = ' ';

  CLOUD_MODE = 1;

  // Set all the input voltage readings to 0
  for (int reading = 0; reading < INPUT_READINGS; reading++) {
    VOLTAGE_READINGS[reading] = 0.00;
  }

  turnAllPixelsOff();
}

// -------------------------------------------------

void setup() {
  // Neopixel setup
  strip.begin();
  strip.show(); // Initialize all pixels to 'off'

  // Microphone
  pinMode(MICRO_PIN, INPUT);
  Serial.begin (9600);

  // Bluetooth
  BTserial.begin(9600);

  // Button
  pinMode(BUTTON_PIN, INPUT);
  pinMode(BUTTON_5V_PIN, OUTPUT);
  digitalWrite(BUTTON_5V_PIN, true);

  // Initialize all values
  initialize();

  // Start a manual loop to prevent blocking the main loop
  manualLoopInterval.interval(50, run);
}

// -------------------------------------------------

void loop() {
  processBluetoothSignals();

  TimeOut::handler();
  Interval::handler();

  measureSoundVoltage();
}

// -------------------------------------------------

void run() {
  double signalIntensity = getVoltageAverage();
  if (signalIntensity > MAX_INTENSITY) {
    MAX_INTENSITY = signalIntensity;
  }
  // Get the ratio between the intensities without the threshold
  double intensityRatio = (THRESHOLD - signalIntensity) / (THRESHOLD - MAX_INTENSITY);

  if (IS_DEBUG) {
    Serial.print("signalIntensity: ");
    Serial.print(signalIntensity);
    Serial.print("; ");
    Serial.print("intensityRatio: ");
    Serial.println(intensityRatio);
  }

  // Dark / reset mode
  if (CLOUD_MODE == 0) {
    initialize();
  }

  // Light mode - turn all pixels on
  else if (CLOUD_MODE == 1) {
    turnAllPixelsOn(LED_COLORS[CLOUD_MODE]);
  }

  // Color modes, color is picked out of the LED_COLORS array
  else if (CLOUD_MODE == 2
    || CLOUD_MODE == 3
    || CLOUD_MODE == 4
    || CLOUD_MODE == 5
    || CLOUD_MODE == 6
  ) {
    if (signalIntensity >= THRESHOLD) {
      lightningStrike(random(NUMBER_OF_LEDS), intensityRatio, LED_COLORS[CLOUD_MODE]);
    }
  }

  else if (CLOUD_MODE == 7) {

  }

  else if (CLOUD_MODE == 8) {

  }

  // Party mode
  else if (CLOUD_MODE == 9) {
    long color[3] = {random(0, 255), random(0, 255), random(0, 255)};
    double randomIntensity = random(0, 100) / 100.0;
    lightningStrike(random(NUMBER_OF_LEDS), randomIntensity, color);
  }
}

// -------------------------------------------------

void processBluetoothSignals() {
  // Read from the Bluetooth module and send to the Arduino Serial Monitor
  if (BTserial.available()) {
    COMMAND = BTserial.read();

    if (IS_DEBUG) {
      Serial.print("bluetooth input: ");
      Serial.write(COMMAND);
    }

    if (COMMAND == '0') { changeMode(0); }
    else if (COMMAND == '1') { changeMode(1); }
    else if (COMMAND == '2') { changeMode(2); }
    else if (COMMAND == '3') { changeMode(3); }
    else if (COMMAND == '4') { changeMode(4); }
    else if (COMMAND == '5') { changeMode(5); }
    else if (COMMAND == '6') { changeMode(6); }
    else if (COMMAND == '7') { changeMode(7); }
    else if (COMMAND == '8') { changeMode(8); }
    else if (COMMAND == '9') { changeMode(9); }
  }
}

// -------------------------------------------------

void measureSoundVoltage() {
  const int sampleWindow = 50;  // Sample window width in mS (50 mS = 20Hz)
  long startMillis = millis();  // Start of sample window
  int peakToPeak = 0;           // peak-to-peak level

  int signalMax = 0;
  int signalMin = 1024;
  int sample;

  // collect data for 50 mS
  while (millis() - startMillis < sampleWindow)
  {
      sample = analogRead(MICRO_PIN);
      if (sample < 1024)  // toss out spurious readings
      {
         if (sample > signalMax)
         {
            signalMax = sample;  // save just the max levels
         }
         else if (sample < signalMin)
         {
            signalMin = sample;  // save just the min levels
         }
      }
  }
  peakToPeak = signalMax - signalMin;  // max - min = peak-peak amplitude
  double volts = (peakToPeak * 5.0) / 1024;  // convert to volts;
  VOLTAGE_READINGS[CURRENT_READING] = (volts / 5) * 100;  // convert to volt percentage

  CURRENT_READING++;

  if (CURRENT_READING > INPUT_READINGS) {
    CURRENT_READING = 0;
  }
}

// -------------------------------------------------

double getVoltageAverage() {
  double average = 0;

  for (int reading = 0; reading < INPUT_READINGS; reading++) {
    average = average + VOLTAGE_READINGS[reading];
  }

  return average / INPUT_READINGS;
}

// -------------------------------------------------

void lightningStrike(int pixel, double intensity, long color[]) {
  // Create color from intensity [r, g, b]
  int rColor = (int)(color[0] * intensity);
  int gColor = (int)(color[1] * intensity);
  int bColor = (int)(color[2] * intensity);

  // Light pixel
  strip.setPixelColor(pixel, strip.Color(rColor, bColor, gColor));
  strip.show();

  // Let it glow for some time
  lightningTimeout.timeOut(random(100, 500), turnAllPixelsOff);
}

// -------------------------------------------------

void turnAllPixelsOn(long color[]) {
  int rColor = (int)(color[0]);
  int gColor = (int)(color[1]);
  int bColor = (int)(color[2]);

  for (int i = 0; i < NUMBER_OF_LEDS; i++) {
    strip.setPixelColor(i, rColor, bColor, gColor);
  }

  strip.show();
}

// -------------------------------------------------

void turnAllPixelsOff() {
  for (int i = 0; i < NUMBER_OF_LEDS; i++) {
    strip.setPixelColor(i, 0);
  }
  strip.show();
}

// -------------------------------------------------

void changeMode(int mode) {

  if (CLOUD_MODE != mode) {
    if (IS_DEBUG) {
      Serial.print("cloud mode: ");
      Serial.write(mode);
    }

    CLOUD_MODE = mode;

    if (CLOUD_MODE > CLOUD_MODE_MAX || CLOUD_MODE < 0) {
      CLOUD_MODE = 0;
    }

    turnAllPixelsOff();
  }
}

