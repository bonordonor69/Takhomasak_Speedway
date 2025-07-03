// Takhomasak Speedway - Multi-Lane Version with MAX7219 Lap Times
// Date: June 6, 2025
// Features: VL53L0X sensors, MAX7219 displays, ESP32 webserver, login system, full separate html
// Note: Added enhanced debug features

/*
 * ESP-WROOM-32 Wiring Diagram
 *
 * Red VL53L0X Sensor:
 *   GPIO 4  (XSHUT) ---- Red ---------------- VL53L0X XSHUT
 *   3.3V    (VCC)   ---- White -------------- VL53L0X VCC
 *   GND             ---- Black -------------- VL53L0X GND
 *   GPIO 21 (SDA)   ---- Blue (White Plug) -- VL53L0X SDA
 *   GPIO 22 (SCL)   ---- Green (White Plug)-- VL53L0X SCL
 *
 * Yellow VL53L0X Sensor:
 *   GPIO 18 (XSHUT) ---- Yellow ------------- VL53L0X XSHUT
 *   3.3V    (VCC)   ---- White -------------- VL53L0X VCC
 *   GND             ---- Black -------------- VL53L0X GND
 *   GPIO 21 (SDA)   ---- Blue (White Plug) -- VL53L0X SDA
 *   GPIO 22 (SCL)   ---- Green (White Plug)-- VL53L0X SCL
 *
 * Blue VL53L0X Sensor:
 *   GPIO 5  (XSHUT) ---- Blue --------------- VL53L0X XSHUT
 *   3.3V    (VCC)   ---- White -------------- VL53L0X VCC
 *   GND             ---- Black -------------- VL53L0X GND
 *   GPIO 21 (SDA)   ---- Blue (White Plug) -- VL53L0X SDA
 *   GPIO 22 (SCL)   ---- Green (White Plug)-- VL53L0X SCL
 *
 * Red LED:
 *   GPIO 14 (Signal) ---- Orange ------------ LED Anode
 *   GND     (via 220Ω) -- Black ------------- LED Cathode
 *
 * Yellow LED:
 *   GPIO 15 (Signal) ---- Yellow ------------ LED Anode
 *   GND     (via 220Ω) -- Black ------------- LED Cathode
 *
 * Blue LED:
 *   GPIO 16 (Signal) ---- Blue -------------- LED Anode
 *   GND     (via 220Ω) -- Black ------------- LED Cathode
 *
 * Red Button:
 *   GPIO 25 (Input) ---- Red (White Plug) --- Button Pin 1
 *   GND             ---- Black -------------- Button Pin 2
 *
 * Yellow Button:
 *   GPIO 27 (Input) ---- Yell (White Plug) -- Button Pin 1
 *   GND             ---- Black -------------- Button Pin 2
 *
 * Blue Button:
 *   GPIO 26 (Input) ---- Blue (White Plug) -- Button Pin 1
 *   GND             ---- Black -------------- Button Pin 2
 *
 * Start/Reset Button:
 *   GPIO 33 (Input) ---- Prpl (White Plug) -- Button Pin 1
 *   GND             ---- Black -------------- Button Pin 2
 *
 * MAX7219 Red Display:
 *   GPIO 23 (DIN)   ---- Gray ---- MAX7219 DIN
 *   GPIO 19 (CLK)   ---- Green --- MAX7219 CLK
 *   GPIO 13 (CS)    ---- Red/White ---- MAX7219 CS
 *   5V      (VCC)   ---- Brown --- MAX7219 VCC
 *   GND             ---- Black --- MAX7219 GND
 *
 * MAX7219 Yellow Display:
 *   GPIO 23 (DIN)   ---- Gray ---- MAX7219 DIN
 *   GPIO 19 (CLK)   ---- Green --- MAX7219 CLK
 *   GPIO 12 (CS)    ---- Yellow/White -- MAX7219 CS
 *   5V      (VCC)   ---- Brown --- MAX7219 VCC
 *   GND             ---- Black --- MAX7219 GND
 *
 * MAX7219 Blue Display:
 *   GPIO 23 (DIN)   ---- Gray ---- MAX7219 DIN
 *   GPIO 19 (CLK)   ---- Green --- MAX7219 CLK
 *   GPIO 32 (CS)    ---- Blue/White ---- MAX7219 CS
 *   5V      (VCC)   ---- Brown --- MAX7219 VCC
 *   GND             ---- Black --- MAX7219 GND
 *
 * Notes:
 * - Use 220-330Ω resistors for LEDs.
 * - Ensure 4.7kΩ pull-ups on I2C SDA/SCL if not on VL53L0X modules.
 * - Brown wires for 5V (MAX7219); white for 3.3V (VL53L0X).
 * - Share I2C (SDA, SCL) and SPI (DIN, CLK) lines; unique XSHUT/CS per device.
 */

//Included Packages
// #include <LedController.hpp> // Disabled for now - MAX7219s not connected
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <esp_task_wdt.h>
#include <vector>
#include <map>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <Adafruit_VL53L0X.h>
#include <SPI.h>
#include "config.h"


// Lane struct
struct Lane {
  String username;
  String carNumber;
  String carColor;
  int currentLap;
  unsigned long lastLapTime;
  unsigned long bestLapTime;
  String history[50];
  int lapCount;
  unsigned long startTime;
  unsigned long prevLapTimestamp;
  unsigned long startDisplayTime;
  int startSequencePhase;
  int pulseState;
  int flashCount;
  bool displayLapTime;
  unsigned long pulseStartTime;
  unsigned long displayTimeStart;
};

// Global lane and spectator variables
Lane yellowLane = { "", "", "", 1, 0, ULONG_MAX, {}, 1, 0, 0, 0, 0, 0, 0, false, 0, 0 };
Lane redLane = { "", "", "", 1, 0, ULONG_MAX, {}, 1, 0, 0, 0, 0, 0, 0, false, 0, 0 };
Lane blueLane = { "", "", "", 1, 0, ULONG_MAX, {}, 1, 0, 0, 0, 0, 0, 0, false, 0, 0 };
String spectators[10];
int spectatorCount = 0;

// Settings (Track Marshall only)
int maxLapCount = 9999;
unsigned long minLapTime = 500;
int displayBrightness = 2;

// Debug log buffer
std::vector<String> debugLogs;
int debugLogIndex = 0;
int debugLogCount = 0;

// User data structure
struct User {
  String username;
  String carNumber;
  String carColor;
  String lane;
  std::vector<unsigned long> lapHistory;
  unsigned long bestLapTime = ULONG_MAX;
};

// Session data structure
struct Session {
  String sessionId;
  unsigned long lastActive;
  String role;};

// Global user and session storage
std::map<String, User> users;
std::map<String, Session> sessions;
const unsigned long SESSION_TIMEOUT = 60000; // 60 seconds for testing, revert to 180*60*1000 (180 minutes) after testing
const String MARSHALL_USERNAME = "marshall";
const String MARSHALL_PASSWORD = "track2025";

// LiDAR Sensor Variables
unsigned long raceDurationSeconds = 0; // Default: no duration limit
unsigned long countdownTimeSeconds = 0; // Default: use hardcoded countdown
int detectionThreshold = 70; // Default detection threshold in mm
bool lapCountingEnabled = true; // Default: enabled
unsigned long lastLoopTime = 0;
unsigned long loopExecutionTime = 0;

// For sensor caching (to optimize performance)
int lastRedDistance = -1;
int lastYellowDistance = -1;
int lastBlueDistance = -1;
unsigned long lastSensorReadTime = 0;
const unsigned long SENSOR_READ_INTERVAL_MS = 50; // Read sensors every 50 ms
unsigned long lastSaveUsersTime = 0;
const unsigned long SAVE_USERS_THROTTLE_MS = 5000; // Throttle to once every 5 seconds
unsigned long lastRaceDataSendTime = 0;
const unsigned long RACE_DATA_SEND_INTERVAL_MS = 500;

// Race state variables
bool raceStarted = false;
unsigned long startTime = 0; // Global start time for the race
unsigned long lastPhaseChange = 0; // Last phase change timestamp
unsigned long phaseChangeInterval = 1250; // Phase change interval in ms

// Static IP configuration
IPAddress local_IP(192, 168, 1, 69);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8);
IPAddress secondaryDNS(8, 8, 4, 4);

//Validate Session
bool validateSession(String sessionId, String username);

// Pin definitions
#define RED_SENSOR_PIN 4     // GPIO 4, now used as RED_XSHUT_PIN
#define YELLOW_SENSOR_PIN 18 // GPIO 18, now used as YELLOW_XSHUT_PIN
#define BLUE_SENSOR_PIN 5    // GPIO 5, now used as BLUE_XSHUT_PIN
#define RED_XSHUT_PIN   RED_SENSOR_PIN    // GPIO 4
#define YELLOW_XSHUT_PIN YELLOW_SENSOR_PIN // GPIO 18
#define BLUE_XSHUT_PIN  BLUE_SENSOR_PIN   // GPIO 5
#define RED_BUTTON_PIN 25
#define YELLOW_BUTTON_PIN 27
#define BLUE_BUTTON_PIN 26
#define START_RESET_BUTTON_PIN 33
#define RED_LED_PIN 14
#define YELLOW_LED_PIN 15
#define BLUE_LED_PIN 16
#define MAX7219_DIN 23
#define MAX7219_CLK 19
#define MAX7219_CS_RED 13
#define MAX7219_CS_YELLOW 12
#define MAX7219_CS_BLUE 32
#define RED_VL53L0X_ADDRESS   0x30
#define YELLOW_VL53L0X_ADDRESS 0x31
#define BLUE_VL53L0X_ADDRESS   0x32

// Create VL53L0X objects for each lane
Adafruit_VL53L0X redSensor = Adafruit_VL53L0X();
Adafruit_VL53L0X yellowSensor = Adafruit_VL53L0X();
Adafruit_VL53L0X blueSensor = Adafruit_VL53L0X();

// Distance threshold for detecting a car (in mm; adjust based on testing)
const int DETECTION_THRESHOLD = 99; // ~3 inches (76 mm), slightly less to account for car height

// MAX7219 setup - Disabled (displays not connected)
// LedController<1,1> lcRed = LedController<1,1>();
// LedController<1,1> lcYellow = LedController<1,1>();
// LedController<1,1> lcBlue = LedController<1,1>();

// Webserver and WebSocket
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// Critical section mutex
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
portMUX_TYPE raceMux = portMUX_INITIALIZER_UNLOCKED;

// Function prototypes
void startRace();
void resetRace();
void handleLap(Lane* lane, String laneName, int ledPin, void* lc);
void sendRaceData();
String formatTime(unsigned long ms);
void updateDisplay(Lane* lane, void* lc, String laneName);
void updateDisplayBlink(Lane* lane, void* lc, String laneName);
void updateLED(Lane* lane, int ledPin);
void startSequence();
void loadUsers();
void saveUsers();
void printUsersJson();
void addDebugLog(String message);

//Setup
void setup() {
  esp_task_wdt_deinit();
  Serial.begin(115200);
  delay(100);
  Serial.println("Takhomasak Speedway Multi-Lane Starting...");

  // Initialize LED pins
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(YELLOW_LED_PIN, OUTPUT);
  pinMode(BLUE_LED_PIN, OUTPUT);
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(YELLOW_LED_PIN, LOW);
  digitalWrite(BLUE_LED_PIN, LOW);
  Serial.println("DEBUG: LEDs initialized to OFF, voltages should be 0V");
  // Initialize button pins
  pinMode(RED_BUTTON_PIN, INPUT_PULLUP);
  pinMode(YELLOW_BUTTON_PIN, INPUT_PULLUP);
  pinMode(BLUE_BUTTON_PIN, INPUT_PULLUP);
  pinMode(START_RESET_BUTTON_PIN, INPUT_PULLUP);
  Serial.println("DEBUG: Buttons initialized with INPUT_PULLUP, states should be HIGH");
  // Verify initial button states
  Serial.println("DEBUG: Initial Red button state: " + String(digitalRead(RED_BUTTON_PIN)));
  Serial.println("DEBUG: Initial Yellow button state: " + String(digitalRead(YELLOW_BUTTON_PIN)));
  Serial.println("DEBUG: Initial Blue button state: " + String(digitalRead(BLUE_BUTTON_PIN)));
  Serial.println("DEBUG: Initial Start/Reset button state: " + String(digitalRead(START_RESET_BUTTON_PIN)));
  if (LittleFS.begin()) {
    Serial.println("LittleFS mounted successfully");
    printUsersJson();
  } else {
    Serial.println("An Error has occurred while mounting LittleFS...");
  }

  // Set up XSHUT pins for VL53L0X sensors
  pinMode(RED_XSHUT_PIN, OUTPUT);
  pinMode(YELLOW_XSHUT_PIN, OUTPUT);
  pinMode(BLUE_XSHUT_PIN, OUTPUT);
  digitalWrite(RED_XSHUT_PIN, LOW);
  digitalWrite(YELLOW_XSHUT_PIN, LOW);
  digitalWrite(BLUE_XSHUT_PIN, LOW);

  // Initialize VL53L0X sensors
  Serial.println("Initializing VL53L0X sensors...");

  // Red sensor
  digitalWrite(RED_XSHUT_PIN, HIGH);
  delay(10);
  if (!redSensor.begin()) {
    Serial.println("Failed to boot Red VL53L0X");
    while (1);
  }
  redSensor.setAddress(RED_VL53L0X_ADDRESS);
  Serial.println("Red VL53L0X initialized at address 0x30");

  // Yellow sensor
  digitalWrite(YELLOW_XSHUT_PIN, HIGH);
  delay(10);
  if (!yellowSensor.begin()) {
    Serial.println("Failed to boot Yellow VL53L0X");
    while (1);
  }
  yellowSensor.setAddress(YELLOW_VL53L0X_ADDRESS);
  Serial.println("Yellow VL53L0X initialized at address 0x31");

  // Blue sensor
  digitalWrite(BLUE_XSHUT_PIN, HIGH);
  delay(10);
  if (!blueSensor.begin()) {
    Serial.println("Failed to boot Blue VL53L0X");
    while (1);
  }
  blueSensor.setAddress(BLUE_VL53L0X_ADDRESS);
  Serial.println("Blue VL53L0X initialized at address 0x32");

  // Initialize MAX7219 displays - DISABLED (not connected)
  Serial.println("MAX7219 displays disabled - hardware not connected");
  Serial.println("MAX7219 displays initialized");

  // Mount LittleFS
  Serial.println("Attempting to mount LittleFS...");
  if (!LittleFS.begin(true)) {
    Serial.println("An Error has occurred while mounting LittleFS. Continuing without filesystem...");
  } else {
    Serial.println("LittleFS mounted successfully");
    printUsersJson();
    if (LittleFS.exists("/index.html")) {
      Serial.println("index.html found in LittleFS");
    } else {
      Serial.println("index.html NOT found in LittleFS");
    }
    if (LittleFS.exists("/qrcode_dkm.png")) {
      Serial.println("qrcode_dkm.png found in LittleFS");
    } else {
      Serial.println("qrcode_dkm.png NOT found in LittleFS");
    }
  }

  // Configure static IP
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("Failed to configure static IP");
  } else {
    Serial.println("Static IP configured: 192.168.1.69");
  }

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi...");
  unsigned long wifiTimeout = millis() + 15000;
  while (WiFi.status() != WL_CONNECTED && millis() < wifiTimeout) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Connected to WiFi");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Failed to connect to WiFi. Continuing without network...");
  }

  // Set up server routes
server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
  if (request->hasParam("spectator") && request->getParam("spectator")->value() == "auto") {
    String username = "spectator_auto_" + String(spectatorCount + 1);
    String sessionId = "spectator_" + String(millis()) + "_" + String(random(1000, 9999));
    if (spectatorCount >= 10) { // Corrected from spectators->spectatorCount
      addDebugLog("Auto-login failed: Spectator limit reached");
      request->send(400, "text/plain", "Spectator limit reached");
      return;
    }
    request->send(400, "text/plain", "");
    for (int i = 0; i < spectatorCount; i++) {
      if (spectators[i] == username) {
        addDebugLog("Auto-login failed: Username conflict: " + username);
        request->send(400, "text/plain", "Username conflict");
        return;
      }
    }
    if (yellowLane.username == username || redLane.username == username || blueLane.username == username) {
      addDebugLog("Auto-login failed: Username already in use: " + username);
      request->send(400, "text/plain", "Username already in use");
      return;
    }
    spectators[spectatorCount++] = username;
    sessions[username] = {sessionId, millis(), "spectator"};
    addDebugLog("Auto-login successful: " + username + " as spectator, sessionId: " + sessionId);
    StaticJsonDocument<200> doc;
    doc["role"] = "spectator";
    doc["username"] = username;
    doc["sessionId"] = sessionId;
    String response;
    serializeJson(doc, response);
    AsyncWebServerResponse *responseObj = request->beginResponse(LittleFS, "/index.html", "text/html");
    responseObj->addHeader("Cache-Control", "no-cache");
    request->send(responseObj);
    ws.textAll("<script>window.autoLoginSpectator='" + username + "'; localStorage.setItem('sessionId', '" + sessionId + "');</script>");
  } else {
    if (LittleFS.exists("/index.html")) {
      AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/index.html", "text/html");
      response->addHeader("Cache-Control", "no-cache");
      request->send(response);
    } else {
      request->send(404, "text/plain", "index.html not found in LittleFS");
    }
  }
});

  // Serve static files from LittleFS
  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html").setFilter([](AsyncWebServerRequest *request) {
    return true;
  });

  // Handle 404
  server.onNotFound([](AsyncWebServerRequest *request){
    Serial.println("File not found: " + request->url());
    request->send(404, "text/plain", "File Not Found");
  });

  // Command route
server.on("/command", HTTP_GET, [](AsyncWebServerRequest *request){
  if (!request->hasParam("cmd") || !request->hasParam("username") || !request->hasParam("sessionId")) {
    addDebugLog("Command failed: Missing cmd, username, or sessionId");
    request->send(400, "text/plain", "Missing parameters");
    return;
  }
  String cmd = request->getParam("cmd")->value();
  String username = request->getParam("username")->value();
  String sessionId = request->getParam("sessionId")->value();
  if (sessions.find(username) == sessions.end() || sessions[username].sessionId != sessionId) {
    addDebugLog("Command failed: Invalid session for user: " + username + ", sessionId: " + sessionId);
    request->send(401, "text/plain", "Invalid session");
    return;
  }
if ((cmd == "r" || cmd == "y" || cmd == "b") && sessions[username].role != "marshall") {
    addDebugLog("Unauthorized command attempt: " + username + ", cmd: " + cmd);
    request->send(403, "text/plain", "Unauthorized: Restricted to Track Marshall");
    return;
}
  sessions[username].lastActive = millis(); // Refresh session
  Serial.println("Received web command: " + cmd + " from " + username);
  if (cmd == "start") {
    startRace();
    addDebugLog("Race started by " + username);
    request->send(200, "text/plain", "Race started");
  } else if (cmd == "reset") {
    resetRace();
    addDebugLog("Race reset by " + username);
    request->send(200, "text/plain", "Race reset");
  } else if (cmd == "r" && raceStarted) {
    handleLap(&redLane, "Red", RED_LED_PIN, nullptr);
    addDebugLog("Red lap triggered by " + username);
    sendRaceData();
    request->send(200, "text/plain", "Red lap triggered");
  } else if (cmd == "y" && raceStarted) {
    handleLap(&yellowLane, "Yellow", YELLOW_LED_PIN, nullptr);
    addDebugLog("Yellow lap triggered by " + username);
    sendRaceData();
    request->send(200, "text/plain", "Yellow lap triggered");
  } else if (cmd == "b" && raceStarted) {
    handleLap(&blueLane, "Blue", BLUE_LED_PIN, nullptr);
    addDebugLog("Blue lap triggered by " + username);
    sendRaceData();
    request->send(200, "text/plain", "Blue lap triggered");
  } else {
    addDebugLog("Invalid command: " + cmd + " by " + username);
    request->send(400, "text/plain", "Invalid command");
  }
});

  // Data route
  server.on("/data", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "{";
    json += "\"raceStarted\":" + String(raceStarted ? "true" : "false") + ",";
    json += "\"raceStatus\":\"" + String(raceStarted ? (yellowLane.startSequencePhase == 1 ? "3" : yellowLane.startSequencePhase == 2 ? "2" : yellowLane.startSequencePhase == 3 ? "1" : yellowLane.startSequencePhase == 4 ? "GO!" : "Running") : "Not Started") + "\",";
    json += "\"yellow\":{\"laps\":" + String(yellowLane.lapCount - 1) + ",\"current\":" + String(yellowLane.lapCount) + ",\"last\":\"" + formatTime(yellowLane.lastLapTime) + "\",\"best\":\"" + formatTime(yellowLane.bestLapTime) + "\",\"history\":[";
    int historyCount = min(yellowLane.lapCount - 1, 50);
    for (int i = historyCount - 1; i >= 0; i--) {
      json += "\"" + yellowLane.history[i] + "\"" + (i > 0 ? "," : "");
    }
    json += "],\"username\":\"" + yellowLane.username + "\",\"carNumber\":\"" + yellowLane.carNumber + "\",\"carColor\":\"" + yellowLane.carColor + "\"},";
    json += "\"red\":{\"laps\":" + String(redLane.lapCount - 1) + ",\"current\":" + String(redLane.lapCount) + ",\"last\":\"" + formatTime(redLane.lastLapTime) + "\",\"best\":\"" + formatTime(redLane.bestLapTime) + "\",\"history\":[";
    historyCount = min(redLane.lapCount - 1, 50);
    for (int i = historyCount - 1; i >= 0; i--) {
      json += "\"" + redLane.history[i] + "\"" + (i > 0 ? "," : "");
    }
    json += "],\"username\":\"" + redLane.username + "\",\"carNumber\":\"" + redLane.carNumber + "\",\"carColor\":\"" + redLane.carColor + "\"},";
    json += "\"blue\":{\"laps\":" + String(blueLane.lapCount - 1) + ",\"current\":" + String(blueLane.lapCount) + ",\"last\":\"" + formatTime(blueLane.lastLapTime) + "\",\"best\":\"" + formatTime(blueLane.bestLapTime) + "\",\"history\":[";
    historyCount = min(blueLane.lapCount - 1, 50);
    for (int i = historyCount - 1; i >= 0; i--) {
      json += "\"" + blueLane.history[i] + "\"" + (i > 0 ? "," : "");
    }
    json += "],\"username\":\"" + blueLane.username + "\",\"carNumber\":\"" + blueLane.carNumber + "\",\"carColor\":\"" + blueLane.carColor + "\"}";
    json += "}";
    request->send(200, "application/json", json);
  });

  // Login route
  server.on("/login", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("username", true) || !request->hasParam("carNumber", true) ||
        !request->hasParam("carColor", true) || !request->hasParam("lane", true)) {
      addDebugLog("Login failed: Missing parameters");
      request->send(400, "text/plain", "Missing parameters");
      return;
    }

    String username = request->getParam("username", true)->value();
    String password = request->hasParam("password", true) ? request->getParam("password", true)->value() : "";
    String carNumber = request->getParam("carNumber", true)->value();
    String carColor = request->getParam("carColor", true)->value();
    String lane = request->getParam("lane", true)->value();
    String role = "player";
    String sessionId = String(millis()) + "_" + String(random(1000, 9999));

    if (username.equalsIgnoreCase(MARSHALL_USERNAME)) {
      if (password != MARSHALL_PASSWORD) {
        addDebugLog("Track Marshall login failed: Invalid password for " + username);
        request->send(401, "text/plain", "Invalid password");
        return;
      }
      role = "marshall";
      lane = "Spectator";
      carNumber = "0";
      carColor = "None";
      addDebugLog("Track Marshall login initiated: " + username);
    } else if (lane == "Spectator") {
      role = "spectator";
    }

    for (int i = 0; i < spectatorCount; i++) {
      if (spectators[i] == username) {
        addDebugLog("Login failed: Username already in use: " + username);
        request->send(400, "text/plain", "Username already in use");
        return;
      }
    }
    if (yellowLane.username == username || redLane.username == username || blueLane.username == username) {
      addDebugLog("Login failed: Username already in use: " + username);
      request->send(400, "text/plain", "Username already in use");
      return;
    }

    if (lane == "Spectator") {
      if (spectatorCount >= 10 && role != "marshall") {
        addDebugLog("Login failed: Spectator limit reached for " + username);
        request->send(400, "text/plain", "Spectator limit reached");
        return;
      }
      spectators[spectatorCount++] = username;
      sessions[username] = {sessionId, millis(), role};
      addDebugLog("Login successful: " + username + ", role: " + role + ", sessionId: " + sessionId);
      StaticJsonDocument<200> doc;
      doc["role"] = role;
      doc["sessionId"] = sessionId;
      String response;
      serializeJson(doc, response);
      request->send(200, "application/json", response);
      return;
    }

    Lane *selectedLane = nullptr;
    if (lane == "Yellow" && yellowLane.username == "") selectedLane = &yellowLane;
    else if (lane == "Red" && redLane.username == "") selectedLane = &redLane;
    else if (lane == "Blue" && blueLane.username == "") selectedLane = &blueLane;
    else {
      addDebugLog("Login failed: Lane unavailable or invalid: " + lane);
      request->send(400, "text/plain", "Lane unavailable or invalid");
      return;
    }

    selectedLane->username = username;
    selectedLane->carNumber = carNumber;
    selectedLane->carColor = carColor;
    selectedLane->currentLap = 1;
    selectedLane->lastLapTime = 0;
    selectedLane->bestLapTime = ULONG_MAX;
    for (int i = 0; i < 50; i++) selectedLane->history[i] = "";
    selectedLane->lapCount = 1;
    selectedLane->startTime = 0;
    selectedLane->prevLapTimestamp = 0;
    selectedLane->startDisplayTime = 0;
    selectedLane->startSequencePhase = 0;
    selectedLane->pulseState = 0;
    selectedLane->flashCount = 0;
    selectedLane->displayLapTime = false;
    selectedLane->pulseStartTime = 0;
    selectedLane->displayTimeStart = 0;

    User &user = users[username];
    user.username = username;
    user.carNumber = carNumber;
    user.carColor = carColor;
    user.lane = lane;
    sessions[username] = {sessionId, millis(), role};
    saveUsers();
    addDebugLog("Login successful: " + username + ", role: " + role + ", lane: " + lane + ", sessionId: " + sessionId);

    StaticJsonDocument<200> doc;
    doc["role"] = role;
    doc["sessionId"] = sessionId;
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });

  // Logout route
    server.on("/logout", HTTP_POST, [](AsyncWebServerRequest *request) {
      if (!request->hasParam("username", true) || !request->hasParam("sessionId", true)) {
        addDebugLog("Logout failed: Missing username or sessionId");
        request->send(400, "text/plain", "Missing username or sessionId");
        return;
      }
      String username = request->getParam("username", true)->value();
      String sessionId = request->getParam("sessionId", true)->value();
      bool isSpectator = false;
      for (int i = 0; i < spectatorCount; i++) {
        if (spectators[i] == username) {
          spectators[i] = spectators[spectatorCount - 1];
          spectators[spectatorCount - 1] = "";
          spectatorCount--;
          isSpectator = true;
          break;
        }
      }
      if (sessions.find(username) != sessions.end()) {
        if (sessions[username].sessionId != sessionId) {
          addDebugLog("Logout failed: Invalid sessionId for user: " + username + ", provided: " + sessionId);
          request->send(401, "text/plain", "Invalid sessionId");
          return;
        }
        sessions.erase(username);
        if (isSpectator) {
          addDebugLog("Logout successful: Spectator " + username + ", sessionId: " + sessionId);
          request->send(200, "text/plain", "Logout successful");
        } else if (yellowLane.username == username) {
          yellowLane = Lane();
          if (users.find(username) != users.end()) {
            users[username].lane = "";
          }
          addDebugLog("Logout successful: Player " + username + " from Yellow lane, sessionId: " + sessionId);
          request->send(200, "text/plain", "Logout successful");
        } else if (redLane.username == username) {
          redLane = Lane();
          if (users.find(username) != users.end()) {
            users[username].lane = "";
          }
          addDebugLog("Logout successful: Player " + username + " from Red lane, sessionId: " + sessionId);
          request->send(200, "text/plain", "Logout successful");
        } else if (blueLane.username == username) {
          blueLane = Lane();
          if (users.find(username) != users.end()) {
            users[username].lane = "";
          }
          addDebugLog("Logout successful: Player " + username + " from Blue lane, sessionId: " + sessionId);
          request->send(200, "text/plain", "Logout successful");
        } else {
          addDebugLog("Logout successful: User " + username + ", sessionId: " + sessionId);
          request->send(200, "text/plain", "Logout successful");
        }
      } else {
        addDebugLog("Logout: No session found for user: " + username + ", sessionId: " + sessionId);
        request->send(200, "text/plain", "Logout successful"); // Allow logout even if session is gone
      }
    });

  // Lanes route
  server.on("/lanes", HTTP_GET, [](AsyncWebServerRequest *request) {
    StaticJsonDocument<200> doc;
    doc["yellow"] = yellowLane.username == "" ? "free" : "taken";
    doc["red"] = redLane.username == "" ? "free" : "taken";
    doc["blue"] = blueLane.username == "" ? "free" : "taken";
    String response;
    serializeJson(doc, response);
    addDebugLog("Lane status requested");
    request->send(200, "application/json", response);
  });

  // Stats route
  server.on("/stats", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("username")) {
      String username = request->getParam("username")->value();
      if (users.find(username) != users.end()) {
        DynamicJsonDocument doc(2048);
        JsonObject user = doc.to<JsonObject>();
        user["username"] = username;
        user["carNumber"] = users[username].carNumber;
        user["carColor"] = users[username].carColor;
        user["bestLapTime"] = users[username].bestLapTime == ULONG_MAX ? "--:--.---" : formatTime(users[username].bestLapTime);
        JsonArray laps = user.createNestedArray("lapHistory");
        for (unsigned long lap : users[username].lapHistory) {
          laps.add(formatTime(lap));
        }
        String json;
        serializeJson(doc, json);
        addDebugLog("Stats requested for " + username);
        request->send(200, "application/json", json);
      } else {
        addDebugLog("Stats request failed: User not found: " + username);
        request->send(404, "text/plain", "User not found");
      }
    } else {
      addDebugLog("Stats request failed: Missing username");
      request->send(400, "text/plain", "Missing username");
    }
  });

  // Settings route (POST for race settings)
  server.on("/settings", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("sessionId", true)) {
      request->send(400, "text/plain", "Missing sessionId");
      return;
    }
    String sessionId = request->getParam("sessionId", true)->value();
    bool found = false;
    String username;
    for (const auto& session : sessions) {
      if (session.second.sessionId == sessionId) {
        username = session.first;
        found = true;
        break;
      }
    }
    if (!found) {
      request->send(401, "text/plain", "Invalid sessionId");
      return;
    }
    if (sessions[username].role != "marshall") {
      request->send(403, "text/plain", "Forbidden: Track Marshall only");
      return;
    }

    String body;
    if (request->hasParam("body", true)) {
      body = request->getParam("body", true)->value();
    } else {
      request->send(400, "text/plain", "Missing body");
      return;
    }

    DynamicJsonDocument doc(512);
    DeserializationError error = deserializeJson(doc, body);
    if (error) {
      addDebugLog("Settings parse error: " + String(error.c_str()));
      request->send(400, "text/plain", "Invalid JSON");
      return;
    }

    raceDurationSeconds = doc["raceDuration"] | 0;
    countdownTimeSeconds = doc["countdownTime"] | 0;
    detectionThreshold = doc["detectionThreshold"] | DETECTION_THRESHOLD;
    lapCountingEnabled = doc["lapCountingEnabled"] | true;

    addDebugLog("Settings updated by " + username + ": raceDuration=" + String(raceDurationSeconds) +
                ", countdownTime=" + String(countdownTimeSeconds) +
                ", detectionThreshold=" + String(detectionThreshold) +
                ", lapCountingEnabled=" + String(lapCountingEnabled));

    request->send(200, "text/plain", "Settings updated");
  });

  // Settings route (POST for maxLapCount, minLapTime, displayBrightness)
  server.on("/settings", HTTP_POST, [](AsyncWebServerRequest *request){
    String username = request->hasParam("username", true) ? request->getParam("username", true)->value() : "";
    if (sessions.find(username) == sessions.end() || sessions[username].role != "marshall") {
      addDebugLog("Settings update denied for " + username);
      request->send(403, "text/plain", "Unauthorized");
      return;
    }
    if (request->hasParam("maxLapCount", true)) {
      maxLapCount = request->getParam("maxLapCount", true)->value().toInt();
    }
    if (request->hasParam("minLapTime", true)) {
      minLapTime = request->getParam("minLapTime", true)->value().toInt();
    }
    if (request->hasParam("displayBrightness", true)) {
      displayBrightness = request->getParam("displayBrightness", true)->value().toInt();
      displayBrightness = constrain(displayBrightness, 0, 15);
      portENTER_CRITICAL(&timerMux);
      // lcRed.setIntensity(0, displayBrightness); // MAX7219 disabled
      // lcYellow.setIntensity(0, displayBrightness); // MAX7219 disabled
      // lcBlue.setIntensity(0, displayBrightness); // MAX7219 disabled
      portEXIT_CRITICAL(&timerMux);
    }
    addDebugLog("Settings updated by " + username + ": maxLapCount=" + String(maxLapCount) + ", minLapTime=" + String(minLapTime) + ", displayBrightness=" + String(displayBrightness));
    request->send(200, "text/plain", "Settings updated");
  });

  // Debug route
server.on("/debug", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("/debug route called, Free Heap: " + String(ESP.getFreeHeap()));
    if (!request->hasParam("sessionId")) {
        addDebugLog("Debug request failed: Missing sessionId");
        request->send(400, "text/plain", "Missing sessionId");
        return;
    }
    String sessionId = request->getParam("sessionId")->value();
    bool found = false;
    String username;
    for (const auto& session : sessions) {
        if (session.second.sessionId == sessionId) {
            username = session.first;
            found = true;
            break;
        }
    }
    if (!found) {
        addDebugLog("Debug request failed: Invalid sessionId: " + sessionId);
        request->send(401, "text/plain", "Invalid sessionId");
        return;
    }
    if (sessions[username].role != "marshall") {
        addDebugLog("Debug request failed: User not Track Marshall: " + username);
        request->send(403, "text/plain", "Forbidden: Track Marshall only");
        return;
    }
    sessions[username].lastActive = millis();
    DynamicJsonDocument doc(2048);
    JsonObject system = doc.createNestedObject("system");
    system["freeHeap"] = ESP.getFreeHeap();
    system["uptime"] = millis() / 1000;
    system["loopExecutionTime"] = loopExecutionTime;
    JsonObject sensors = doc.createNestedObject("sensors");
    sensors["red"] = lastRedDistance >= 0 ? String(lastRedDistance) + " mm" : "Error";
    sensors["yellow"] = lastYellowDistance >= 0 ? String(lastYellowDistance) + " mm" : "Error";
    sensors["blue"] = lastBlueDistance >= 0 ? String(lastBlueDistance) + " mm" : "Error";
    JsonArray logs = doc.createNestedArray("logs");
    for (const String& log : debugLogs) {
        logs.add(log);
    }
    String response;
    serializeJson(doc, response);
    Serial.println("/debug JSON constructed, Free Heap: " + String(ESP.getFreeHeap()));
    request->send(200, "application/json", response);
    Serial.println("/debug response sent, Free Heap: " + String(ESP.getFreeHeap()));
});

  // Clear logs route
  server.on("/clearLogs", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("sessionId", true)) {
      request->send(400, "text/plain", "Missing sessionId");
      return;
    }
    String sessionId = request->getParam("sessionId", true)->value();
    bool found = false;
    String username;
    for (const auto& session : sessions) {
      if (session.second.sessionId == sessionId) {
        username = session.first;
        found = true;
        break;
      }
    }
    if (!found) {
      request->send(401, "text/plain", "Invalid sessionId");
      return;
    }
    if (sessions[username].role != "marshall") {
      request->send(403, "text/plain", "Forbidden: Track Marshall only");
      return;
    }

    debugLogs.clear();
    addDebugLog("Debug logs cleared by " + username);
    request->send(200, "text/plain", "Logs cleared");
  });

  // Check session route
  server.on("/check-session", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!request->hasParam("username")) {
      addDebugLog("Session check failed: Missing username");
      request->send(400, "text/plain", "Missing username");
      return;
    }

    String username = request->getParam("username")->value();
    if (sessions.find(username) != sessions.end()) {
      sessions[username].lastActive = millis();
      StaticJsonDocument<200> doc;
      doc["role"] = sessions[username].role;
      String response;
      serializeJson(doc, response);
      addDebugLog("Session check successful for " + username + ", role: " + sessions[username].role);
      request->send(200, "application/json", response);
    } else {
      addDebugLog("Session check failed: No session for " + username);
      request->send(404, "text/plain", "No active session");
    }
  });

  // Validate session route
server.on("/validate-session", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("sessionId")) {
        Serial.println("DEBUG: Session validation failed: Missing sessionId");
        request->send(400, "text/plain", "Missing sessionId");
        return;
    }
    String sessionId = request->getParam("sessionId")->value();
    Serial.print("DEBUG: Validating sessionId: ");
    Serial.println(sessionId);

    bool found = false;
    String username, role;
    for (const auto& session : sessions) {
        if (session.second.sessionId == sessionId) {
            found = true;
            username = session.first;
            role = session.second.role;
            sessions[username].lastActive = millis();
            Serial.print("DEBUG: Session found for ");
            Serial.print(username);
            Serial.print(", lastActive updated to ");
            Serial.println(sessions[username].lastActive);
            break;
        }
    }

    if (found) {
        DynamicJsonDocument doc(512);
        doc["username"] = username;
        doc["role"] = role;
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    } else {
        Serial.print("DEBUG: Session validation failed: No session for sessionId: ");
        Serial.println(sessionId);
        request->send(404, "text/plain", "Session not found");
    }
});

  // Adjust lap route
  server.on("/adjustLap", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("username", true) || !request->hasParam("lane", true) || !request->hasParam("adjustment", true)) {
      addDebugLog("Lap adjustment failed: Missing parameters");
      request->send(400, "text/plain", "Missing parameters");
      return;
    }

    String username = request->getParam("username", true)->value();
    String lane = request->getParam("lane", true)->value();
    String adjustment = request->getParam("adjustment", true)->value();

    if (sessions.find(username) == sessions.end() || sessions[username].role != "marshall") {
      addDebugLog("Lap adjustment failed: User not authorized: " + username);
      request->send(403, "text/plain", "Not authorized");
      return;
    }

    Lane *selectedLane = nullptr;
    if (lane == "yellow") selectedLane = &yellowLane;
    else if (lane == "red") selectedLane = &redLane;
    else if (lane == "blue") selectedLane = &blueLane;
    else {
      addDebugLog("Lap adjustment failed: Invalid lane: " + lane);
      request->send(400, "text/plain", "Invalid lane");
      return;
    }

    if (adjustment == "increment") {
      selectedLane->lapCount++;
      addDebugLog("Lap incremented for " + lane + " lane by " + username);
    } else if (adjustment == "decrement" && selectedLane->lapCount > 1) {
      selectedLane->lapCount--;
      addDebugLog("Lap decremented for " + lane + " lane by " + username);
    } else {
      addDebugLog("Lap adjustment failed: Invalid adjustment or lap count for " + lane);
      request->send(400, "text/plain", "Invalid adjustment or lap count");
      return;
    }

    sendRaceData();
    request->send(200, "text/plain", "Lap adjusted");
  });

// Kick driver route
server.on("/kickDriver", HTTP_POST, [](AsyncWebServerRequest *request) {
  if (!request->hasParam("username", true) || !request->hasParam("lane", true)) {
    addDebugLog("Kick driver failed: Missing username or lane");
    request->send(400, "text/plain", "Missing username or lane");
    return;
  }

  String username = request->getParam("username", true)->value();
  String lane = request->getParam("lane", true)->value();

  if (sessions.find(username) == sessions.end() || sessions[username].role != "marshall") {
    addDebugLog("Kick driver failed: User not authorized: " + username);
    request->send(403, "text/plain", "Not authorized");
    return;
  }

  Lane* selectedLane = nullptr;
  String laneName;
  if (lane == "yellow") {
    selectedLane = &yellowLane;
    laneName = "Yellow";
  } else if (lane == "red") {
    selectedLane = &redLane;
    laneName = "Red";
  } else if (lane == "blue") {
    selectedLane = &blueLane;
    laneName = "Blue";
  } else {
    addDebugLog("Kick driver failed: Invalid lane: " + lane);
    request->send(400, "text/plain", "Invalid lane");
    return;
  }

  if (selectedLane->username.isEmpty()) {
    addDebugLog("Kick driver failed: No driver in " + laneName + " lane");
    request->send(400, "text/plain", "No driver in lane");
    return;
  }

  String driverUsername = selectedLane->username;
  if (sessions.find(driverUsername) != sessions.end()) {
    sessions.erase(driverUsername);
    addDebugLog("Session removed for kicked driver: " + driverUsername + " from " + laneName + " lane");
  }

  if (users.find(driverUsername) != users.end()) {
    users[driverUsername].lane = "";
    saveUsers();
  }

  *selectedLane = Lane();
  addDebugLog("Driver kicked from " + laneName + " by " + username + ": " + driverUsername);
  sendRaceData();
  request->send(200, "text/plain", "Driver kicked");
});

  // Add WebSocket handler
  server.addHandler(&ws);

  // Start server
  if (WiFi.status() == WL_CONNECTED) {
    server.begin();
    Serial.println("Web server started");
  } else {
    Serial.println("Web server not started due to WiFi failure");
  }

  // Final initialization
  loadUsers();
  // Watchdog Timer - ESP32 Arduino Core 2.x compatible
  esp_task_wdt_init(10, true); // 10 seconds, panic on timeout
  esp_task_wdt_add(NULL);
  Serial.println("Setup complete");
}

void sendRaceData() {
  unsigned long currentTime = millis();
  if (currentTime - lastRaceDataSendTime < RACE_DATA_SEND_INTERVAL_MS) {
    return;
  }
  lastRaceDataSendTime = currentTime;
  if (ws.count() == 0) {
    return;
  }
  if (ws.count() > 5) {
    Serial.println("DEBUG: Too many WebSocket clients: " + String(ws.count()));
    return;
  }
  if (ESP.getFreeHeap() < 8192) {
    Serial.println("WARNING: Low heap (" + String(ESP.getFreeHeap()) + " bytes), skipping sendRaceData");
    return;
  }
  static unsigned long lastHeapLog = 0;
  if (currentTime - lastHeapLog >= 10000) { // Log every 10 seconds
    Serial.println("DEBUG: sendRaceData started, Free Heap: " + String(ESP.getFreeHeap()));
    lastHeapLog = currentTime;
  }
  DynamicJsonDocument doc(512);
  doc["raceStarted"] = raceStarted;
  doc["raceStatus"] = raceStarted ? (yellowLane.startSequencePhase == 1 ? "3" :
                                     yellowLane.startSequencePhase == 2 ? "2" :
                                     yellowLane.startSequencePhase == 3 ? "1" :
                                     yellowLane.startSequencePhase == 4 ? "GO!" : "Running") : "Not Started";
  JsonObject yellow = doc.createNestedObject("yellow");
  yellow["laps"] = yellowLane.lapCount - 1;
  yellow["current"] = yellowLane.lapCount;
  yellow["last"] = formatTime(yellowLane.lastLapTime);
  yellow["best"] = formatTime(yellowLane.bestLapTime);
  yellow["username"] = yellowLane.username;
  yellow["carNumber"] = yellowLane.carNumber;
  yellow["carColor"] = yellowLane.carColor;
  JsonObject red = doc.createNestedObject("red");
  red["laps"] = redLane.lapCount - 1;
  red["current"] = redLane.lapCount;
  red["last"] = formatTime(redLane.lastLapTime);
  red["best"] = formatTime(redLane.bestLapTime);
  red["username"] = redLane.username;
  red["carNumber"] = redLane.carNumber;
  red["carColor"] = redLane.carColor;
  JsonObject blue = doc.createNestedObject("blue");
  blue["laps"] = blueLane.lapCount - 1;
  blue["current"] = blueLane.lapCount;
  blue["last"] = formatTime(blueLane.lastLapTime);
  blue["best"] = formatTime(blueLane.bestLapTime);
  blue["username"] = blueLane.username;
  blue["carNumber"] = blueLane.carNumber;
  blue["carColor"] = blueLane.carColor;
  String json;
  size_t jsonSize = serializeJson(doc, json);
  if (currentTime - lastHeapLog >= 10000) {
    Serial.println("DEBUG: JSON size: " + String(jsonSize) + ", Free Heap: " + String(ESP.getFreeHeap()));
  }
  ws.textAll(json);
}

void loadUsers() {
  File file = LittleFS.open("/users.json", "r");
  if (!file) {
    Serial.println("No users.json found, starting fresh");
    return;
  }
  DynamicJsonDocument doc(4096);
  DeserializationError error = deserializeJson(doc, file);
  if (error) {
    Serial.println("Failed to parse users.json: " + String(error.c_str()));
    file.close();
    return;
  }
  file.close();
  for (JsonObject user : doc.as<JsonArray>()) {
    User u;
    u.username = user["username"].as<String>();
    u.carNumber = user["carNumber"].as<String>();
    u.carColor = user["carColor"].as<String>();
    u.lane = user["lane"].as<String>();
    u.bestLapTime = user["bestLapTime"].as<unsigned long>();
    JsonArray laps = user["lapHistory"];
    for (unsigned long lap : laps) {
      u.lapHistory.push_back(lap);
    }
    users[u.username] = u;
  }
  Serial.println("Loaded " + String(users.size()) + " users");
}

void saveUsers() {
    unsigned long currentTime = millis();
    if (currentTime - lastSaveUsersTime < SAVE_USERS_THROTTLE_MS) {
        Serial.println("Throttling saveUsers() call");
        return;
    }
    lastSaveUsersTime = currentTime;

    File file = LittleFS.open("/users.json", "w");
    if (!file) {
        Serial.println("Failed to open users.json for writing");
        return;
    }
    DynamicJsonDocument doc(4096);
    JsonArray array = doc.to<JsonArray>();
    for (auto& pair : users) {
        JsonObject user = array.createNestedObject();
        user["username"] = pair.second.username;
        user["carNumber"] = pair.second.carNumber;
        user["carColor"] = pair.second.carColor;
        user["lane"] = pair.second.lane;
        user["bestLapTime"] = pair.second.bestLapTime;
        JsonArray laps = user.createNestedArray("lapHistory");
        for (unsigned long lap : pair.second.lapHistory) {
            laps.add(lap);
        }
    }
    if (serializeJson(doc, file) == 0) {
        Serial.println("Failed to write to users.json");
    }
    file.close();
    Serial.println("Users saved to LittleFS");
}

void printUsersJson() {
    Serial.println("=== Current users.json contents ===");
    File file = LittleFS.open("/users.json", "r");
    if (!file) {
        Serial.println("No users.json file found");
        return;
    }
    Serial.println("File size: " + String(file.size()) + " bytes");
    if (file.size() == 0) {
        Serial.println("users.json is empty");
        file.close();
        return;
    }
    while (file.available()) {
        Serial.write(file.read());
    }
    file.close();
    Serial.println("\n=== End users.json contents ===");
}

bool validateSession(String sessionId, String username) {
    // Check if session exists for the username
    if (sessions.find(username) == sessions.end()) {
        return false;
    }
    // Check if sessionId matches
    if (sessions[username].sessionId != sessionId) {
        return false;
    }
    // Update last active time
    sessions[username].lastActive = millis();
    return true;
}

String formatTime(unsigned long ms) {
  if (ms == ULONG_MAX || ms == 0) return "--:--";
  unsigned long minutes = ms / 60000;
  ms %= 60000;
  unsigned long seconds = ms / 1000;
  unsigned long millisecs = ms % 1000;
  char buffer[12];
  snprintf(buffer, 12, "%02lu:%02lu.%03lu", minutes, seconds, millisecs);
  return String(buffer);
}

void addDebugLog(String message) {
    debugLogs.push_back(message);
    if (debugLogs.size() > 25) { // Changed from 50
        debugLogs.erase(debugLogs.begin());
    }
    Serial.println("DEBUG: " + message);
}


void startSequence() {
  // Flash LEDs: Red -> Yellow -> Blue, 500ms each
  digitalWrite(RED_LED_PIN, HIGH);
  delay(500);
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(YELLOW_LED_PIN, HIGH);
  delay(500);
  digitalWrite(YELLOW_LED_PIN, LOW);
  digitalWrite(BLUE_LED_PIN, HIGH);
  delay(500);
  digitalWrite(BLUE_LED_PIN, LOW);
  Serial.println("DEBUG: Lightshow completed");
}

void startRace() {
  if (raceStarted) {
    Serial.println("DEBUG: Race already started");
    return;
  }
  esp_task_wdt_reset();
  Serial.println("startRace() started, Free Heap: " + String(ESP.getFreeHeap()));
  portENTER_CRITICAL(&raceMux);
  raceStarted = true;
  startTime = millis();
  yellowLane.startSequencePhase = 1;
  redLane.startSequencePhase = 1;
  blueLane.startSequencePhase = 1;
  unsigned long phaseDuration = countdownTimeSeconds > 0 ? (countdownTimeSeconds * 1000 / 4) : 1250;
  lastPhaseChange = startTime;
  phaseChangeInterval = phaseDuration;
  yellowLane.startDisplayTime = startTime;
  redLane.startDisplayTime = startTime;
  blueLane.startDisplayTime = startTime;
  portEXIT_CRITICAL(&raceMux);
  startSequence(); // Add lightshow here
  addDebugLog("Race started at " + String(startTime));
  Serial.println("Updating displays...");
  updateDisplay(&redLane, nullptr, "Red"); esp_task_wdt_reset();
  updateDisplay(&yellowLane, nullptr, "Yellow"); esp_task_wdt_reset();
  updateDisplay(&blueLane, nullptr, "Blue"); esp_task_wdt_reset();
  Serial.println("Calling sendRaceData, Free Heap: " + String(ESP.getFreeHeap()));
  if (ESP.getFreeHeap() > 8192) {
    sendRaceData();
  } else {
    Serial.println("WARNING: Low heap (" + String(ESP.getFreeHeap()) + " bytes), skipping sendRaceData");
  }
  esp_task_wdt_reset();
  Serial.println("startRace() completed");
}

void resetRace() {
    raceStarted = false;
    
    // Save player settings before clearing lane data
    String redUsername = redLane.username;
    String redCarNumber = redLane.carNumber;
    String redCarColor = redLane.carColor;
    String yellowUsername = yellowLane.username;
    String yellowCarNumber = yellowLane.carNumber;
    String yellowCarColor = yellowLane.carColor;
    String blueUsername = blueLane.username;
    String blueCarNumber = blueLane.carNumber;
    String blueCarColor = blueLane.carColor;
    
    // Clear lane race data but preserve player assignments
    redLane = Lane { redUsername, redCarNumber, redCarColor, 1, 0, ULONG_MAX, {}, 1, 0, 0, 0, 0, 0, 0, false, 0, 0 };
    yellowLane = Lane { yellowUsername, yellowCarNumber, yellowCarColor, 1, 0, ULONG_MAX, {}, 1, 0, 0, 0, 0, 0, 0, false, 0, 0 };
    blueLane = Lane { blueUsername, blueCarNumber, blueCarColor, 1, 0, ULONG_MAX, {}, 1, 0, 0, 0, 0, 0, 0, false, 0, 0 };
    
    // Clear user lap history using saved usernames
    if (!redUsername.isEmpty()) {
        users[redUsername].lapHistory.clear();
        users[redUsername].bestLapTime = ULONG_MAX;
    }
    if (!yellowUsername.isEmpty()) {
        users[yellowUsername].lapHistory.clear();
        users[yellowUsername].bestLapTime = ULONG_MAX;
    }
    if (!blueUsername.isEmpty()) {
        users[blueUsername].lapHistory.clear();
        users[blueUsername].bestLapTime = ULONG_MAX;
    }
    saveUsers();
    digitalWrite(RED_LED_PIN, LOW);
    digitalWrite(YELLOW_LED_PIN, LOW);
    digitalWrite(BLUE_LED_PIN, LOW);
    portENTER_CRITICAL(&timerMux);
    // lcRed.clearDisplay(0); // MAX7219 disabled
    // lcYellow.clearDisplay(0); // MAX7219 disabled
    // lcBlue.clearDisplay(0); // MAX7219 disabled
    portEXIT_CRITICAL(&timerMux);
    updateDisplay(&redLane, nullptr, "Red");
    updateDisplay(&yellowLane, nullptr, "Yellow");
    updateDisplay(&blueLane, nullptr, "Blue");
    addDebugLog("RACE RESET: All lanes reset, displays cleared");
    sendRaceData();
}

void handleLap(Lane* lane, String laneName, int ledPin, void* lc) {
    if (!lane || !raceStarted || !lapCountingEnabled || lane->startSequencePhase != 0) {
        Serial.println("DEBUG: Lap ignored for " + laneName + ": Invalid state");
        return;
    }
    unsigned long currentTime = millis();
    unsigned long lapTime = (lane->lapCount == 1) ? (currentTime - startTime) : (currentTime - lane->prevLapTimestamp);
    if (lapTime < minLapTime) {
        addDebugLog("Lap ignored for " + laneName + ": Time " + String(lapTime) + "ms below minimum " + String(minLapTime) + "ms");
        return;
    }
    if (lane->lapCount >= 9999 || (maxLapCount > 0 && lane->lapCount > maxLapCount)) {
        addDebugLog("Lap ignored for " + laneName + ": Max lap count reached");
        return;
    }
    lane->lastLapTime = lapTime;
    lane->prevLapTimestamp = currentTime;
    if (lane->lapCount <= 50) {
        lane->history[lane->lapCount - 1] = formatTime(lapTime);
    }
    lane->lapCount++;
    bool isBestLap = lapTime < lane->bestLapTime && lapTime > 0;
    if (isBestLap) {
        lane->bestLapTime = lapTime;
        lane->pulseState = 2; // Fast pulse for best lap
        lane->flashCount = 0;
    } else {
        lane->pulseState = 1; // Normal pulse
    }
    lane->pulseStartTime = currentTime;
    lane->displayLapTime = true;
    lane->displayTimeStart = currentTime;
    updateLED(lane, ledPin); // Turn on lane-specific LED
    if (!lane->username.isEmpty()) {
        users[lane->username].lapHistory.push_back(lapTime);
        if (users[lane->username].lapHistory.size() > 50) {
            users[lane->username].lapHistory.erase(users[lane->username].lapHistory.begin());
        }
        if (isBestLap) {
            users[lane->username].bestLapTime = lapTime;
        }
        saveUsers();
    }
    if (lc != nullptr) {
        updateDisplay(lane, lc, laneName);
    }
    addDebugLog("LAP TRIGGERED: " + laneName + ": Completed Lap " + String(lane->lapCount - 1) + ", Time: " + formatTime(lapTime) + (isBestLap ? " (NEW BEST)" : ""));
    Serial.println("DEBUG: " + laneName + " lap triggered");
    if (ESP.getFreeHeap() > 8192) {
        sendRaceData();
    } else {
        Serial.println("WARNING: Low heap (" + String(ESP.getFreeHeap()) + " bytes), skipping sendRaceData");
    }
}

void updateDisplay(Lane* lane, void* lc, String laneName) {
    // MAX7219 displays disabled - hardware not connected
    if (lc == nullptr) return;
    
    Serial.print("Updating display for ");
    Serial.print(laneName);
    Serial.print(", phase: ");
    Serial.println(lane->startSequencePhase);

    // Quick critical section for display configuration only
    portENTER_CRITICAL(&timerMux);
    // lc->activateAllSegments(); // MAX7219 disabled
    // lc->setIntensity(displayBrightness); // MAX7219 disabled
    // lc->clearMatrix(); // MAX7219 disabled
    portEXIT_CRITICAL(&timerMux);

    if (!raceStarted) {
        Serial.println("MAX7219 " + laneName + " Updating: Blank (no race)");
        return;
    }

    if (lane->startSequencePhase > 0) {
        // Handle countdown phases
        if (lane->startSequencePhase == 1) {
            Serial.println("MAX7219 " + laneName + " Updating: 33333333");
            // Single critical section for all digits
            portENTER_CRITICAL(&timerMux);
            for (int i = 0; i < 8; i++) {
                // // lc->setDigit(0, i, 3, false); // MAX7219 disabled
            }
            portEXIT_CRITICAL(&timerMux);
        } else if (lane->startSequencePhase == 2) {
            Serial.println("MAX7219 " + laneName + " Updating: 22222222");
            portENTER_CRITICAL(&timerMux);
            for (int i = 0; i < 8; i++) {
                // // lc->setDigit(0, i, 2, false); // MAX7219 disabled
            }
            portEXIT_CRITICAL(&timerMux);
        } else if (lane->startSequencePhase == 3) {
            Serial.println("MAX7219 " + laneName + " Updating: 11111111");
            portENTER_CRITICAL(&timerMux);
            for (int i = 0; i < 8; i++) {
                // // lc->setDigit(0, i, 1, false); // MAX7219 disabled
            }
            portEXIT_CRITICAL(&timerMux);
        } else if (lane->startSequencePhase == 4) {
            // Handle GO! phase
            unsigned long elapsed = millis() - lane->startDisplayTime;
            int cycle = elapsed / 400;
            int phase = elapsed % 400;
            if (phase < 200 && cycle < 3) {
                Serial.println("MAX7219 " + laneName + " Updating: --------");
                // Single critical section for all dashes
                portENTER_CRITICAL(&timerMux);
                for (int i = 0; i < 8; i++) {
                    // // lc->setChar(0, i, '-', false); // MAX7219 disabled
                }
                portEXIT_CRITICAL(&timerMux);
            } else {
                Serial.println("MAX7219 " + laneName + " Updating: Blank (flash)");
                portENTER_CRITICAL(&timerMux);
                // lc->clearMatrix(); // MAX7219 disabled
                portEXIT_CRITICAL(&timerMux);
            }
        }
        
    } else if (lane->displayLapTime) {
        // Lap time display - prepare data outside critical section
        unsigned long ms = lane->lastLapTime;
        unsigned long minutes = ms / 60000;
        ms %= 60000;
        unsigned long seconds = ms / 1000;
        unsigned long millisecs = ms % 1000;
        char buffer[9];
        snprintf(buffer, 9, "%02lu%02lu%03lu", minutes, seconds, millisecs);
        
        Serial.print("MAX7219 " + laneName + " Updating Lap Time: ");
        Serial.println(buffer);
        
        // Single critical section for all digits
        portENTER_CRITICAL(&timerMux);
        for (int i = 0; i < 8; i++) {
            if (i == 4) {
                // lc->setChar(0, 7 - i, '.', false);
            } else {
                int digit = buffer[i < 4 ? i : i - 1] - '0';
                // lc->setDigit(0, 7 - i, digit, false);
            }
        }
        portEXIT_CRITICAL(&timerMux);
        
    } else {
        // Lap count display
        Serial.print("MAX7219 " + laneName + " Updating Lap Count: LAP ");
        Serial.println(lane->lapCount);
        
        // Single critical section for lap count display
        portENTER_CRITICAL(&timerMux);
        // lc->setChar(0, 7, 'L', false);
        // lc->setChar(0, 6, 'A', false);
        // lc->setChar(0, 5, 'P', false);
        // lc->setChar(0, 4, ' ', false);
        int lap = lane->lapCount;
        if (lap > 9999) lap = 9999;
        int thousands = (lap / 1000) % 10;
        int hundreds = (lap / 100) % 10;
        int tens = (lap / 10) % 10;
        int ones = lap % 10;
        // lc->setDigit(0, 3, thousands, false);
        // lc->setDigit(0, 2, hundreds, false);
        // lc->setDigit(0, 1, tens, false);
        // lc->setDigit(0, 0, ones, false);
        portEXIT_CRITICAL(&timerMux);
    }
}

void updateDisplayBlink(Lane* lane, void* lc, String laneName) {
    if (lc == nullptr || lane->pulseState != 2) return;
    unsigned long elapsed = millis() - lane->pulseStartTime;
    if (elapsed < 1200) {
        int cycle = elapsed / 400;
        int phase = elapsed % 400;
        portENTER_CRITICAL(&timerMux);
        if (phase < 200) {
            // lc->activateAllSegments(); // MAX7219 disabled
            // lc->setIntensity(2); // MAX7219 disabled
            // setScanLimit not needed in LedController
            unsigned long ms = lane->lastLapTime;
            unsigned long minutes = ms / 60000;
            ms %= 60000;
            unsigned long seconds = ms / 1000;
            unsigned long millisecs = ms % 1000;
            char buffer[9];
            snprintf(buffer, 9, "%02lu%02lu%03lu", minutes, seconds, millisecs);
            for (int i = 0; i < 8; i++) {
                if (i == 4) {
                    // lc->setChar(0, 7 - i, '.', false);
                } else {
                    int digit = buffer[i < 4 ? i : i - 1] - '0';
                    // lc->setDigit(0, 7 - i, digit, false);
                }
                delay(10);
            }
        } else {
            // lc->clearMatrix(); // MAX7219 disabled
        }
        portEXIT_CRITICAL(&timerMux);
        if (phase >= 399 && cycle < 2) {
            lane->flashCount++;
        }
    } else if (millis() - lane->displayTimeStart < 5000) {
        portENTER_CRITICAL(&timerMux);
        // lc->activateAllSegments(); // MAX7219 disabled
        // lc->setIntensity(2); // MAX7219 disabled
        // setScanLimit not needed in LedController
        unsigned long ms = lane->lastLapTime;
        unsigned long minutes = ms / 60000;
        ms %= 60000;
        unsigned long seconds = ms / 1000;
        unsigned long millisecs = ms % 1000;
        char buffer[9];
        snprintf(buffer, 9, "%02lu%02lu%03lu", minutes, seconds, millisecs);
        for (int i = 0; i < 8; i++) {
            if (i == 4) {
                // lc->setChar(0, 7 - i, '.', false);
            } else {
                int digit = buffer[i < 4 ? i : i - 1] - '0';
                // lc->setDigit(0, 7 - i, digit, false);
            }
            delay(10);
        }
        portEXIT_CRITICAL(&timerMux);
        Serial.print("MAX7219 Updating Lap Time (post-blink): ");
        Serial.println(buffer);
    } else {
        lane->pulseState = 0;
        lane->flashCount = 0;
        lane->displayLapTime = false;
        updateDisplay(lane, lc, laneName);
    }
}

void updateLED(Lane* lane, int ledPin) {
  static bool lastPulsePhase[3] = {false, false, false}; // Track last phase for Red, Yellow, Blue LEDs
  int ledIndex = (ledPin == RED_LED_PIN) ? 0 : (ledPin == YELLOW_LED_PIN) ? 1 : 2; // Map pin to index
  unsigned long currentTime = millis();

  if (lane->pulseState == 1) { // Normal lap
    if (currentTime - lane->pulseStartTime < 1000) {
      if (!lastPulsePhase[ledIndex]) {
        digitalWrite(ledPin, HIGH);
        Serial.println("DEBUG: LED " + String(ledPin) + " ON (normal lap)");
        lastPulsePhase[ledIndex] = true;
      }
    } else {
      if (lastPulsePhase[ledIndex]) {
        digitalWrite(ledPin, LOW);
        lane->pulseState = 0;
        Serial.println("DEBUG: LED " + String(ledPin) + " OFF (normal lap end)");
        lastPulsePhase[ledIndex] = false;
      }
    }
  } else if (lane->pulseState == 2) { // Best lap
    unsigned long elapsed = currentTime - lane->pulseStartTime;
    if (elapsed < 1200) {
      int cycle = elapsed / 400;
      int phase = elapsed % 400;
      bool newPhase = phase < 200;
      if (newPhase != lastPulsePhase[ledIndex]) {
        digitalWrite(ledPin, newPhase ? HIGH : LOW);
        Serial.println("DEBUG: LED " + String(ledPin) + " " + (newPhase ? "ON" : "OFF") + " (best lap, cycle " + String(cycle) + ")");
        lastPulsePhase[ledIndex] = newPhase;
      }
      if (phase >= 399 && cycle < 2) {
        lane->flashCount++;
      }
    } else {
      if (lastPulsePhase[ledIndex]) {
        digitalWrite(ledPin, LOW);
        lane->pulseState = 0;
        lane->flashCount = 0;
        Serial.println("DEBUG: LED " + String(ledPin) + " OFF (best lap end)");
        lastPulsePhase[ledIndex] = false;
      }
    }
  }
}

void handleSensors(unsigned long currentTime) {
    if (currentTime - lastSensorReadTime < SENSOR_READ_INTERVAL_MS) {
        return;
    }
    lastSensorReadTime = currentTime;

    VL53L0X_RangingMeasurementData_t measure;

    redSensor.rangingTest(&measure, false);
    int redDistance = (measure.RangeStatus != 4 && measure.RangeMilliMeter <= 2000) ? measure.RangeMilliMeter : -1;
    if (redDistance >= 0 && redDistance < detectionThreshold && raceStarted && lapCountingEnabled && yellowLane.startSequencePhase == 0) {
        unsigned long lapTime = (redLane.lapCount == 1) ? (currentTime - startTime) : (currentTime - redLane.prevLapTimestamp);
        if (lapTime >= minLapTime) {
            handleLap(&redLane, "Red", RED_LED_PIN, nullptr);
        }
    }
    lastRedDistance = redDistance;

    yellowSensor.rangingTest(&measure, false);
    int yellowDistance = (measure.RangeStatus != 4 && measure.RangeMilliMeter <= 2000) ? measure.RangeMilliMeter : -1;
    if (yellowDistance >= 0 && yellowDistance < detectionThreshold && raceStarted && lapCountingEnabled && yellowLane.startSequencePhase == 0) {
        unsigned long lapTime = (yellowLane.lapCount == 1) ? (currentTime - startTime) : (currentTime - yellowLane.prevLapTimestamp);
        if (lapTime >= minLapTime) {
            handleLap(&yellowLane, "Yellow", YELLOW_LED_PIN, nullptr);
        }
    }
    lastYellowDistance = yellowDistance;

    blueSensor.rangingTest(&measure, false);
    int blueDistance = (measure.RangeStatus != 4 && measure.RangeMilliMeter <= 2000) ? measure.RangeMilliMeter : -1;
    if (blueDistance >= 0 && blueDistance < detectionThreshold && raceStarted && lapCountingEnabled && yellowLane.startSequencePhase == 0) {
        unsigned long lapTime = (blueLane.lapCount == 1) ? (currentTime - startTime) : (currentTime - blueLane.prevLapTimestamp);
        if (lapTime >= minLapTime) {
            handleLap(&blueLane, "Blue", BLUE_LED_PIN, nullptr);
        }
    }
    lastBlueDistance = blueDistance;

    updateLED(&redLane, RED_LED_PIN);
    updateLED(&yellowLane, YELLOW_LED_PIN);
    updateLED(&blueLane, BLUE_LED_PIN);
    updateDisplayBlink(&redLane, nullptr, "Red");
    updateDisplayBlink(&yellowLane, nullptr, "Yellow");
    updateDisplayBlink(&blueLane, nullptr, "Blue");

    unsigned long handleSensorsEndTime = millis();
    loopExecutionTime = handleSensorsEndTime - currentTime;
}

void checkRaceStatus(unsigned long currentTime) {
    if (!raceStarted) {
        return;
    }
    if (yellowLane.startSequencePhase > 0) {
        unsigned long elapsed = currentTime - lastPhaseChange;
        if (elapsed >= phaseChangeInterval) {
            portENTER_CRITICAL(&raceMux);
            yellowLane.startSequencePhase++;
            redLane.startSequencePhase++;
            blueLane.startSequencePhase++;
            lastPhaseChange = currentTime;
            if (yellowLane.startSequencePhase == 4) {
                yellowLane.startDisplayTime = currentTime;
                redLane.startDisplayTime = currentTime;
                blueLane.startDisplayTime = currentTime;
            } else if (yellowLane.startSequencePhase > 4) {
                startTime = currentTime;
                yellowLane.startSequencePhase = 0;
                redLane.startSequencePhase = 0;
                blueLane.startSequencePhase = 0;
            }
            portEXIT_CRITICAL(&raceMux);
            updateDisplay(&redLane, nullptr, "Red");
            updateDisplay(&yellowLane, nullptr, "Yellow");
            updateDisplay(&blueLane, nullptr, "Blue");
            sendRaceData();
        }
    }
    if (raceDurationSeconds > 0 && currentTime - startTime >= raceDurationSeconds * 1000) {
        portENTER_CRITICAL(&raceMux);
        raceStarted = false;
        portEXIT_CRITICAL(&raceMux);
        addDebugLog("Race ended due to duration limit: " + String(raceDurationSeconds) + " seconds");
        digitalWrite(RED_LED_PIN, LOW);
        digitalWrite(YELLOW_LED_PIN, LOW);
        digitalWrite(BLUE_LED_PIN, LOW);
        updateDisplay(&redLane, nullptr, "Red");
        updateDisplay(&yellowLane, nullptr, "Yellow");
        updateDisplay(&blueLane, nullptr, "Blue");
        sendRaceData();
    }
}

void updateWebSocketClients() {
    static unsigned long lastWebSocketUpdate = 0;
    unsigned long currentTime = millis();
    
    // Only send WebSocket updates every 1000ms (1 second) to prevent spam
    if (currentTime - lastWebSocketUpdate >= 1000) {
        if (ws.count() > 0) {
            sendRaceData();
        }
        lastWebSocketUpdate = currentTime;
    }
}

void handleButtons() {
  static unsigned long lastButtonCheck = 0;
  unsigned long currentTime = millis();
  if (currentTime - lastButtonCheck < 50) return; // Debounce
  lastButtonCheck = currentTime;

  // Start/Reset Button
  static int lastStartResetState = HIGH;
  static unsigned long lastStartResetTrigger = 0;
  int startResetState = digitalRead(START_RESET_BUTTON_PIN);
  if (lastStartResetState != startResetState) {
    Serial.println("DEBUG: Start/Reset button state: " + String(startResetState) + " (HIGH=Open, LOW=Pressed)");
  }
  if (lastStartResetState == HIGH && startResetState == LOW && currentTime - lastStartResetTrigger > 200) {
    lastStartResetTrigger = currentTime;
    if (raceStarted) {
      resetRace();
      addDebugLog("Race reset via start/reset button");
      Serial.println("DEBUG: Start/Reset button triggered reset");
    } else {
      startRace();
      addDebugLog("Race started via start/reset button");
      Serial.println("DEBUG: Start/Reset button triggered start");
    }
  }
  lastStartResetState = startResetState;

  // Red Lane Button
  static int lastRedButtonState = HIGH;
  static unsigned long lastRedTrigger = 0;
  int redButtonState = digitalRead(RED_BUTTON_PIN);
  if (lastRedButtonState != redButtonState) {
    Serial.println("DEBUG: Red button state: " + String(redButtonState) + " (HIGH=Open, LOW=Pressed)");
  }
  if (lastRedButtonState == HIGH && redButtonState == LOW && currentTime - lastRedTrigger > 200 && raceStarted && redLane.startSequencePhase == 0) {
    lastRedTrigger = currentTime;
    handleLap(&redLane, "Red", RED_LED_PIN, nullptr);
    addDebugLog("Red lap triggered via button");
    Serial.println("DEBUG: Red button triggered lap");
  }
  lastRedButtonState = redButtonState;

  // Yellow Lane Button
  static int lastYellowButtonState = HIGH;
  static unsigned long lastYellowTrigger = 0;
  int yellowButtonState = digitalRead(YELLOW_BUTTON_PIN);
  if (lastYellowButtonState != yellowButtonState) {
    Serial.println("DEBUG: Yellow button state: " + String(yellowButtonState) + " (HIGH=Open, LOW=Pressed)");
  }
  if (lastYellowButtonState == HIGH && yellowButtonState == LOW && currentTime - lastYellowTrigger > 200 && raceStarted && yellowLane.startSequencePhase == 0) {
    lastYellowTrigger = currentTime;
    handleLap(&yellowLane, "Yellow", YELLOW_LED_PIN, nullptr);
    addDebugLog("Yellow lap triggered via button");
    Serial.println("DEBUG: Yellow button triggered lap");
  }
  lastYellowButtonState = yellowButtonState;

  // Blue Lane Button
  static int lastBlueButtonState = HIGH;
  static unsigned long lastBlueTrigger = 0;
  int blueButtonState = digitalRead(BLUE_BUTTON_PIN);
  if (lastBlueButtonState != blueButtonState) {
    Serial.println("DEBUG: Blue button state: " + String(blueButtonState) + " (HIGH=Open, LOW=Pressed)");
  }
  if (lastBlueButtonState == HIGH && blueButtonState == LOW && currentTime - lastBlueTrigger > 200 && raceStarted && blueLane.startSequencePhase == 0) {
    lastBlueTrigger = currentTime;
    handleLap(&blueLane, "Blue", BLUE_LED_PIN, nullptr);
    addDebugLog("Blue lap triggered via button");
    Serial.println("DEBUG: Blue button triggered lap");
  }
  lastBlueButtonState = blueButtonState;
}

void loop() {
    unsigned long currentTime = millis();
    esp_task_wdt_reset();
    handleButtons();

    static unsigned long lastSensorUpdate = 0;
    static unsigned long lastSessionCheck = 0;
    static bool firstRun = true;
    const unsigned long SENSOR_UPDATE_INTERVAL = 100;
    const unsigned long SESSION_CHECK_INTERVAL = 30000; // Check sessions every 30 seconds
    
    // Initialize lastSessionCheck on first run to prevent immediate session cleanup
    if (firstRun) {
        lastSessionCheck = currentTime;
        firstRun = false;
    }

    if (currentTime - lastSensorUpdate >= SENSOR_UPDATE_INTERVAL) {
        handleSensors(currentTime);
        lastSensorUpdate = currentTime;
    }
    checkRaceStatus(currentTime);
    updateWebSocketClients();

// Session cleanup (run every 10 seconds)
if (currentTime - lastSessionCheck >= SESSION_CHECK_INTERVAL) {
    Serial.println("DEBUG: Running session cleanup - checking " + String(sessions.size()) + " sessions");
    for (auto it = sessions.begin(); it != sessions.end();) {
        unsigned long lastActive = it->second.lastActive;
        unsigned long diff = currentTime - lastActive; // Simpler subtraction; unsigned arithmetic handles rollover, diff < 100 check prevents underflow        
        String sessionId = it->second.sessionId;
        String username = it->first;

        // Only log sessions that are close to timeout or expired (reduce spam)
        if (diff > (SESSION_TIMEOUT - 10000)) { // Only log in last 10 seconds before timeout
            String userType = "Unknown";
            if (username.equalsIgnoreCase("marshall")) {
                userType = "Marshall";
            } else {
                for (int i = 0; i < spectatorCount; i++) {
                    if (spectators[i] == username) {
                        userType = "Spectator";
                        break;
                    }
                }
                if (redLane.username == username || yellowLane.username == username || blueLane.username == username) {
                    userType = "Player";
                }
            }
            Serial.println("DEBUG: Session " + username + " (" + userType + ") age: " + String(diff / 1000) + "s (timeout at " + String(SESSION_TIMEOUT / 1000) + "s)");
        }

        if (diff < 1000) {
            // Skip sessions less than 1 second old
            ++it;
            continue;
        }
        
        // Reset watchdog during session cleanup to prevent crash
        esp_task_wdt_reset();

        if (it->second.role != "marshall" && diff >= SESSION_TIMEOUT) {
            String logMessage = "Session timeout: ";
            String userType = "Unknown";
            
            // Update spectators array for spectators
            for (int i = 0; i < spectatorCount; i++) {
                if (spectators[i] == username) {
                    spectators[i] = spectators[spectatorCount - 1];
                    spectators[spectatorCount - 1] = "";
                    spectatorCount--;
                    userType = "Spectator";
                    break;
                }
            }

            // Clear lane assignments for players
            if (yellowLane.username == username) {
                yellowLane = Lane();
                if (users.find(username) != users.end()) {
                    users[username].lane = "";
                }
                userType = "Player";
                logMessage += "Player " + username + " from Yellow lane";
            } else if (redLane.username == username) {
                redLane = Lane();
                if (users.find(username) != users.end()) {
                    users[username].lane = "";
                }
                userType = "Player";
                logMessage += "Player " + username + " from Red lane";
            } else if (blueLane.username == username) {
                blueLane = Lane();
                if (users.find(username) != users.end()) {
                    users[username].lane = "";
                }
                userType = "Player";
                logMessage += "Player " + username + " from Blue lane";
            } else {
                logMessage += userType + " " + username;
            }

            if (username == "marshall") {
                userType = "Marshall";
                logMessage = "Session timeout: Marshall " + username; // Should not occur due to condition
            }

            logMessage += ", sessionId: " + sessionId + ", lastActive: " + String(lastActive);
            addDebugLog(logMessage);
            Serial.println("DEBUG: " + logMessage);
            it = sessions.erase(it);
            saveUsers();
            Serial.println("Users saved to LittleFS");
        } else {
            ++it;
        }
    }
    lastSessionCheck = currentTime;
}
}