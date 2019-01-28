// https://learn.adafruit.com/adafruit-neopixel-uberguide/arduino-library-use
#include <Adafruit_NeoPixel.h>
// https://www.pjrc.com/teensy/td_libs_AltSoftSerial.html
#include <AltSoftSerial.h>
// https://github.com/NitrofMtl/TimeOut
#include <TimeOut.h>

// -------------------------------------------------

/*
 * Bluetooth commands
 *
 * off: everything off, reset thresholds
 * on: everything on
 * c0: color 0: custom color lightning
 * c1: color 1: white lightning
 * c2: color 2: red lightning
 * c3: color 3: green lightning
 * c4: color 4: blue lightning
 * t0: threshold +: increase the threshold
 * t1: threshold -: decrease the threshold
 * party: party mode
 */

// -------------------------------------------------

const int DEBUG_LEVEL = 0;              // 0 to 3; 0 = somthing, 2 = everything, 3 = nothing

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
const long LED_COLORS[5][3] = {          // Color map
  {160, 200, 255},                      // Color 0: custom color
  {255, 255, 255},                      // Color 1: white
  {255, 0, 0},                          // Color 2: red
  {0, 255, 0},                          // Color 3: green
  {0, 0, 255}                           // Color 4: blue
};
double MAX_INTENSITY;                   // Volt threshold in percent 0 to 100

// BLUETOOTH
String COMMAND;                         // Bluetooth input command, multiple collected chars

// MISC
String CLOUD_MODE;                      // Current  cloud mode

Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUMBER_OF_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
AltSoftSerial BTserial;
TimeOut lightningTimeout;
Interval manualLoopInterval;

// -------------------------------------------------

void initialize() {
  CLOUD_MODE = "off";
  
  THRESHOLD = 10;
  CURRENT_READING = 0;
  MAX_INTENSITY = 0;


  // Set all the input voltage readings to 0
  for (int reading = 0; reading < INPUT_READINGS; reading++) {
    VOLTAGE_READINGS[reading] = 0.00;
  }

  turnAllPixelsOff();
}

// -------------------------------------------------

void setup() {
  Serial.println("Debug level: " + String(DEBUG_LEVEL));

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
  turnAllPixelsOn();

  // Start a manual loop to prevent blocking the main loop
  manualLoopInterval.interval(50, run);
}

// -------------------------------------------------

void loop() {
  if (COMMAND == "c1") { changeMode(COMMAND); }
  else if (COMMAND == "c2") { changeMode(COMMAND); }
  else if (COMMAND == "c3") { changeMode(COMMAND); }
  else if (COMMAND == "c4") { changeMode(COMMAND); }
  else if (COMMAND == "c5") { changeMode(COMMAND); }
  else if (COMMAND == "party") { changeMode(COMMAND); }
  
  processBluetoothSignals();

  TimeOut::handler();
  Interval::handler();

  measureMircophoneVoltage();
}

// -------------------------------------------------

void run() {
  double signalIntensity = getVoltageAverage();
  if (signalIntensity > MAX_INTENSITY) {
    MAX_INTENSITY = signalIntensity;
  }
  // Get the ratio between the intensities without the threshold
  double intensityRatio = (THRESHOLD - signalIntensity) / (THRESHOLD - MAX_INTENSITY);
  String firstCommandChar = String(CLOUD_MODE.charAt(0));

  if (DEBUG_LEVEL >= 2) {
    Serial.print("SignalIntensity: ");
    Serial.print(signalIntensity);
    Serial.print("; ");
    Serial.print("IntensityRatio: ");
    Serial.println(intensityRatio);
  }

  // Color modes, color is picked out of the LED_COLORS array
  if (firstCommandChar == "c") {
    if (signalIntensity >= THRESHOLD) {
      int colorIndex = String(CLOUD_MODE.charAt(1)).toInt();
      
      lightningStrike(random(NUMBER_OF_LEDS), intensityRatio, LED_COLORS[colorIndex]);
    }
  }

  // Party mode
  else if (CLOUD_MODE == "party") {
    long color[3] = {random(0, 255), random(0, 255), random(0, 255)};
    double randomIntensity = random(0, 100) / 100.0;
    lightningStrike(random(NUMBER_OF_LEDS), randomIntensity, color);
  }
}

// -------------------------------------------------

void runOnCommandChanged(String command) {
  String firstCommandChar = String(command.charAt(0));
  
  // Dark / reset mode
  if (command == "off") {
    initialize();
  }

  // Light mode - turn all pixels on
  else if (command == "on") {
    turnAllPixelsOn();
  }
  
  else if (firstCommandChar == "t") {
    Serial.println(String(command.substring(1, 4)));

    double newThreshold = String(command.substring(1, 4)).toDouble();
    updateThreshold(newThreshold);
  }
}

// -------------------------------------------------

void processBluetoothSignals() {
  
  // Read from the Bluetooth module and send to the Arduino Serial Monitor
  if (BTserial.available()) {
    char cchar = ' ';
    cchar = BTserial.read();

    // Add char to command except it's the end of the stream
    if (cchar != '\r' && cchar != '\n') {
      COMMAND += String(cchar);
    } 
    else {
      if (DEBUG_LEVEL >= 0) {
        Serial.print("Bluetooth input: ");
        Serial.print(COMMAND);
        Serial.println("");
      }

      runOnCommandChanged(COMMAND);

      COMMAND = "";
    }
  }
}

// -------------------------------------------------

void measureMircophoneVoltage() {
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
  int duration = random(100, 1000);  

  // Light pixel
  strip.setPixelColor(pixel, strip.Color(rColor, bColor, gColor));
  strip.show();

  if (DEBUG_LEVEL >= 1) {
    Serial.println("Lightning strike: ");
    Serial.print("pixel: " + (String)pixel + ", ");
    Serial.print("color rgb: " + (String)rColor + ", " + (String)bColor + ", " +  (String)gColor + ", ");
    Serial.println("duration: " + (String)duration);
  }

  // Let it glow for some time
  lightningTimeout.timeOut(duration, turnAllPixelsOff);
}

// -------------------------------------------------

void turnAllPixelsOn() {
  long color[] = {255, 255, 255};
  
  int rColor = (int)(color[0]);
  int gColor = (int)(color[1]);
  int bColor = (int)(color[2]);

  for (int i = 0; i < NUMBER_OF_LEDS; i++) {
    strip.setPixelColor(i, rColor, bColor, gColor);
  }

  if (DEBUG_LEVEL >= 1) {
    Serial.println("Turn all pixels on");
  }

  strip.show();
}

// -------------------------------------------------

void turnAllPixelsOff() {
  for (int i = 0; i < NUMBER_OF_LEDS; i++) {
    strip.setPixelColor(i, 0);
  }

  if (DEBUG_LEVEL >= 1) {
    Serial.println("Turn all pixels off");
  }
  
  strip.show();
}

// -------------------------------------------------

void changeMode(String mode) {

  if (CLOUD_MODE != mode) {
    if (DEBUG_LEVEL >= 0) {
      Serial.print("Cloud mode: " + String(mode));
      Serial.println("");
    }

    CLOUD_MODE = mode;

    turnAllPixelsOff();
  }
}

// -------------------------------------------------

void updateThreshold(double newThreshold) {
  THRESHOLD = newThreshold;
  
  if (DEBUG_LEVEL >= 0) {
    Serial.println("Threshold changed: " + String(THRESHOLD));
  }
}


