#include <LedControl.h>

// Pins
#define RED_SENSOR_PIN 4    // TCRT5000 red lane
#define RED_BUTTON_PIN 25   // Red lane button
#define RED_LED_PIN 14      // Red LED
#define MAX7219_DIN 23
#define MAX7219_CLK 19      // Try 19 if issues
#define MAX7219_CS_RED 13

LedControl lcRed = LedControl(MAX7219_DIN, MAX7219_CLK, MAX7219_CS_RED, 1);
struct Lane {
  int lapCount = 0;
  unsigned long lastLapTime = 0;
  unsigned long prevLapTimestamp = 0;
  unsigned long displayTimeStart = 0;
  bool displayLapTime = false;
};
Lane redLane;
bool raceStarted = false;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("Minimal MAX7219 Test Starting...");
  pinMode(RED_SENSOR_PIN, INPUT_PULLUP);
  pinMode(RED_BUTTON_PIN, INPUT_PULLUP);
  pinMode(RED_LED_PIN, OUTPUT);
  digitalWrite(RED_LED_PIN, LOW);

  lcRed.shutdown(0, false);
  lcRed.setIntensity(0, 4);
  lcRed.clearDisplay(0);
  Serial.println("MAX7219: Displaying 88888888 for 2s");
  portENTER_CRITICAL(&timerMux);
  for (int i = 0; i < 8; i++) {
    lcRed.setDigit(0, i, 8, false);
    delay(20);
  }
  portEXIT_CRITICAL(&timerMux);
  delay(2000);
  updateDisplay(&redLane);
  Serial.println("MAX7219: Initialized, should display 00000000");
}

void loop() {
  static unsigned long lastRedTrigger = 0;
  if ((digitalRead(RED_SENSOR_PIN) == LOW || digitalRead(RED_BUTTON_PIN) == LOW) && millis() - lastRedTrigger > 400) {
    if (raceStarted) {
      unsigned long currentTime = millis();
      if (redLane.lapCount == 0) {
        redLane.lastLapTime = currentTime - redLane.prevLapTimestamp;
      } else {
        redLane.lastLapTime = currentTime - redLane.prevLapTimestamp;
      }
      redLane.prevLapTimestamp = currentTime;
      redLane.lapCount++;
      redLane.displayLapTime = true;
      redLane.displayTimeStart = currentTime;
      digitalWrite(RED_LED_PIN, HIGH);
      delay(1000);
      digitalWrite(RED_LED_PIN, LOW);
      updateDisplay(&redLane);
      Serial.print("LAP TRIGGERED: Red: Lap ");
      Serial.print(redLane.lapCount);
      Serial.print(", Time: ");
      Serial.println(formatTime(redLane.lastLapTime));
    }
    lastRedTrigger = millis();
  }

  if (redLane.displayLapTime && millis() - redLane.displayTimeStart >= 5000) {
    redLane.displayLapTime = false;
    updateDisplay(&redLane);
  }

  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd == "start") {
      raceStarted = true;
      redLane.prevLapTimestamp = millis();
      Serial.println("RACE STARTED");
    } else if (cmd == "reset") {
      raceStarted = false;
      redLane.lapCount = 0;
      redLane.lastLapTime = 0;
      redLane.displayLapTime = false;
      portENTER_CRITICAL(&timerMux);
      lcRed.clearDisplay(0);
      portEXIT_CRITICAL(&timerMux);
      updateDisplay(&redLane);
      Serial.println("RACE RESET");
    }
  }
}

String formatTime(unsigned long ms) {
  unsigned long minutes = ms / 60000;
  ms %= 60000;
  unsigned long seconds = ms / 1000;
  unsigned long millisecs = ms % 1000;
  char buffer[12];
  snprintf(buffer, 12, "%02lu.%02lu.%03lu", minutes, seconds, millisecs);
  return String(buffer);
}

void updateDisplay(Lane* lane) {
  portENTER_CRITICAL(&timerMux);
  lcRed.clearDisplay(0);
  if (lane->displayLapTime) {
    unsigned long ms = lane->lastLapTime;
    unsigned long minutes = ms / 60000;
    ms %= 60000;
    unsigned long seconds = ms / 1000;
    unsigned long millisecs = ms % 1000;
    char buffer[9];
    snprintf(buffer, 9, "%02lu%02lu%03lu", minutes, seconds, millisecs);
    Serial.print("MAX7219 Updating Lap Time: ");
    Serial.print(buffer);
    Serial.print(" (Digits: ");
    for (int i = 0; i < 8; i++) {
      if (i == 4) {
        lcRed.setChar(0, 7 - i, '.', false);
        Serial.print(".");
      } else {
        int digit = buffer[i < 4 ? i : i - 1] - '0';
        lcRed.setDigit(0, 7 - i, digit, false);
        Serial.print(digit);
      }
      delay(20);
      if (i < 7) Serial.print(",");
    }
    Serial.println(")");
  } else {
    char buffer[9];
    snprintf(buffer, 9, "%08d", lane->lapCount);
    Serial.print("MAX7219 Updating Lap Count: ");
    Serial.print(buffer);
    Serial.print(" (Digits: ");
    for (int i = 0; i < 8; i++) {
      int digit = buffer[i] - '0';
      lcRed.setDigit(0, 7 - i, digit, false);
      Serial.print(digit);
      delay(20);
      if (i < 7) Serial.print(",");
    }
    Serial.println(")");
  }
  portEXIT_CRITICAL(&timerMux);
}