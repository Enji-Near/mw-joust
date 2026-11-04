// Joust retrofit: motor endurance bench test.
// Cycles two N20 gearmotors and two 8 mm steppers between position A and B until told
// to stop, and reports cycle counts and resets over serial. tools/bench_logger.py records it.
#include <Arduino.h>
#include <EEPROM.h>
#include <avr/wdt.h>
#include "config.h"

// ---------- N20 gearmotors ----------
struct N20 {
  uint8_t in1, in2, encA, encB;
  bool invert;
  volatile int32_t count;
  volatile uint8_t prevState;
  float integral;
  int32_t target;
  bool stalled;
  bool disabled;  // Latched by the runaway check. That motor sits out; the rest carry on.
};

N20 n20[2] = {
  {N20_1_IN1, N20_1_IN2, N20_1_ENC_A, N20_1_ENC_B, N20_1_ENC_INVERT, 0, 0, 0, 0, false, false},
  {N20_2_IN1, N20_2_IN2, N20_2_ENC_A, N20_2_ENC_B, N20_2_ENC_INVERT, 0, 0, 0, 0, false, false},
};

// Indexed by (previous AB << 2 | current AB). Impossible transitions count as 0.
const int8_t QUAD_TABLE[16] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};

static inline void updateEncoder(N20 &m) {
  uint8_t cur = (digitalRead(m.encA) << 1) | digitalRead(m.encB);
  int8_t d = QUAD_TABLE[(m.prevState << 2) | cur];
  m.count += m.invert ? -d : d;
  m.prevState = cur;
}
void isrEnc0() { updateEncoder(n20[0]); }
void isrEnc1() { updateEncoder(n20[1]); }

int32_t readCount(const N20 &m) {
  // 32-bit reads are not atomic on AVR, so an ISR could tear the value mid-read.
  noInterrupts();
  int32_t c = m.count;
  interrupts();
  return c;
}

// Slow decay (one input held high, TI SLVSAR1E table 2): the motor stays shorted between
// PWM pulses, which holds speed far better at the low duty this test runs at.
void driveN20(N20 &m, int16_t pwm) {
  if (m.disabled) {
    digitalWrite(m.in1, LOW);
    digitalWrite(m.in2, LOW);
  } else if (pwm > 0) {
    digitalWrite(m.in1, HIGH);
    analogWrite(m.in2, 255 - constrain(pwm, 0, 255));
  } else if (pwm < 0) {
    analogWrite(m.in1, 255 - constrain(-pwm, 0, 255));
    digitalWrite(m.in2, HIGH);
  } else {  // Brake: draws no supply current, so the gearbox holds position cold.
    digitalWrite(m.in1, HIGH);
    digitalWrite(m.in2, HIGH);
  }
}

void controlN20(N20 &m, int32_t setpoint) {
  if (m.disabled) return;
  int32_t err = setpoint - readCount(m);
  if (abs(err) > N20_RUNAWAY_COUNTS) {
    m.disabled = true;
    driveN20(m, 0);
    Serial.print(F("EVT RUNAWAY n20="));
    Serial.println(&m == &n20[0] ? 1 : 2);
    return;
  }
  if (abs(err) <= N20_TOL_COUNTS) {
    m.integral = 0;
    driveN20(m, 0);
    return;
  }
  m.integral = constrain(m.integral + N20_KI * err, -N20_I_MAX, N20_I_MAX);
  float out = N20_KP * err + m.integral;
  out += (out > 0) ? N20_MIN_PWM : -N20_MIN_PWM;
  driveN20(m, (int16_t)constrain(out, -255.0f, 255.0f));
}

// ---------- Steppers ----------
struct Stepper {
  const uint8_t *pins;
  uint8_t phase;
};
Stepper steppers[2] = {{STEP_1_PINS, 0}, {STEP_2_PINS, 0}};

// Output levels for AIN1, AIN2, BIN1, BIN2 at each electrical phase.
const uint8_t PHASE_ONE_COIL[4][4] = {{1, 0, 0, 0}, {0, 0, 1, 0}, {0, 1, 0, 0}, {0, 0, 0, 1}};
const uint8_t PHASE_TWO_COIL[4][4] = {{1, 0, 1, 0}, {0, 1, 1, 0}, {0, 1, 0, 1}, {1, 0, 0, 1}};

void energize(Stepper &s) {
  const uint8_t *levels = STEPPER_TWO_COIL ? PHASE_TWO_COIL[s.phase] : PHASE_ONE_COIL[s.phase];
  for (uint8_t i = 0; i < 4; i++) digitalWrite(s.pins[i], levels[i]);
}
void release(Stepper &s) {
  for (uint8_t i = 0; i < 4; i++) digitalWrite(s.pins[i], LOW);
}
void stepOnce(Stepper &s, int8_t dir) {
  s.phase = (s.phase + 4 + dir) % 4;
  energize(s);
}

// ---------- Persistence ----------
// A ring of slots spreads wear and means a reset during a write loses only that one slot.
// The Mega keeps EEPROM across uploads, so the checksum is seeded with the build time:
// a new build ignores old totals instead of resuming a previous test the moment it's flashed.
struct Checkpoint {
  uint32_t seq;
  uint32_t total;
  uint16_t resets;
  uint8_t running;
  uint8_t check;
};
const uint8_t SLOT_COUNT = 16;

uint32_t seqCounter = 0;
uint8_t nextSlot = 0;
uint32_t totalCycles = 0;
uint32_t sessionCycles = 0;
uint16_t resetCount = 0;
bool testActive = false;

uint8_t checksum(const Checkpoint &c) {
  const uint8_t *p = reinterpret_cast<const uint8_t *>(&c);
  uint8_t sum = 0x5A;
  for (const char *b = __DATE__ __TIME__; *b; b++) sum = (sum ^ *b) + 1;
  for (uint8_t i = 0; i < sizeof(Checkpoint) - 1; i++) sum = (sum ^ p[i]) + 1;
  return sum;
}

void saveCheckpoint() {
  Checkpoint c = {++seqCounter, totalCycles, resetCount, (uint8_t)(testActive ? 1 : 0), 0};
  c.check = checksum(c);
  EEPROM.put(nextSlot * sizeof(Checkpoint), c);
  nextSlot = (nextSlot + 1) % SLOT_COUNT;
}

void loadCheckpoint() {
  uint32_t best = 0;
  for (uint8_t i = 0; i < SLOT_COUNT; i++) {
    Checkpoint c;
    EEPROM.get(i * sizeof(Checkpoint), c);
    if (c.check != checksum(c) || c.seq == 0xFFFFFFFFUL || c.seq <= best) continue;
    best = c.seq;
    totalCycles = c.total;
    resetCount = c.resets;
    testActive = c.running;
    nextSlot = (i + 1) % SLOT_COUNT;
  }
  seqCounter = best;
}

// ---------- Test state machine ----------
enum Phase { P_IDLE, P_MOVE, P_DWELL, P_JOG_N20, P_JOG_STEPPER };
Phase phase = P_IDLE;
bool atB = false;        // The side we're at, or heading for during a move.
bool stopRequested = false;
uint32_t phaseStart = 0;
uint32_t lastControl = 0;
uint8_t stepsDone = 0;
int8_t stepDir = 1;
int32_t moveFrom[2];

int32_t errAtA[2] = {0, 0}, errAtB[2] = {0, 0};
uint32_t faultN20 = 0, faultStep = 0, faultReported = 0;
bool prevFaultN20 = false, prevFaultStep = false;

uint8_t jogIndex = 0;
int32_t jogStartCount = 0;
uint16_t jogStepsLeft = 0;
int16_t jogPwm = 0;

void startMove(bool toB) {
  atB = toB;
  stepDir = toB ? 1 : -1;
  stepsDone = 0;
  for (uint8_t i = 0; i < 2; i++) {
    moveFrom[i] = n20[i].target;
    n20[i].target = toB ? N20_TRAVEL_COUNTS : 0;
    n20[i].integral = 0;
  }
  phaseStart = millis();
  phase = P_MOVE;
}

void reportStall(uint8_t i, bool nowStalled) {
  if (nowStalled == n20[i].stalled) return;
  n20[i].stalled = nowStalled;
  Serial.print(nowStalled ? F("EVT STALL n20=") : F("EVT STALL_CLEAR n20="));
  Serial.println(i + 1);
}

void endMove() {
  for (uint8_t i = 0; i < 2; i++) {
    driveN20(n20[i], 0);
    release(steppers[i]);
  }
  phaseStart = millis();
  phase = P_DWELL;
}

void printCycle() {
  Serial.print(F("CYC session="));
  Serial.print(sessionCycles);
  Serial.print(F(" total="));
  Serial.print(totalCycles);
  Serial.print(F(" ms="));
  Serial.print(millis());
  Serial.print(F(" e1="));
  Serial.print(errAtA[0]);
  Serial.print('/');
  Serial.print(errAtB[0]);
  Serial.print(F(" e2="));
  Serial.print(errAtA[1]);
  Serial.print('/');
  Serial.println(errAtB[1]);
}

void printStatus() {
  Serial.print(F("STATUS active="));
  Serial.print(testActive);
  Serial.print(F(" side="));
  Serial.print(atB ? 'B' : 'A');
  Serial.print(F(" session="));
  Serial.print(sessionCycles);
  Serial.print(F(" total="));
  Serial.print(totalCycles);
  Serial.print(F(" resets="));
  Serial.print(resetCount);
  Serial.print(F(" enc1="));
  Serial.print(readCount(n20[0]));
  Serial.print(F(" enc2="));
  Serial.print(readCount(n20[1]));
  Serial.print(F(" faults_n20="));
  Serial.print(faultN20);
  Serial.print(F(" faults_step="));
  Serial.println(faultStep);
}

// Runs at the end of each dwell, when nothing is moving: EEPROM writes and serial prints
// take a few ms and would otherwise disturb the motion timing.
void endDwell() {
  for (uint8_t i = 0; i < 2; i++) {
    int32_t err = n20[i].target - readCount(n20[i]);
    (atB ? errAtB : errAtA)[i] = err;
    if (!n20[i].disabled) reportStall(i, abs(err) > N20_STALL_COUNTS);
  }
  if (atB) {
    startMove(false);
    return;
  }
  sessionCycles++;
  totalCycles++;
  printCycle();
  if (faultN20 + faultStep != faultReported) {
    faultReported = faultN20 + faultStep;
    Serial.print(F("EVT FAULT n20="));
    Serial.print(faultN20);
    Serial.print(F(" step="));
    Serial.println(faultStep);
  }
  if (stopRequested) {
    stopRequested = false;
    testActive = false;
    saveCheckpoint();
    phase = P_IDLE;
    Serial.print(F("EVT STOPPED "));
    printStatus();
    return;
  }
  if (totalCycles % CHECKPOINT_EVERY_CYCLES == 0) saveCheckpoint();
  startMove(true);
}

void tickMove(uint32_t now) {
  uint32_t elapsed = now - phaseStart;
  if (stepsDone < STEPPER_STEPS_PER_MOVE &&
      elapsed >= stepsDone * (MOVE_MS / STEPPER_STEPS_PER_MOVE)) {
    stepOnce(steppers[0], stepDir);
    stepOnce(steppers[1], stepDir);
    stepsDone++;
  }
  if (now - lastControl >= N20_CONTROL_MS) {
    lastControl = now;
    uint32_t t = min(elapsed, MOVE_MS);
    for (uint8_t i = 0; i < 2; i++) {
      // Follow a moving setpoint so the N20s take the full MOVE_MS, rather than
      // getting there in a fifth of the time and sitting idle.
      int32_t span = n20[i].target - moveFrom[i];
      controlN20(n20[i], moveFrom[i] + span * (int32_t)t / (int32_t)MOVE_MS);
    }
  }
  if (elapsed >= MOVE_MS) {
    endMove();
    if (!testActive) {
      phase = P_IDLE;
      Serial.print(F("EVT AT "));
      Serial.println(atB ? 'B' : 'A');
    }
  }
}

void tickJog(uint32_t now) {
  if (phase == P_JOG_N20) {
    if (now - phaseStart >= N20_JOG_TIMEOUT_MS) {
      Serial.println(F("EVT JOG_TIMEOUT"));
      phase = P_IDLE;
      driveN20(n20[jogIndex], 0);
    }
    return;
  }
  if (now - phaseStart < STEPPER_JOG_MS) return;
  if (jogStepsLeft == 0) {
    release(steppers[jogIndex]);
    phase = P_IDLE;
    Serial.println(F("EVT JOG_DONE"));
    return;
  }
  phaseStart = now;
  stepOnce(steppers[jogIndex], 1);
  jogStepsLeft--;
}

void finishN20Jog() {
  driveN20(n20[jogIndex], 0);
  delay(100);  // Let the rotor coast down so the count is final.
  int32_t moved = readCount(n20[jogIndex]) - jogStartCount;
  Serial.print(F("EVT JOG_COUNTS n20="));
  Serial.print(jogIndex + 1);
  Serial.print(F(" counts="));
  Serial.println(moved);
  if ((moved > 0) != (jogPwm > 0)) {
    Serial.println(F("EVT ENCODER_REVERSED set N20_x_ENC_INVERT in config.h"));
  }
  phase = P_IDLE;
}

void printHelp() {
  Serial.println(F("g start  x stop at A  s status  a/b go to A/B  z this position is A"));
  Serial.println(F("c <1|2> <pwm> run N20 open loop (x stops, prints counts)"));
  Serial.println(F("t <1|2> <steps> step a stepper (20 should be one turn)  n clear totals"));
}

void handleCommand(char *line) {
  char cmd = line[0];
  if (cmd == 's') { printStatus(); return; }
  if (cmd == 'h') { printHelp(); return; }
  if (cmd == 'x') {
    if (phase == P_JOG_N20) finishN20Jog();
    else if (testActive) { stopRequested = true; Serial.println(F("EVT STOP_REQUESTED")); }
    return;
  }
  if (phase != P_IDLE) {
    Serial.println(F("EVT BUSY"));
    return;
  }
  int a = 0, b = 0;
  switch (cmd) {
    case 'g':
      if (atB) { Serial.println(F("EVT NOT_AT_A send a first")); break; }
      testActive = true;
      sessionCycles = 0;
      saveCheckpoint();
      Serial.println(F("EVT TEST_START"));
      startMove(true);
      break;
    case 'a':
    case 'b':
      // A repeat would re-step the steppers, whose travel is relative to where they are.
      if ((cmd == 'b') == atB) { Serial.println(F("EVT ALREADY_THERE")); break; }
      startMove(cmd == 'b');
      break;
    case 'z':
      for (uint8_t i = 0; i < 2; i++) {
        noInterrupts();
        n20[i].count = 0;
        interrupts();
        n20[i].target = 0;
      }
      atB = false;
      Serial.println(F("EVT ZEROED"));
      break;
    case 'n':
      totalCycles = sessionCycles = 0;
      resetCount = 0;
      faultN20 = faultStep = faultReported = 0;
      saveCheckpoint();
      Serial.println(F("EVT CLEARED"));
      break;
    case 'c':
      if (sscanf(line + 1, "%d %d", &a, &b) == 2 && (a == 1 || a == 2) && b != 0) {
        jogIndex = a - 1;
        jogPwm = constrain(b, -255, 255);
        jogStartCount = readCount(n20[jogIndex]);
        phaseStart = millis();
        phase = P_JOG_N20;
        driveN20(n20[jogIndex], jogPwm);
      } else Serial.println(F("EVT BAD_ARGS"));
      break;
    case 't':
      if (sscanf(line + 1, "%d %d", &a, &b) == 2 && (a == 1 || a == 2) && b > 0) {
        jogIndex = a - 1;
        jogStepsLeft = b;
        phaseStart = millis();
        phase = P_JOG_STEPPER;
      } else Serial.println(F("EVT BAD_ARGS"));
      break;
    default:
      Serial.println(F("EVT UNKNOWN_COMMAND"));
  }
}

void pollSerial() {
  static char buf[24];
  static uint8_t len = 0;
  while (Serial.available()) {
    char ch = Serial.read();
    if (ch == '\n' || ch == '\r') {
      if (len > 0) {
        buf[len] = 0;
        handleCommand(buf);
        len = 0;
      }
    } else if (len < sizeof(buf) - 1) {
      buf[len++] = ch;
    }
  }
}

void pollFaults() {
  // Count falling edges only, since a sustained fault would otherwise count every pass.
  bool f = digitalRead(FAULT_N20_PIN) == LOW;
  if (f && !prevFaultN20) faultN20++;
  prevFaultN20 = f;
  f = digitalRead(FAULT_STEP_PIN) == LOW;
  if (f && !prevFaultStep) faultStep++;
  prevFaultStep = f;
}

// The Mega's bootloader clears MCUSR before the sketch starts, so the chip can't report why it
// reset. These survive a reset but not a power loss, which gives a best-effort answer.
const uint16_t RAM_MARKER = 0xB007;
uint16_t warmMarker __attribute__((section(".noinit")));
uint16_t watchdogMarker __attribute__((section(".noinit")));

// Interrupt + reset mode: this runs on the first timeout, the reset comes on the next one.
ISR(WDT_vect) { watchdogMarker = RAM_MARKER; }

void startWatchdog() {
  cli();
  wdt_reset();
  WDTCSR |= _BV(WDCE) | _BV(WDE);
  WDTCSR = _BV(WDIE) | _BV(WDE) | _BV(WDP2) | _BV(WDP1) | _BV(WDP0);  // ~2 s
  sei();
}

void printResetCause() {
  Serial.print(F("BOOT reset="));
  if (watchdogMarker == RAM_MARKER) Serial.print(F("WATCHDOG"));
  else if (warmMarker == RAM_MARKER) Serial.print(F("WARM"));  // Button, USB/DTR, or a dip
  else Serial.print(F("POWER_ON"));
  watchdogMarker = 0;
  warmMarker = RAM_MARKER;
  Serial.print(F(" total="));
  Serial.print(totalCycles);
  Serial.print(F(" resets="));
  Serial.print(resetCount);
  Serial.print(F(" resuming="));
  Serial.println(testActive);
}

void setup() {
  wdt_disable();  // A watchdog reset leaves it running; setup's delays could trip it.
  Serial.begin(SERIAL_BAUD);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(FAULT_N20_PIN, INPUT_PULLUP);
  pinMode(FAULT_STEP_PIN, INPUT_PULLUP);

  for (uint8_t i = 0; i < 2; i++) {
    pinMode(n20[i].in1, OUTPUT);
    pinMode(n20[i].in2, OUTPUT);
    driveN20(n20[i], 0);
    pinMode(n20[i].encA, INPUT_PULLUP);
    pinMode(n20[i].encB, INPUT_PULLUP);
    n20[i].prevState = (digitalRead(n20[i].encA) << 1) | digitalRead(n20[i].encB);
    for (uint8_t p = 0; p < 4; p++) pinMode(steppers[i].pins[p], OUTPUT);
  }
  attachInterrupt(digitalPinToInterrupt(N20_1_ENC_A), isrEnc0, CHANGE);
  attachInterrupt(digitalPinToInterrupt(N20_1_ENC_B), isrEnc0, CHANGE);
  attachInterrupt(digitalPinToInterrupt(N20_2_ENC_A), isrEnc1, CHANGE);
  attachInterrupt(digitalPinToInterrupt(N20_2_ENC_B), isrEnc1, CHANGE);

  loadCheckpoint();
  if (testActive) {
    resetCount++;
    saveCheckpoint();
  }
  printResetCause();

  // The rotor can rest at any detent, so pull it onto phase 0 now. Otherwise this
  // snap would happen on the first move, after the flag had been marked.
  for (uint8_t i = 0; i < 2; i++) energize(steppers[i]);
  delay(200);
  for (uint8_t i = 0; i < 2; i++) release(steppers[i]);

  if (testActive) {
    // Encoders restart from zero and the stepper phase is lost, so the flags may no longer
    // line up with the marks. Keep cycling anyway: the wear data is still worth having.
    Serial.println(F("EVT RESUMED position reference lost, flag marks may be offset"));
    startMove(true);
  } else {
    printHelp();
  }

  startWatchdog();  // Last, so the setup delays can't trip it.
}

void loop() {
  wdt_reset();
  uint32_t now = millis();
  pollSerial();
  pollFaults();
  switch (phase) {
    case P_MOVE: tickMove(now); break;
    case P_DWELL:
      if (now - phaseStart >= DWELL_MS) endDwell();
      break;
    case P_JOG_N20:
    case P_JOG_STEPPER: tickJog(now); break;
    default: break;
  }
  // Fast blink while cycling, slow when idle: visible from across the room.
  digitalWrite(LED_BUILTIN, (now >> (testActive ? 7 : 10)) & 1);
}
