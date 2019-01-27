#include <TimeOut.h>
#include <Adafruit_NeoPixel.h>
#include <AltSoftSerial.h>

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

// LEDs
int NUM_LEDS = 75;                      // Number of LEDs on the pixel stripe
double MIN_INTENSITY = 0.1;             // Min output intensity
double MAX_INTENSITY = 1.0;             // Max output intensity

// BLUETOOTH
char COMMAND=' ';

// MISC
int CLOUD_MODE = 0;                     // 0 = All on; 1 = Thunder color 1; 2 = Thunder color 2; 3 = Party
int CLOUD_MODE_MAX = 4;                 // Max number of modes

int COUNTER = 0;                        // Counter for decreasing max value
int INPUT_MAX_NORMALIZE_CHECK = 100;    // Time to check decrease input max value
int LAST_STRIKE = 0;                    // Time of last lightning strike

// Create neopixel stripe
// Parameter 1 = number of pixels in strip
// Parameter 2 = pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// https://www.pjrc.com/teensy/td_libs_AltSoftSerial.html
AltSoftSerial BTserial; 

// https://github.com/NitrofMtl/TimeOut
TimeOut lightningTimeout;
Interval manualLoopInterval;

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

  // Initialize all the input max readings to 0
  for (int reading = 0; reading < INPUT_READINGS; reading++) {
    INPUT_MAX[reading] = 0;
  }

  // Start a manual loop to pervent blocking the main loop
  manualLoopInterval.interval(50, manualLoop);
}

// -------------------------------------------------

void loop() {
 
  // Read and write bluetooth signals
  processBluetoothSignals();

  TimeOut::handler();
  Interval::handler();
}

// -------------------------------------------------
void manualLoop(){
  int microSignal = analogRead(MICRO_PIN);
  COUNTER++;
Serial.print("CLOUD_MODE: ");
Serial.println(CLOUD_MODE);
  // Normalize input max average every INPUT_MAX_NORMALIZE_CHECK if there wasn't a strike within the last loops
  if (COUNTER % INPUT_MAX_NORMALIZE_CHECK == 0 && COUNTER - INPUT_MAX_NORMALIZE_CHECK < LAST_STRIKE) {
    normalizeInputMaxAverage();
  }

  // Collect signal min and max peaks
  collectPeaks(microSignal);

  // Let's do some magic!
  run(microSignal);
}


void run(int microSignal) {
  double signalIntensity = getSignalIntensity(microSignal);
  
  //Serial.print("microSignal: ");
  //Serial.print(microSignal);
  //Serial.print(", signalIntensity: ");
  //Serial.println(signalIntensity);
  
  // All pixels on
  if (CLOUD_MODE == 0) {
    long color[3] = {255, 225, 220};
    turnAllPixelsOn(color);
  }

  // Color mode 1
  else if (CLOUD_MODE == 1) {
    if (signalIntensity >= THRESHOLD) {
      long color[3] = {255, 255, 255};
      lightningStrike(random(NUM_LEDS), getLedIntensity(microSignal), color);
    }
  }

  // Color mode 2
  else if (CLOUD_MODE == 2) {
    if (signalIntensity > THRESHOLD) {
      long color[3] = {0, 50, 255};
      lightningStrike(random(NUM_LEDS), getLedIntensity(microSignal), color);
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

void processBluetoothSignals() {
  // Read from the Bluetooth module and send to the Arduino Serial Monitor
  if (BTserial.available()) {
    COMMAND = BTserial.read();
    Serial.write(COMMAND);

    if (COMMAND == '0'){
      changeMode(0);
    }
    else if (COMMAND == '1'){
      changeMode(1);
    }
    else if (COMMAND == '2'){
      changeMode(2);
    }
    else if (COMMAND == '3'){
      changeMode(3);
    }
  }
}

// -------------------------------------------------

void collectPeaks(int microSignal) {
  double maxSignalSummed = 0;

  if (microSignal > INPUT_MAX_AVERAGE) {

    // Check if array index is greater than max input readings
    if (INPUT_MAX_INDEX >= INPUT_READINGS) {
      INPUT_MAX_INDEX = 0;
    }
    else {
      INPUT_MAX_INDEX++;
    }

    // Save micro signal
    INPUT_MAX[INPUT_MAX_INDEX] = microSignal;

    // Create new average input level max
    for (int reading = 0; reading < INPUT_READINGS; reading++) {
      maxSignalSummed = maxSignalSummed + INPUT_MAX[reading];
    }

    // Calculate the average reading
    INPUT_MAX_AVERAGE = maxSignalSummed / INPUT_READINGS;
  }
  else if (microSignal < INPUT_MIN) {
    INPUT_MIN = microSignal;  // save just the min levels
  }
}

// -------------------------------------------------

double getSignalIntensity(int microSignal) {
  double peakToPeak = 0;
  double signalIntensity = 0; // peak-to-peak intensity in percent from 0-1

  // Slow down the micro signal on its peak
  if (microSignal > INPUT_MAX_AVERAGE) {
    microSignal = INPUT_MAX_AVERAGE;
  }

  peakToPeak = INPUT_MAX_AVERAGE - INPUT_MIN;  // max - min = peak-peak amplitude
  signalIntensity = (microSignal - INPUT_MIN) / peakToPeak;

  return signalIntensity;
}

// -------------------------------------------------

double getLedIntensity(int microSignal) {
  int max_input_last_readings = INPUT_MAX[0];
  int min_input_last_readings = INPUT_MAX[0];

  // Get highest and lowest value of last max reading
  for (int reading = 0; reading < INPUT_READINGS; reading++) {
    if (max_input_last_readings < INPUT_MAX[reading]) {
      max_input_last_readings = INPUT_MAX[reading];
    }

    if (min_input_last_readings > INPUT_MAX[reading]) {
      min_input_last_readings = INPUT_MAX[reading];
    }
  }

  // Calculate signal within led intensity range
  int peakToPeakSignal = max_input_last_readings - min_input_last_readings;

  // If there are no values to calculate return min intensity
  if (peakToPeakSignal == 0) {
    return MIN_INTENSITY;
  }

  double peakToPeakIntensity = MAX_INTENSITY - MIN_INTENSITY;
  double signal = double(microSignal - min_input_last_readings) / peakToPeakSignal;

  return signal * peakToPeakIntensity;
}

// -------------------------------------------------

void lightningStrike(int pixel, double intensity, long color[]) {
  LAST_STRIKE = COUNTER;

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

  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, rColor, bColor, gColor);
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

// -------------------------------------------------

void normalizeInputMaxAverage() {
  int maxReadingIndex = 0;

  for (int reading = 0; reading < INPUT_READINGS; reading++) {
    // Save index of the highest input max reading
    if (INPUT_MAX[reading] > INPUT_MAX[maxReadingIndex]) {
      maxReadingIndex = reading;
    }
  }

  // Decline max peak to average
  INPUT_MAX[maxReadingIndex] = INPUT_MAX_AVERAGE;
}

// -------------------------------------------------

void changeMode(int mode) {
  CLOUD_MODE = mode;

  if (CLOUD_MODE >= CLOUD_MODE_MAX || CLOUD_MODE < 0) {
    CLOUD_MODE = 0;
  }

  turnAllPixelsOff();
}
