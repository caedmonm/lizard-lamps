
#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <MycilaDimmers.h>
#include <MycilaPulseAnalyzer.h>
#include <math.h>

// ==========================================
// CONFIGURATION
// ==========================================

// Independent temperature targets
float targetTemperatures[2] = {
  19.0,  // Sensor 1 target
  19.0   // Sensor 2 target
};

// Which physical lamp to control
int activeLamp = 0;  // 0 = Lamp 1, 1 = Lamp 2

// Degrees below target to reach full power
const float FADE_TEMP_RANGE = 3.0;

// Temperature sensors
#define ONE_WIRE_PIN 4

// AC dimmer
#define ZERO_CROSS_PIN GPIO_NUM_23
#define DIMMER_PIN_1   GPIO_NUM_25
#define DIMMER_PIN_2   GPIO_NUM_26

const unsigned long TEMP_INTERVAL_MS = 1000;
const unsigned long TEMP_CONVERSION_MS = 750;

// ==========================================
// HARDWARE
// ==========================================

OneWire oneWire(ONE_WIRE_PIN);
DallasTemperature sensors(&oneWire);

Mycila::PulseAnalyzer pulseAnalyzer;
Mycila::ThyristorDimmer lamp1;
Mycila::ThyristorDimmer lamp2;

// ==========================================
// STATE
// ==========================================

bool dimmerReady = false;
bool tempPending = false;
bool sensorsHealthy = false;

unsigned long tempStarted = 0;

float temperatures[2] = {
  DEVICE_DISCONNECTED_C,
  DEVICE_DISCONNECTED_C
};

int currentPower = 0;

// ==========================================
// TEMPERATURE
// ==========================================

void requestTemps() {
  sensors.requestTemperatures();
  tempStarted = millis();
  tempPending = true;
}

bool validTemp(float t) {
  return isfinite(t) &&
         t != DEVICE_DISCONNECTED_C;
}

void printTemp(float t) {
  if (!validTemp(t)) {
    Serial.print("Disconnected");
  } else {
    Serial.print(t, 2);
    Serial.print(" C");
  }
}

// ==========================================
// LAMP CONTROL
// ==========================================

void setLampPower(int percent) {
  percent = constrain(percent, 0, 100);

  float duty = percent / 100.0f;

  // Only the selected lamp receives power
  lamp1.setDutyCycle(activeLamp == 0 ? duty : 0.0f);
  lamp2.setDutyCycle(activeLamp == 1 ? duty : 0.0f);

  currentPower = percent;
}

void updateLampControl() {
  int requestedPower = 0;

  bool safe =
    dimmerReady &&
    pulseAnalyzer.isOnline() &&
    sensorsHealthy &&
    (activeLamp == 0 || activeLamp == 1) &&
    FADE_TEMP_RANGE > 0;

  if (safe) {
    // Calculate how far below target each sensor is
    float deficit1 =
      targetTemperatures[0] - temperatures[0];

    float deficit2 =
      targetTemperatures[1] - temperatures[1];

    // Either sensor can request heating.
    // Use whichever requires the most heat.
    float deficit = max(deficit1, deficit2);

    float duty = constrain(
      deficit / FADE_TEMP_RANGE,
      0.0f,
      1.0f
    );

    requestedPower = roundf(duty * 100.0f);
  }

  if (dimmerReady) {
    setLampPower(requestedPower);
  } else {
    currentPower = 0;
  }
}

// ==========================================
// SETUP
// ==========================================

void setup() {
  Serial.begin(115200);
  delay(1000);

  sensors.begin();
  sensors.setResolution(12);
  sensors.setWaitForConversion(false);

  Serial.printf(
    "Found %d temperature sensors\n",
    sensors.getDeviceCount()
  );

  requestTemps();

  // UK 50Hz mains
  Mycila::Dimmer::setSemiPeriod(10000);

  lamp1.setPin(DIMMER_PIN_1);
  lamp2.setPin(DIMMER_PIN_2);

  lamp1.enablePowerLUT(true);
  lamp2.enablePowerLUT(true);

  bool started1 = lamp1.begin();
  bool started2 = lamp2.begin();

  if (started1 && started2) {
    lamp1.setDutyCycle(0);
    lamp2.setDutyCycle(0);

    lamp1.setOnline(true);
    lamp2.setOnline(true);

    pulseAnalyzer.onZeroCross(
      Mycila::ThyristorDimmer::onZeroCross
    );

    dimmerReady = pulseAnalyzer.begin(ZERO_CROSS_PIN);
  }

  if (!dimmerReady) {
    lamp1.end();
    lamp2.end();
    Serial.println("Dimmer initialization failed");
  } else {
    Serial.println("Dimmer initialized");
  }

  Serial.printf(
    "Targets: %.1f C / %.1f C | Active lamp: %d\n",
    targetTemperatures[0],
    targetTemperatures[1],
    activeLamp + 1
  );
}

// ==========================================
// LOOP
// ==========================================

void loop() {
  unsigned long now = millis();

  if (tempPending &&
      now - tempStarted >= TEMP_CONVERSION_MS) {

    temperatures[0] = sensors.getTempCByIndex(0);
    temperatures[1] = sensors.getTempCByIndex(1);

    sensorsHealthy =
      sensors.getDeviceCount() >= 2 &&
      validTemp(temperatures[0]) &&
      validTemp(temperatures[1]);

    tempPending = false;

    updateLampControl();

    Serial.print("Sensor 1: ");
    printTemp(temperatures[0]);

    Serial.printf(
      " (target %.1f)",
      targetTemperatures[0]
    );

    Serial.print(" | Sensor 2: ");
    printTemp(temperatures[1]);

    Serial.printf(
      " (target %.1f)",
      targetTemperatures[1]
    );

    Serial.printf(
      " | Lamp: %d | Power: %d%%\n",
      activeLamp + 1,
      currentPower
    );
  }

  // Request temperatures every second
  if (!tempPending &&
      now - tempStarted >= TEMP_INTERVAL_MS) {
    requestTemps();
  }

  // Turn off if zero-cross detection is lost
  if (dimmerReady &&
      !pulseAnalyzer.isOnline() &&
      currentPower > 0) {
    setLampPower(0);
  }
}
