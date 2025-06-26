// Takhomasak Speedway - Multi-Lane Version with MAX7219 Lap Times
// Date: May 21, 2025
// Features: TCRT5000 sensors, MAX7219 displays, ESP32 webserver, login system, full separate html

#include <LedControl.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <esp_task_wdt.h>
#include <vector>
#include <map>
#include <LittleFS.h>
#include <ArduinoJson.h>

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
int maxLapCount = 0;
unsigned long minLapTime = 500;
int displayBrightness = 2;

// Debug log buffer
String debugLogs[50];
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
    String username;
    unsigned long lastActive;
    String role;
    String sessionId; // New field for session ID
};

// Global user and session storage
std::map<String, User> users;
std::map<String, Session> sessions;
const unsigned long SESSION_TIMEOUT = 30 * 60 * 1000;
const String MARSHALL_USERNAME = "marshall";
const String MARSHALL_PASSWORD = "track2025";

// Function prototypes
void startRace();
void resetRace();
void handleLap(Lane* lane, String laneName, int ledPin, LedControl* lc);
void sendRaceData();
String formatTime(unsigned long ms);
void updateDisplay(Lane* lane, LedControl* lc, String laneName);
void updateDisplayBlink(Lane* lane, LedControl* lc);
void updateLED(Lane* lane, int ledPin);
void loadUsers();
void saveUsers();
void addDebugLog(String message);

// Wi-Fi credentials
const char* ssid = "Lawson";
const char* password = "yamaha350";

// Static IP configuration
IPAddress local_IP(192, 168, 1, 69);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8);
IPAddress secondaryDNS(8, 8, 4, 4);

// Pin definitions
#define RED_SENSOR_PIN 4
#define YELLOW_SENSOR_PIN 18
#define BLUE_SENSOR_PIN 5
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

// MAX7219 setup
LedControl lcRed = LedControl(MAX7219_DIN, MAX7219_CLK, MAX7219_CS_RED, 1);
LedControl lcYellow = LedControl(MAX7219_DIN, MAX7219_CLK, MAX7219_CS_YELLOW, 1);
LedControl lcBlue = LedControl(MAX7219_DIN, MAX7219_CLK, MAX7219_CS_BLUE, 1);

// Webserver and WebSocket
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// Race state
bool raceStarted = false;

// Critical section mutex
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

void setup() {
  esp_task_wdt_deinit();
  Serial.begin(115200);
  delay(100);
  Serial.println("Takhomasak Speedway Multi-Lane Starting...");

  pinMode(RED_SENSOR_PIN, INPUT);
  pinMode(YELLOW_SENSOR_PIN, INPUT);
  pinMode(BLUE_SENSOR_PIN, INPUT);
  pinMode(RED_BUTTON_PIN, INPUT_PULLUP);
  pinMode(YELLOW_BUTTON_PIN, INPUT_PULLUP);
  pinMode(BLUE_BUTTON_PIN, INPUT_PULLUP);
  pinMode(START_RESET_BUTTON_PIN, INPUT_PULLUP);
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(YELLOW_LED_PIN, OUTPUT);
  pinMode(BLUE_LED_PIN, OUTPUT);
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(YELLOW_LED_PIN, LOW);
  digitalWrite(BLUE_LED_PIN, LOW);

  lcRed.shutdown(0, false);
  lcRed.setIntensity(0, displayBrightness);
  lcRed.clearDisplay(0);
  lcYellow.shutdown(0, false);
  lcYellow.setIntensity(0, displayBrightness);
  lcYellow.clearDisplay(0);
  lcBlue.shutdown(0, false);
  lcBlue.setIntensity(0, displayBrightness);
  lcBlue.clearDisplay(0);
  Serial.println("MAX7219 displays initialized");

  Serial.println("Attempting to mount LittleFS...");
  if (!LittleFS.begin(true)) {
    Serial.println("An Error has occurred while mounting LittleFS. Continuing without filesystem...");
  } else {
    Serial.println("LittleFS mounted successfully");
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

  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("Failed to configure static IP");
  } else {
    Serial.println("Static IP configured: 192.168.1.69");
  }

  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi...");
  unsigned long wifiTimeout = millis() + 15000;
  while (WiFi.status() != WL_CONNECTED && millis() < wifiTimeout) {
    delay(1000);
    Serial.println("Attempting to connect...");
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
        if (spectatorCount >= 10) {
            addDebugLog("Auto-login failed: Spectator limit reached");
            request->send(400, "text/plain", "Spectator limit reached");
            return;
        }
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
        sessions[username] = {username, millis(), "spectator", sessionId};
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
    //Serial.println("Serving static file: " + request->url());
    return true;
  });

  // Handle 404 for missing files
  server.onNotFound([](AsyncWebServerRequest *request){
    Serial.println("File not found: " + request->url());
    request->send(404, "text/plain", "File Not Found");
  });

  // Existing server routes (unchanged)
  server.on("/command", HTTP_GET, [](AsyncWebServerRequest *request){
    String cmd;
    if (request->hasParam("cmd")) {
      cmd = request->getParam("cmd")->value();
      String username = request->hasParam("username") ? request->getParam("username")->value() : "";
      if (sessions.find(username) == sessions.end() || sessions[username].role != "marshall") {
        if (cmd == "r" || cmd == "y" || cmd == "b") {
          addDebugLog("Unauthorized lap trigger attempt: " + username + ", cmd: " + cmd);
          request->send(403, "text/plain", "Unauthorized: Lap triggers restricted to Track Marshall");
          return;
        }
      }
      Serial.println("Received web command: " + cmd);
      if (cmd == "start") {
        startRace();
        addDebugLog("Race started by " + username);
        request->send(200, "text/plain", "Race started");
      } else if (cmd == "reset") {
        resetRace();
        addDebugLog("Race reset by " + username);
        request->send(200, "text/plain", "Race reset");
      } else if (cmd == "r" && raceStarted) {
        handleLap(&redLane, "Red", RED_LED_PIN, &lcRed);
        addDebugLog("Red lap triggered by " + username);
        sendRaceData();
        request->send(200, "text/plain", "Red lap triggered");
      } else if (cmd == "y" && raceStarted) {
        handleLap(&yellowLane, "Yellow", YELLOW_LED_PIN, &lcYellow);
        addDebugLog("Yellow lap triggered by " + username);
        sendRaceData();
        request->send(200, "text/plain", "Yellow lap triggered");
      } else if (cmd == "b" && raceStarted) {
        handleLap(&blueLane, "Blue", BLUE_LED_PIN, &lcBlue);
        addDebugLog("Blue lap triggered by " + username);
        sendRaceData();
        request->send(200, "text/plain", "Blue lap triggered");
      } else {
        addDebugLog("Invalid command: " + cmd + " by " + username);
        request->send(400, "text/plain", "Invalid command");
      }
    } else {
      addDebugLog("Missing command from " + (request->hasParam("username") ? request->getParam("username")->value() : "unknown"));
      request->send(400, "text/plain", "Missing command");
    }
  });

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
        sessions[username] = {username, millis(), role, sessionId};
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
    sessions[username] = {username, millis(), role, sessionId};
    saveUsers();
    addDebugLog("Login successful: " + username + ", role: " + role + ", lane: " + lane + ", sessionId: " + sessionId);

    StaticJsonDocument<200> doc;
    doc["role"] = role;
    doc["sessionId"] = sessionId;
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
});

server.on("/logout", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("username", true) || !request->hasParam("sessionId", true)) {
        addDebugLog("Logout failed: Missing username or sessionId");
        request->send(400, "text/plain", "Missing username or sessionId");
        return;
    }

    String username = request->getParam("username", true)->value();
    String sessionId = request->getParam("sessionId", true)->value();

    if (sessions.find(username) != sessions.end() && sessions[username].sessionId == sessionId) {
        for (int i = 0; i < spectatorCount; i++) {
            if (spectators[i] == username) {
                spectators[i] = spectators[spectatorCount - 1];
                spectators[spectatorCount - 1] = "";
                spectatorCount--;
                sessions.erase(username);
                addDebugLog("Logout successful: Spectator " + username + ", sessionId: " + sessionId + ", via manual logout");
                request->send(200, "text/plain", "Logout successful");
                return;
            }
        }

        if (yellowLane.username == username) {
            yellowLane = Lane();
            sessions.erase(username);
            addDebugLog("Logout successful: Player " + username + " from Yellow lane, sessionId: " + sessionId + ", via manual logout");
            request->send(200, "text/plain", "Logout successful");
        } else if (redLane.username == username) {
            redLane = Lane();
            sessions.erase(username);
            addDebugLog("Logout successful: Player " + username + " from Red lane, sessionId: " + sessionId + ", via manual logout");
            request->send(200, "text/plain", "Logout successful");
        } else if (blueLane.username == username) {
            blueLane = Lane();
            sessions.erase(username);
            addDebugLog("Logout successful: Player " + username + " from Blue lane, sessionId: " + sessionId + ", via manual logout");
            request->send(200, "text/plain", "Logout successful");
        } else {
            addDebugLog("Logout failed: User not found: " + username + ", sessionId: " + sessionId);
            request->send(400, "text/plain", "User not found");
        }
    } else {
        addDebugLog("Logout failed: Invalid sessionId for user: " + username + ", sessionId: " + sessionId);
        request->send(400, "text/plain", "Invalid sessionId");
    }
});

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

server.on("/settings", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("username", true) || !request->hasParam("raceDuration", true) || !request->hasParam("countdownTime", true)) {
        addDebugLog("Settings update failed: Missing parameters");
        request->send(400, "text/plain", "Missing parameters");
        return;
    }

    String username = request->getParam("username", true)->value();
    String raceDuration = request->getParam("raceDuration", true)->value();
    String countdownTime = request->getParam("countdownTime", true)->value();

    if (sessions.find(username) == sessions.end() || sessions[username].role != "marshall") {
        addDebugLog("Settings update failed: User not authorized: " + username);
        request->send(403, "text/plain", "Not authorized");
        return;
    }

    // Update race settings (adjust based on your sketch)
    if (raceDuration.toInt() > 0) {
        // Example: raceDurationSeconds = raceDuration.toInt();
        addDebugLog("Updated raceDuration to " + raceDuration + " seconds");
    }
    if (countdownTime.toInt() > 0) {
        // Example: countdownTimeSeconds = countdownTime.toInt();
        addDebugLog("Updated countdownTime to " + countdownTime + " seconds");
    }

    addDebugLog("Settings updated by " + username + ": raceDuration=" + raceDuration + ", countdownTime=" + countdownTime);
    request->send(200, "text/plain", "Settings updated");
});

  server.on("/settings", HTTP_POST, [](AsyncWebServerRequest *request){
    String username = request->hasParam("username") ? request->getParam("username")->value() : "";
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
      lcRed.setIntensity(0, displayBrightness);
      lcYellow.setIntensity(0, displayBrightness);
      lcBlue.setIntensity(0, displayBrightness);
      portEXIT_CRITICAL(&timerMux);
    }
    addDebugLog("Settings updated by " + username + ": maxLapCount=" + String(maxLapCount) + ", minLapTime=" + String(minLapTime) + ", displayBrightness=" + String(displayBrightness));
    request->send(200, "text/plain", "Settings updated");
  });

  server.on("/debug", HTTP_GET, [](AsyncWebServerRequest *request){
    String username = request->hasParam("username") ? request->getParam("username")->value() : "";
    if (username.isEmpty() || sessions.find(username) == sessions.end() || sessions[username].role != "marshall") {
      addDebugLog("Debug access denied for " + (username.isEmpty() ? "no username" : username));
      request->send(403, "text/plain", "Unauthorized: Track Marshall access required");
      return;
    }
    DynamicJsonDocument doc(2048);
    JsonArray logs = doc.createNestedArray("logs");
    int start = debugLogCount < 50 ? 0 : debugLogIndex;
    for (int i = 0; i < debugLogCount; i++) {
      logs.add(debugLogs[(start + i) % 50]);
    }
    JsonObject sensors = doc.createNestedObject("sensors");
    sensors["red"] = digitalRead(RED_SENSOR_PIN) == LOW ? "LOW" : "HIGH";
    sensors["yellow"] = digitalRead(YELLOW_SENSOR_PIN) == LOW ? "LOW" : "HIGH";
    sensors["blue"] = digitalRead(BLUE_SENSOR_PIN) == LOW ? "LOW" : "HIGH";
    String response;
    serializeJson(doc, response);
    addDebugLog("Debug info sent to " + username + ": " + String(debugLogCount) + " logs");
    request->send(200, "application/json", response);
  });

  server.on("/debug", HTTP_POST, [](AsyncWebServerRequest *request){
    String username = request->hasParam("username") ? request->getParam("username")->value() : "";
    if (sessions.find(username) == sessions.end() || sessions[username].role != "marshall") {
      addDebugLog("Debug action denied for " + username);
      request->send(403, "text/plain", "Unauthorized");
      return;
    }
    if (!request->hasParam("action", true)) {
      addDebugLog("Debug action failed: Missing action");
      request->send(400, "text/plain", "Missing action");
      return;
    }
    String action = request->getParam("action", true)->value();
    if (action == "adjustLap") {
      if (!request->hasParam("lane", true) || !request->hasParam("lapTime", true)) {
        addDebugLog("Lap adjustment failed: Missing lane or lapTime");
        request->send(400, "text/plain", "Missing lane or lapTime");
        return;
      }
      String lane = request->getParam("lane", true)->value();
      unsigned long lapTime = request->getParam("lapTime", true)->value().toInt();
      Lane *selectedLane = nullptr;
      String laneName;
      int ledPin;
      LedControl *lc;
      if (lane == "red") {
        selectedLane = &redLane;
        laneName = "Red";
        ledPin = RED_LED_PIN;
        lc = &lcRed;
      } else if (lane == "yellow") {
        selectedLane = &yellowLane;
        laneName = "Yellow";
        ledPin = YELLOW_LED_PIN;
        lc = &lcYellow;
      } else if (lane == "blue") {
        selectedLane = &blueLane;
        laneName = "Blue";
        ledPin = BLUE_LED_PIN;
        lc = &lcBlue;
      } else {
        addDebugLog("Lap adjustment failed: Invalid lane: " + lane);
        request->send(400, "text/plain", "Invalid lane");
        return;
      }
      if (raceStarted && selectedLane->lapCount <= (maxLapCount > 0 ? maxLapCount + 1 : 9999)) {
        selectedLane->lastLapTime = lapTime;
        selectedLane->prevLapTimestamp = millis();
        if (selectedLane->lapCount <= 50) {
          selectedLane->history[selectedLane->lapCount - 1] = formatTime(lapTime);
        }
        selectedLane->lapCount++;
        bool isBestLap = lapTime < selectedLane->bestLapTime && lapTime > 0;
        if (isBestLap) {
          selectedLane->bestLapTime = lapTime;
          selectedLane->pulseState = 2;
          selectedLane->flashCount = 0;
        } else {
          selectedLane->pulseState = 1;
        }
        selectedLane->pulseStartTime = millis();
        selectedLane->displayLapTime = true;
        selectedLane->displayTimeStart = millis();
        if (!selectedLane->username.isEmpty()) {
          users[selectedLane->username].lapHistory.push_back(lapTime);
          if (users[selectedLane->username].lapHistory.size() > 50) {
            users[selectedLane->username].lapHistory.erase(users[selectedLane->username].lapHistory.begin());
          }
          if (isBestLap) {
            users[selectedLane->username].bestLapTime = selectedLane->bestLapTime;
          }
          saveUsers();
        }
        updateDisplay(selectedLane, lc, laneName);
        addDebugLog("Manual lap adjusted: " + laneName + ", time: " + formatTime(lapTime));
        sendRaceData();
        request->send(200, "text/plain", "Lap time adjusted");
      } else {
        addDebugLog("Lap adjustment failed: Race not started or max laps reached for " + laneName);
        request->send(400, "text/plain", "Race not started or max laps reached");
      }
    } else {
      addDebugLog("Debug action failed: Invalid action: " + action);
      request->send(400, "text/plain", "Invalid action");
    }
  });

  server.on("/check-session", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!request->hasParam("username")) {
        addDebugLog("Session check failed: Missing username");
        request->send(400, "text/plain", "Missing username");
        return;
    }
    
    String username = request->getParam("username")->value();
    if (sessions.find(username) != sessions.end()) {
        // Session exists, update lastActive timestamp
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

server.on("/validate-session", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!request->hasParam("sessionId")) {
        addDebugLog("Session validation failed: Missing sessionId");
        request->send(400, "text/plain", "Missing sessionId");
        return;
    }

    String sessionId = request->getParam("sessionId")->value();
    for (auto& session : sessions) {
        if (session.second.sessionId == sessionId) {
            String username = session.second.username;
            sessions[username].lastActive = millis();
            StaticJsonDocument<200> doc;
            doc["username"] = username;
            doc["role"] = sessions[username].role;
            doc["carNumber"] = username == MARSHALL_USERNAME ? "0" : users[username].carNumber;
            doc["carColor"] = username == MARSHALL_USERNAME ? "None" : users[username].carColor;
            doc["lane"] = username == MARSHALL_USERNAME ? "Spectator" : users[username].lane;
            String response;
            serializeJson(doc, response);
            addDebugLog("Session validation successful for " + username + ", sessionId: " + sessionId + ", role: " + sessions[username].role + ", lane: " + users[username].lane);
            request->send(200, "application/json", response);
            return;
        }
    }
    addDebugLog("Session validation failed: No session for sessionId: " + sessionId);
    request->send(404, "text/plain", "No active session");
});

  Serial.println("Setting up WebSocket...");
  ws.onEvent([](AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len){
    if (type == WS_EVT_CONNECT) {
      Serial.println("WebSocket client connected, ID: " + String(client->id()));
      sendRaceData();
    } else if (type == WS_EVT_DISCONNECT) {
      Serial.println("WebSocket client disconnected, ID: " + String(client->id()));
    }
  });

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
    } else if (adjustment == "decrement" && selectedLane->lapCount > 0) {
        selectedLane->lapCount--;
        addDebugLog("Lap decremented for " + lane + " lane by " + username);
    } else {
        addDebugLog("Lap adjustment failed: Invalid adjustment or lap count for " + lane);
        request->send(400, "text/plain", "Invalid adjustment");
        return;
    }

    sendRaceData();
    request->send(200, "text/plain", "Lap adjusted");
});

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

    Lane *selectedLane = nullptr;
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
    addDebugLog("Driver kicked from " + laneName + " lane by " + username + ": " + driverUsername);
    sendRaceData();
    request->send(200, "text/plain", "Driver kicked");
});

  server.addHandler(&ws);

  if (WiFi.status() == WL_CONNECTED) {
    server.begin();
    Serial.println("Webserver started");
  } else {
    Serial.println("Webserver not started due to Wi-Fi failure");
  }

  loadUsers();
  esp_task_wdt_init(15, true);
  esp_task_wdt_add(NULL);
  Serial.println("Setup complete");
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

String formatTime(unsigned long ms) {
  if (ms == ULONG_MAX || ms == 0) return "--:--.---";
  unsigned long minutes = ms / 60000;
  ms %= 60000;
  unsigned long seconds = ms / 1000;
  unsigned long millisecs = ms % 1000;
  char buffer[12];
  snprintf(buffer, 12, "%02lu:%02lu.%03lu", minutes, seconds, millisecs);
  return String(buffer);
}

void addDebugLog(String message) {
  debugLogs[debugLogIndex] = message;
  debugLogIndex = (debugLogIndex + 1) % 50;
  debugLogCount = min(debugLogCount + 1, 50);
  Serial.println("DEBUG: " + message);
}

void sendRaceData() {
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
  Serial.println("Sending WebSocket data: " + json);
  ws.textAll(json);
}

void startRace() {
  if (!raceStarted) {
    raceStarted = true;
    unsigned long currentTime = millis();
    redLane.startTime = currentTime;
    redLane.prevLapTimestamp = currentTime;
    redLane.startDisplayTime = currentTime;
    redLane.startSequencePhase = 1;
    yellowLane.startTime = currentTime;
    yellowLane.prevLapTimestamp = currentTime;
    yellowLane.startDisplayTime = currentTime;
    yellowLane.startSequencePhase = 1;
    blueLane.startTime = currentTime;
    blueLane.prevLapTimestamp = currentTime;
    blueLane.startDisplayTime = currentTime;
    blueLane.startSequencePhase = 1;
    digitalWrite(RED_LED_PIN, HIGH);
    delay(200);
    digitalWrite(RED_LED_PIN, LOW);
    digitalWrite(YELLOW_LED_PIN, HIGH);
    delay(200);
    digitalWrite(YELLOW_LED_PIN, LOW);
    digitalWrite(BLUE_LED_PIN, HIGH);
    delay(200);
    digitalWrite(BLUE_LED_PIN, LOW);
    updateDisplay(&redLane, &lcRed, "Red");
    updateDisplay(&yellowLane, &lcYellow, "Yellow");
    updateDisplay(&blueLane, &lcBlue, "Blue");
    addDebugLog("RACE STARTED: Use 'r', 'y', 'b' to trigger laps, or buttons/sensors");
  } else {
    addDebugLog("Race already started");
  }
  sendRaceData();
}

void resetRace() {
  raceStarted = false;
  redLane.lapCount = 1;
  redLane.lastLapTime = 0;
  redLane.bestLapTime = ULONG_MAX;
  redLane.prevLapTimestamp = 0;
  redLane.pulseState = 0;
  redLane.flashCount = 0;
  redLane.displayLapTime = false;
  redLane.startSequencePhase = 0;
  for (int i = 0; i < 50; i++) redLane.history[i] = "";
  yellowLane.lapCount = 1;
  yellowLane.lastLapTime = 0;
  yellowLane.bestLapTime = ULONG_MAX;
  yellowLane.prevLapTimestamp = 0;
  yellowLane.pulseState = 0;
  yellowLane.flashCount = 0;
  yellowLane.displayLapTime = false;
  yellowLane.startSequencePhase = 0;
  for (int i = 0; i < 50; i++) yellowLane.history[i] = "";
  blueLane.lapCount = 1;
  blueLane.lastLapTime = 0;
  blueLane.bestLapTime = ULONG_MAX;
  blueLane.prevLapTimestamp = 0;
  blueLane.pulseState = 0;
  blueLane.flashCount = 0;
  blueLane.displayLapTime = false;
  blueLane.startSequencePhase = 0;
  for (int i = 0; i < 50; i++) blueLane.history[i] = "";
  if (!redLane.username.isEmpty()) {
    users[redLane.username].lapHistory.clear();
    users[redLane.username].bestLapTime = ULONG_MAX;
  }
  if (!yellowLane.username.isEmpty()) {
    users[yellowLane.username].lapHistory.clear();
    users[yellowLane.username].bestLapTime = ULONG_MAX;
  }
  if (!blueLane.username.isEmpty()) {
    users[blueLane.username].lapHistory.clear();
    users[blueLane.username].bestLapTime = ULONG_MAX;
  }
  saveUsers();
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(YELLOW_LED_PIN, LOW);
  digitalWrite(BLUE_LED_PIN, LOW);
  portENTER_CRITICAL(&timerMux);
  lcRed.clearDisplay(0);
  lcYellow.clearDisplay(0);
  lcBlue.clearDisplay(0);
  portEXIT_CRITICAL(&timerMux);
  updateDisplay(&redLane, &lcRed, "Red");
  updateDisplay(&yellowLane, &lcYellow, "Yellow");
  updateDisplay(&blueLane, &lcBlue, "Blue");
  addDebugLog("RACE RESET: All lanes reset, displays blank");
  sendRaceData();
}

void handleLap(Lane* lane, String laneName, int ledPin, LedControl* lc) {
  unsigned long currentTime = millis();
  unsigned long lapTime;
  if (lane->lapCount == 1) {
    lapTime = currentTime - lane->startTime;
  } else {
    lapTime = currentTime - lane->prevLapTimestamp;
  }
  if (lapTime < minLapTime) {
    addDebugLog("Lap ignored for " + laneName + ": Time " + String(lapTime) + "ms below minimum " + String(minLapTime) + "ms");
    return;
  }
  if (maxLapCount > 0 && lane->lapCount > maxLapCount) {
    addDebugLog("Lap ignored for " + laneName + ": Max lap count " + String(maxLapCount) + " reached");
    return;
  }
  lane->lastLapTime = lapTime;
  lane->prevLapTimestamp = currentTime;
  if (lane->lapCount <= 50) {
    lane->history[lane->lapCount - 1] = formatTime(lane->lastLapTime);
  }
  lane->lapCount++;
  bool isBestLap = lane->lastLapTime < lane->bestLapTime && lane->lastLapTime > 0;
  if (isBestLap) {
    lane->bestLapTime = lapTime;
    lane->pulseState = 2;
    lane->flashCount = 0;
  } else {
    lane->pulseState = 1;
  }
  lane->pulseStartTime = currentTime;
  lane->displayLapTime = true;
  lane->displayTimeStart = currentTime;
  if (!lane->username.isEmpty()) {
    users[lane->username].lapHistory.push_back(lane->lastLapTime);
    if (users[lane->username].lapHistory.size() > 50) {
      users[lane->username].lapHistory.erase(users[lane->username].lapHistory.begin());
    }
    if (isBestLap) {
      users[lane->username].bestLapTime = lane->bestLapTime;
    }
    saveUsers();
  }
  if (lc != nullptr) {
    updateDisplay(lane, lc, laneName);
  }
  addDebugLog("LAP TRIGGERED: " + laneName + ": Completed Lap " + String(lane->lapCount - 1) + ", Time: " + formatTime(lane->lastLapTime) + (isBestLap ? " (NEW BEST)" : ""));
  sendRaceData();
}

void updateDisplay(Lane* lane, LedControl* lc, String laneName) {
  if (lc == nullptr) return;
  Serial.print("Updating display for ");
  Serial.print(laneName);
  Serial.print(", phase: ");
  Serial.println(lane->startSequencePhase);

  portENTER_CRITICAL(&timerMux);
  lc->shutdown(0, false);
  lc->setIntensity(0, 2);
  lc->setScanLimit(0, 7);
  lc->clearDisplay(0);
  portEXIT_CRITICAL(&timerMux);

  if (!raceStarted) {
    Serial.println("MAX7219 " + laneName + " Updating: Blank (no race)");
  } else if (lane->startSequencePhase > 0) {
    portENTER_CRITICAL(&timerMux);
    if (lane->startSequencePhase == 1) {
      Serial.println("MAX7219 " + laneName + " Updating: 33333333");
      for (int i = 0; i < 8; i++) {
        lc->setDigit(0, i, 3, false);
        delay(10);
      }
    } else if (lane->startSequencePhase == 2) {
      Serial.println("MAX7219 " + laneName + " Updating: 22222222");
      for (int i = 0; i < 8; i++) {
        lc->setDigit(0, i, 2, false);
        delay(10);
      }
    } else if (lane->startSequencePhase == 3) {
      Serial.println("MAX7219 " + laneName + " Updating: 11111111");
      for (int i = 0; i < 8; i++) {
        lc->setDigit(0, i, 1, false);
        delay(10);
      }
    } else if (lane->startSequencePhase == 4) {
      unsigned long elapsed = millis() - (lane->startDisplayTime + 3750);
      int cycle = elapsed / 400;
      int phase = elapsed % 400;
      if (phase < 200 && cycle < 3) {
        Serial.println("MAX7219 " + laneName + " Updating: --------");
        for (int i = 0; i < 8; i++) {
          lc->setChar(0, i, '-', false);
          delay(laneName == "Blue" ? 20 : 10);
        }
      } else {
        Serial.println("MAX7219 " + laneName + " Updating: Blank (flash)");
        lc->clearDisplay(0);
        delay(laneName == "Blue" ? 20 : 10);
      }
    }
    portEXIT_CRITICAL(&timerMux);
  } else if (lane->displayLapTime) {
    unsigned long ms = lane->lastLapTime;
    unsigned long minutes = ms / 60000;
    ms %= 60000;
    unsigned long seconds = ms / 1000;
    unsigned long millisecs = ms % 1000;
    char buffer[9];
    snprintf(buffer, 9, "%02lu%02lu%03lu", minutes, seconds, millisecs);
    Serial.print("MAX7219 " + laneName + " Updating Lap Time: ");
    Serial.print(buffer);
    Serial.print(" (Digits: ");
    portENTER_CRITICAL(&timerMux);
    for (int i = 0; i < 8; i++) {
      if (i == 4) {
        lc->setChar(0, 7 - i, '.', false);
        Serial.print(".");
      } else {
        int digit = buffer[i < 4 ? i : i - 1] - '0';
        lc->setDigit(0, 7 - i, digit, false);
        Serial.print(digit);
      }
      delay(10);
      if (i < 7) Serial.print(",");
    }
    portEXIT_CRITICAL(&timerMux);
    Serial.println(")");
  } else {
    Serial.print("MAX7219 " + laneName + " Updating Lap Count: LAP ");
    Serial.print(lane->lapCount);
    Serial.print(" (Digits: L,A,P, , , , ,");
    Serial.print(lane->lapCount);
    Serial.println(")");
    portENTER_CRITICAL(&timerMux);
    lc->setChar(0, 7, 'L', false);
    lc->setChar(0, 6, 'A', false);
    lc->setChar(0, 5, 'P', false);
    for (int i = 1; i <= 4; i++) {
      lc->setChar(0, 4 - i, ' ', false);
    }
    lc->setDigit(0, 0, lane->lapCount, false);
    delay(10);
    portEXIT_CRITICAL(&timerMux);
  }
}

void updateDisplayBlink(Lane* lane, LedControl* lc, String laneName) {
  if (lc == nullptr || lane->pulseState != 2) return;
  unsigned long elapsed = millis() - lane->pulseStartTime;
  if (elapsed < 1200) {
    int cycle = elapsed / 400;
    int phase = elapsed % 400;
    portENTER_CRITICAL(&timerMux);
    if (phase < 200) {
      lc->shutdown(0, false);
      lc->setIntensity(0, 2);
      lc->setScanLimit(0, 7);
      unsigned long ms = lane->lastLapTime;
      unsigned long minutes = ms / 60000;
      ms %= 60000;
      unsigned long seconds = ms / 1000;
      unsigned long millisecs = ms % 1000;
      char buffer[9];
      snprintf(buffer, 9, "%02lu%02lu%03lu", minutes, seconds, millisecs);
      for (int i = 0; i < 8; i++) {
        if (i == 4) {
          lc->setChar(0, 7 - i, '.', false);
        } else {
          int digit = buffer[i < 4 ? i : i - 1] - '0';
          lc->setDigit(0, 7 - i, digit, false);
        }
        delay(10);
      }
    } else {
      lc->clearDisplay(0);
    }
    portEXIT_CRITICAL(&timerMux);
    if (phase >= 399 && cycle < 2) {
      lane->flashCount++;
    }
  } else if (millis() - lane->displayTimeStart < 5000) {
    portENTER_CRITICAL(&timerMux);
    lc->shutdown(0, false);
    lc->setIntensity(0, 2);
    lc->setScanLimit(0, 7);
    unsigned long ms = lane->lastLapTime;
    unsigned long minutes = ms / 60000;
    ms %= 60000;
    unsigned long seconds = ms / 1000;
    unsigned long millisecs = ms % 1000;
    char buffer[9];
    snprintf(buffer, 9, "%02lu%02lu%03lu", minutes, seconds, millisecs);
    for (int i = 0; i < 8; i++) {
      if (i == 4) {
        lc->setChar(0, 7 - i, '.', false);
      } else {
        int digit = buffer[i < 4 ? i : i - 1] - '0';
        lc->setDigit(0, 7 - i, digit, false);
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
  unsigned long currentTime = millis();
  if (lane->pulseState == 1) {
    if (currentTime - lane->pulseStartTime < 1000) {
      digitalWrite(ledPin, HIGH);
    } else {
      digitalWrite(ledPin, LOW);
      lane->pulseState = 0;
    }
  } else if (lane->pulseState == 2) {
    unsigned long elapsed = currentTime - lane->pulseStartTime;
    if (elapsed < 1200) {
      int cycle = elapsed / 400;
      int phase = elapsed % 400;
      digitalWrite(ledPin, phase < 200 ? HIGH : LOW);
      if (phase >= 399 && cycle < 2) {
        lane->flashCount++;
      }
    } else {
      digitalWrite(ledPin, LOW);
      lane->pulseState = 0;
      lane->flashCount = 0;
    }
  }
}

void loop() {
  esp_task_wdt_reset();
  static unsigned long lastRedTrigger = 0, lastYellowTrigger = 0, lastBlueTrigger = 0, lastStartResetButton = 0;

  if ((digitalRead(RED_SENSOR_PIN) == LOW || digitalRead(RED_BUTTON_PIN) == LOW) && millis() - lastRedTrigger > 400) {
    if (raceStarted) {
      handleLap(&redLane, "Red", RED_LED_PIN, &lcRed);
    }
    lastRedTrigger = millis();
  }

  if ((digitalRead(YELLOW_SENSOR_PIN) == LOW || digitalRead(YELLOW_BUTTON_PIN) == LOW) && millis() - lastYellowTrigger > 400) {
    if (raceStarted) {
      handleLap(&yellowLane, "Yellow", YELLOW_LED_PIN, &lcYellow);
    }
    lastYellowTrigger = millis();
  }

  if ((digitalRead(BLUE_SENSOR_PIN) == LOW || digitalRead(BLUE_BUTTON_PIN) == LOW) && millis() - lastBlueTrigger > 400) {
    if (raceStarted) {
      handleLap(&blueLane, "Blue", BLUE_LED_PIN, &lcBlue);
    }
    lastBlueTrigger = millis();
  }

  if (digitalRead(START_RESET_BUTTON_PIN) == LOW && millis() - lastStartResetButton > 400) {
    if (raceStarted) {
      resetRace();
    } else {
      startRace();
    }
    lastStartResetButton = millis();
  }

  if (redLane.startSequencePhase > 0 && millis() - redLane.startDisplayTime >= redLane.startSequencePhase * 1250) {
    redLane.startSequencePhase++;
    if (redLane.startSequencePhase > 4) redLane.startSequencePhase = 0;
    updateDisplay(&redLane, &lcRed, "Red");
  }
  if (yellowLane.startSequencePhase > 0 && millis() - yellowLane.startDisplayTime >= yellowLane.startSequencePhase * 1250) {
    yellowLane.startSequencePhase++;
    if (yellowLane.startSequencePhase > 4) yellowLane.startSequencePhase = 0;
    updateDisplay(&yellowLane, &lcYellow, "Yellow");
  }
  if (blueLane.startSequencePhase > 0 && millis() - blueLane.startDisplayTime >= blueLane.startSequencePhase * 1250) {
    blueLane.startSequencePhase++;
    if (blueLane.startSequencePhase > 4) blueLane.startSequencePhase = 0;
    updateDisplay(&blueLane, &lcBlue, "Blue");
  }

  if (redLane.displayLapTime && millis() - redLane.displayTimeStart >= 5000) {
    redLane.displayLapTime = false;
    updateDisplay(&redLane, &lcRed, "Red");
  }
  if (yellowLane.displayLapTime && millis() - yellowLane.displayTimeStart >= 5000) {
    yellowLane.displayLapTime = false;
    updateDisplay(&yellowLane, &lcYellow, "Yellow");
  }
  if (blueLane.displayLapTime && millis() - blueLane.displayTimeStart >= 5000) {
    blueLane.displayLapTime = false;
    updateDisplay(&blueLane, &lcBlue, "Blue");
  }

  if (redLane.pulseState == 2 && redLane.displayLapTime) {
    updateDisplayBlink(&redLane, &lcRed, "Red");
  }
  if (yellowLane.pulseState == 2 && yellowLane.displayLapTime) {
    updateDisplayBlink(&yellowLane, &lcYellow, "Yellow");
  }
  if (blueLane.pulseState == 2 && blueLane.displayLapTime) {
    updateDisplayBlink(&blueLane, &lcBlue, "Blue");
  }

  updateLED(&redLane, RED_LED_PIN);
  updateLED(&yellowLane, YELLOW_LED_PIN);
  updateLED(&blueLane, BLUE_LED_PIN);

  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (raceStarted && cmd == "r") {
      handleLap(&redLane, "Red", RED_LED_PIN, &lcRed);
    } else if (raceStarted && cmd == "y") {
      handleLap(&yellowLane, "Yellow", YELLOW_LED_PIN, &lcYellow);
    } else if (raceStarted && cmd == "b") {
      handleLap(&blueLane, "Blue", BLUE_LED_PIN, &lcBlue);
    } else if (cmd == "start") {
      startRace();
    } else if (cmd == "reset") {
      resetRace();
    } else {
      Serial.println("Unknown command. Use: r/y/b (lap), start, reset");
    }
  }

unsigned long currentTime = millis();
for (auto it = sessions.begin(); it != sessions.end();) {
    if (currentTime - it->second.lastActive > SESSION_TIMEOUT) {
        String username = it->second.username;
        String sessionId = it->second.sessionId;
        if (redLane.username == username) {
            redLane = Lane();
            addDebugLog("Session expired for player " + username + " from Red lane, sessionId: " + sessionId);
        }
        if (yellowLane.username == username) {
            yellowLane = Lane();
            addDebugLog("Session expired for player " + username + " from Yellow lane, sessionId: " + sessionId);
        }
        if (blueLane.username == username) {
            blueLane = Lane();
            addDebugLog("Session expired for player " + username + " from Blue lane, sessionId: " + sessionId);
        }
        for (int i = 0; i < spectatorCount; i++) {
            if (spectators[i] == username) {
                spectators[i] = spectators[spectatorCount - 1];
                spectators[spectatorCount - 1] = "";
                spectatorCount--;
                addDebugLog("Session expired for spectator " + username + ", sessionId: " + sessionId);
                break;
            }
        }
        users[username].lane = "";
        it = sessions.erase(it);
        addDebugLog("Session cleanup complete for " + username + ", sessionId: " + sessionId);
        saveUsers();
        sendRaceData();
    } else {
        ++it;
    }
}

  ws.cleanupClients();
}