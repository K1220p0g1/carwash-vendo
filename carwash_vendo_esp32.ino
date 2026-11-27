/*
 * Carwash Vendo Hardware Controller (ESP32 / Arduino Mega / Uno)
 * Pairs with Android Carwash Vendo APK via USB-to-Serial (115200 baud, 8N1)
 *
 * Wiring (ESP32 DevKit v1):
 *   GPIO 27 -> Multi-Coin Slot Signal (10k pull-up to 3.3V, optocoupler/divider if 5V coin slot)
 *   GPIO 26 -> Relay 1: High-Pressure Water Pump (Active LOW optocoupler module)
 *   GPIO 25 -> Relay 2: Foam Soap Pump (Active LOW optocoupler module)
 *   GPIO 33 -> Relay 3: Air Blower (Active LOW optocoupler module)
 *   GPIO 14 -> Physical Button: WATER (INPUT_PULLUP to GND)
 *   GPIO 12 -> Physical Button: SOAP (INPUT_PULLUP to GND)
 *   GPIO 13 -> Physical Button: BLOWER (INPUT_PULLUP to GND)
 *   GPIO 32 -> Physical Button: PAUSE (INPUT_PULLUP to GND)
 *   GPIO 2  -> Onboard Status LED
 */

#include <Arduino.h>

// --- Pin Definitions (ESP32 defaults; adjust for Arduino Uno/Nano if needed) ---
constexpr int PIN_COIN_PULSE   = 27;
constexpr int PIN_RELAY_WATER  = 26;
constexpr int PIN_RELAY_SOAP   = 25;
constexpr int PIN_RELAY_BLOWER = 33;

constexpr int PIN_BTN_WATER    = 14;
constexpr int PIN_BTN_SOAP     = 12;
constexpr int PIN_BTN_BLOWER   = 13;
constexpr int PIN_BTN_PAUSE    = 32;
constexpr int PIN_STATUS_LED   = 2;

// Most 4-channel relay modules with optocouplers are ACTIVE LOW
constexpr int RELAY_ON  = LOW;
constexpr int RELAY_OFF = HIGH;

// --- Coin Pulse Debounce & Train Grouping ---
volatile unsigned long lastPulseEdgeMs = 0;
volatile int pulseCount = 0;
constexpr unsigned long PULSE_DEBOUNCE_MS = 25;   // Reject noise shorter than 25ms
constexpr unsigned long PULSE_TRAIN_GAP_MS = 320; // Gap > 320ms finalizes coin pulse train

// --- Watchdog Heartbeat ---
// If Android tablet crashes, reboots, or USB disconnects, shut off pumps within 3.5s
unsigned long lastAndroidPingMs = 0;
constexpr unsigned long WATCHDOG_TIMEOUT_MS = 3500;
bool relaysActive = false;

String serialBuffer = "";

#if defined(ESP32)
void IRAM_ATTR onCoinPulseISR() {
#else
void onCoinPulseISR() {
#endif
  unsigned long now = millis();
  if (now - lastPulseEdgeMs >= PULSE_DEBOUNCE_MS) {
    pulseCount++;
    lastPulseEdgeMs = now;
  }
}

void setRelays(bool water, bool soap, bool blower) {
  digitalWrite(PIN_RELAY_WATER,  water  ? RELAY_ON : RELAY_OFF);
  digitalWrite(PIN_RELAY_SOAP,   soap   ? RELAY_ON : RELAY_OFF);
  digitalWrite(PIN_RELAY_BLOWER, blower ? RELAY_ON : RELAY_OFF);
  relaysActive = (water || soap || blower);
  digitalWrite(PIN_STATUS_LED, relaysActive ? HIGH : LOW);
}

void allRelaysOff() {
  setRelays(false, false, false);
}

void processSerialCommand(const String& cmd) {
  if (cmd == "PING") {
    lastAndroidPingMs = millis();
    Serial.println("PONG");
    return;
  }

  lastAndroidPingMs = millis();

  if (cmd == "RELAY:WATER:ON") {
    setRelays(true, false, false);
  } else if (cmd == "RELAY:SOAP:ON") {
    setRelays(false, true, false);
  } else if (cmd == "RELAY:BLOWER:ON") {
    setRelays(false, false, true);
  } else if (cmd == "RELAY:ALL:OFF") {
    allRelaysOff();
  } else if (cmd.startsWith("TEST:")) {
    // Format: TEST:<water 0/1>:<soap 0/1>:<blower 0/1>
    if (cmd.length() >= 10) {
      bool w = cmd.charAt(5) == '1';
      bool s = cmd.charAt(7) == '1';
      bool b = cmd.charAt(9) == '1';
      setRelays(w, s, b);
    }
  } else {
    Serial.print("ERR:UNKNOWN_CMD:");
    Serial.println(cmd);
  }
}

void pollPhysicalButtons() {
  static unsigned long lastBtnMs = 0;
  if (millis() - lastBtnMs < 220) return;

  if (digitalRead(PIN_BTN_WATER) == LOW) {
    lastBtnMs = millis();
    Serial.println("BTN:WATER");
  } else if (digitalRead(PIN_BTN_SOAP) == LOW) {
    lastBtnMs = millis();
    Serial.println("BTN:SOAP");
  } else if (digitalRead(PIN_BTN_BLOWER) == LOW) {
    lastBtnMs = millis();
    Serial.println("BTN:BLOWER");
  } else if (digitalRead(PIN_BTN_PAUSE) == LOW) {
    lastBtnMs = millis();
    Serial.println("BTN:PAUSE");
  }
}

void setup() {
  // Force relays OFF before configuring outputs to prevent boot glitch
  digitalWrite(PIN_RELAY_WATER,  RELAY_OFF);
  digitalWrite(PIN_RELAY_SOAP,   RELAY_OFF);
  digitalWrite(PIN_RELAY_BLOWER, RELAY_OFF);

  pinMode(PIN_RELAY_WATER,  OUTPUT);
  pinMode(PIN_RELAY_SOAP,   OUTPUT);
  pinMode(PIN_RELAY_BLOWER, OUTPUT);
  pinMode(PIN_STATUS_LED,   OUTPUT);
  allRelaysOff();

  pinMode(PIN_BTN_WATER,  INPUT_PULLUP);
  pinMode(PIN_BTN_SOAP,   INPUT_PULLUP);
  pinMode(PIN_BTN_BLOWER, INPUT_PULLUP);
  pinMode(PIN_BTN_PAUSE,  INPUT_PULLUP);

  pinMode(PIN_COIN_PULSE, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_COIN_PULSE), onCoinPulseISR, FALLING);

  Serial.begin(115200);
  lastAndroidPingMs = millis();
}

void loop() {
  // 1. Read newline-delimited commands from Android tablet
  while (Serial.available() > 0) {
    char c = static_cast<char>(Serial.read());
    if (c == '\n') {
      serialBuffer.trim();
      if (serialBuffer.length() > 0) {
        processSerialCommand(serialBuffer);
      }
      serialBuffer = "";
    } else if (c != '\r') {
      serialBuffer += c;
    }
  }

  // 2. Finalize coin pulse train after gap timeout (Universal Multi-Coin Acceptor)
  // Configure Allan/Yenox multi-coin slot: 1 PHP = 1 pulse, 5 PHP = 5 pulses, 10 PHP = 10 pulses
  noInterrupts();
  int capturedPulses = pulseCount;
  unsigned long gap = millis() - lastPulseEdgeMs;
  if (capturedPulses > 0 && gap >= PULSE_TRAIN_GAP_MS) {
    pulseCount = 0;
    interrupts();
    Serial.print("COIN:");
    Serial.println(capturedPulses);
  } else {
    interrupts();
  }

  // 3. Poll optional physical panel push-buttons
  pollPhysicalButtons();

  // 4. Hardware Safety Watchdog: turn off pumps if Android heartbeat times out
  if (relaysActive && (millis() - lastAndroidPingMs > WATCHDOG_TIMEOUT_MS)) {
    allRelaysOff();
    Serial.println("ERR:WATCHDOG_TIMEOUT_RELAYS_OFF");
  }
}
