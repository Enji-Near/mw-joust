// Joust bench firmware v2. Task 1 skeleton: stops every motor at boot, then idles.
// Jog, switch watch and calibration arrive in Tasks 5-6.
#include <Adafruit_MotorShield.h>
#include "axis.h"  // builds lib/motion for AVR, so the Mega build checks it too
#include "config.h"

static Adafruit_MotorShield shield;

void setup() {
  // First: begin() zeroes all 16 PWM channels (Adafruit_MotorShield.cpp:77-78), stopping
  // any shield motor left running through the Mega reset. Returns false if the shield is absent.
  const bool shieldOk = shield.begin();

  // Write the level before switching to output, so the relays never pulse on at boot.
  digitalWrite(PIN_CHAIN_RELAY_CCW, RELAY_OFF_LEVEL);
  digitalWrite(PIN_CHAIN_RELAY_CW, RELAY_OFF_LEVEL);
  pinMode(PIN_CHAIN_RELAY_CCW, OUTPUT);
  pinMode(PIN_CHAIN_RELAY_CW, OUTPUT);

  Serial.begin(SERIAL_BAUD);
  Serial.println(F("Joust v2 bench skeleton: motors stopped."));
  if (!shieldOk) Serial.println(F("FAULT: motor shield not found on I2C"));
}

void loop() {}
