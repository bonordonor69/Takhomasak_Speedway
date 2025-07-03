# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**Takhomasak Speedway** is an ESP32-based slot car racing timing system that provides real-time lap timing, race management, and web-based control interface. The system features three racing lanes with VL53L0X LIDAR sensors for lap detection, MAX7219 displays for visual feedback, and a comprehensive web interface for race management.

## Core Architecture

### Hardware Components
- **ESP32-WROOM-32**: Main microcontroller running the timing system
- **VL53L0X LIDAR Sensors**: Distance sensors for lap detection (one per lane)
- **MAX7219 Displays**: 8-digit 7-segment displays for lap times and race status
- **Physical Controls**: Buttons for manual lap triggers and race control
- **Status LEDs**: Visual indicators for each lane

### Software Architecture
The system is built around several key subsystems:

1. **Race Management Core**: Handles race state, timing, and lap counting
2. **Web Server**: ESP32 AsyncWebServer providing REST API and WebSocket communication
3. **Session Management**: User authentication and role-based access control
4. **Display Controller**: MAX7219 display management with countdown sequences
5. **Sensor Handler**: VL53L0X sensor reading and lap detection logic
6. **Data Persistence**: LittleFS-based storage for user data and race history

### Key Data Structures
- **Lane**: Contains race data for each lane (username, lap times, history, display state)
- **User**: Persistent user data with race history and best times
- **Session**: Active user sessions with roles (player/spectator/marshall)

## Development Workflow

### Building and Uploading
1. **Arduino IDE Setup**: Use Arduino IDE with ESP32 board package
2. **Required Libraries**:
   - `LedControl` (MAX7219 displays)
   - `WiFi` (ESP32 built-in)
   - `ESPAsyncWebServer` and `AsyncTCP`
   - `ArduinoJson` (JSON handling)
   - `Adafruit_VL53L0X` (LIDAR sensors)
   - `LittleFS` (file system)

3. **Upload Process**:
   - Upload sketch to ESP32 via USB
   - Upload data files to LittleFS using "ESP32 Sketch Data Upload" tool
   - Data files located in `/data/` directory (index.html, favicon.ico, etc.)

### Network Configuration
- **Static IP**: 192.168.1.69 (configured in code)
- **WiFi Credentials**: Currently hardcoded ("Lawson" / "yamaha350")
- **Web Interface**: Accessible at http://192.168.1.69/

### Project Structure
```
Takhomasak_Speedway/
├── Takhomasak_Speedway/           # Main current version
│   ├── Takhomasak_Speedway.ino    # Main Arduino sketch
│   ├── data/                      # Web files for LittleFS
│   │   ├── index.html            # Web interface
│   │   ├── favicon.ico           # Browser icon
│   │   └── qrcode_dkm.png        # QR code image
│   └── Takhomasak_Speedway.fzz   # Fritzing circuit diagram
├── Takhomasak_Speedway_RELEASE_1.0/  # Stable release version
├── Takhomasak_Speedway_Minimal_MAX7219_Test/  # Hardware test sketch
└── [Various development versions and backups]
```

## Pin Configuration

### VL53L0X LIDAR Sensors (I2C)
- **SDA**: GPIO 21 (shared)
- **SCL**: GPIO 22 (shared)
- **XSHUT Pins**: GPIO 4 (Red), GPIO 18 (Yellow), GPIO 5 (Blue)
- **I2C Addresses**: 0x30 (Red), 0x31 (Yellow), 0x32 (Blue)

### MAX7219 Displays (SPI)
- **DIN**: GPIO 23 (shared)
- **CLK**: GPIO 19 (shared)
- **CS Pins**: GPIO 13 (Red), GPIO 12 (Yellow), GPIO 32 (Blue)

### Controls and Indicators
- **Lane LEDs**: GPIO 14 (Red), GPIO 15 (Yellow), GPIO 16 (Blue)
- **Manual Buttons**: GPIO 25 (Red), GPIO 27 (Yellow), GPIO 26 (Blue)
- **Start/Reset Button**: GPIO 33

## Key Features and Functions

### Race Management
- **Start Race**: Initiates countdown sequence and begins timing
- **Reset Race**: Clears all lap data and returns to ready state
- **Lap Detection**: Automatic via LIDAR sensors or manual via buttons/web interface
- **Race Duration**: Configurable time limits with automatic race ending

### User System
- **Authentication**: Session-based with unique session IDs
- **Roles**: Players (lane assignment), Spectators (view-only), Track Marshall (full control)
- **Data Persistence**: User statistics and race history stored in LittleFS

### Web Interface Features
- **Real-time Updates**: WebSocket-based live race data
- **Mobile Responsive**: Optimized for tablets and phones
- **Track Marshall Controls**: Advanced race management and debugging tools
- **Spectator Mode**: Auto-login for easy viewing access

## Development Notes

### Performance Considerations
- **Sensor Reading**: Throttled to 50ms intervals to prevent overwhelming the system
- **Data Saving**: User data saves are throttled to every 5 seconds
- **WebSocket Updates**: Race data broadcasts limited to 500ms intervals
- **Memory Management**: Debug logs limited to 25 entries, lap history to 50 entries

### Critical Sections
- **Display Updates**: Protected with `timerMux` mutex due to SPI timing sensitivity
- **Race State Changes**: Protected with `raceMux` mutex for thread safety
- **Watchdog Timer**: 30-second timeout to prevent system lockups

### Common Development Tasks

#### Adding New Hardware
1. Update pin definitions in the main sketch
2. Add initialization code in `setup()`
3. Update the hardware documentation comments
4. Test with hardware-specific test sketches

#### Modifying Web Interface
1. Edit `/data/index.html` for UI changes
2. Update corresponding API endpoints in the main sketch
3. Test WebSocket data format compatibility
4. Re-upload data folder to LittleFS

#### Debugging Issues
- Use Track Marshall login (username: "marshall", password: "track2025")
- Access debug endpoint: `GET /debug?sessionId=<session_id>`
- Monitor Serial output at 115200 baud
- Check system status: heap memory, sensor readings, session information

#### Testing Hardware Components
- Use `Takhomasak_Speedway_Minimal_MAX7219_Test.ino` for display testing
- Individual sensor testing available through debug interface
- Manual lap triggers available via web interface for Track Marshall

### Important Configuration Settings
- **Detection Threshold**: Default 99mm for LIDAR sensors (adjustable via web interface)
- **Minimum Lap Time**: 500ms default to prevent false triggers
- **Session Timeout**: 180 minutes (3 hours)
- **Display Brightness**: 0-15 scale, default 2

### File System Structure
- `/users.json`: Persistent user data and race statistics
- `/index.html`: Main web interface
- `/favicon.ico`: Browser icon
- `/qrcode_dkm.png`: QR code for easy mobile access

When working with this codebase, prioritize safety and reliability as this is used for live racing events. Test all changes thoroughly, especially sensor calibration and race timing logic.

## Development Environment Setup (Completed)

### Arduino CLI Configuration
- **Location**: `/mnt/c/Users/tlaws/Documents/Arduino/Takhomasak_Speedway/bin/arduino-cli`
- **Usage**: `export PATH="/mnt/c/Users/tlaws/Documents/Arduino/Takhomasak_Speedway/bin:$PATH"`
- **Board**: `esp32:esp32:esp32`
- **Compile**: `arduino-cli compile --fqbn esp32:esp32:esp32 sketch.ino`
- **Upload**: `arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32 sketch.ino`

### Installed Libraries
- ✅ **ArduinoJson** 7.4.2 - JSON handling
- ✅ **Adafruit_VL53L0X** 1.2.4 - LIDAR sensors (with dependencies)
- ✅ **ESP Async WebServer** 3.7.8 - Web server
- ✅ **AsyncTCP** 1.1.4 - TCP async support
- ✅ **LEDMatrixDriver** 0.2.2 - For future MAX7219 implementation

### Hardware Status
- ✅ **VL53L0X Sensors**: Working, tested
- ✅ **ESP32 Web Server**: Working, tested  
- ❌ **MAX7219 Displays**: Not connected, code disabled
- ❌ **Control LEDs**: Not connected (Red/Yellow/Blue lane LEDs)
- ❌ **Manual Buttons**: Not connected (Red/Yellow/Blue/Start buttons)

### Git Repository
- **Status**: Local repository initialized (private)
- **Security**: WiFi credentials in code (consider environment variables)
- **Branches**: Currently on `master`
- **Remote**: None (local only, networking info safe)

### Current Code Issues
1. **AsyncWebServer Warning**: Minor compatibility warning with AsyncTCP, doesn't affect functionality
2. **MAX7219 Integration**: Disabled until hardware connected, will need library replacement
3. **WiFi Credentials**: Hardcoded in source (lines in main sketch)

### Next Session TODO
1. **Connect Hardware**: MAX7219 displays, LEDs, buttons
2. **Library Integration**: Proper MAX7219 library with 7-segment support
3. **Security**: Move WiFi credentials to config file or environment variables
4. **Feature Development**: Ready for race timing enhancements

### Development Notes
- Core timing and sensor functionality working
- Web interface operational
- Session management functional  
- Race data persistence working
- Ready for feature additions and hardware completion

## Session Notes (Evening Session - Library Compatibility Resolution)

### ✅ SUCCESSFULLY COMPLETED
1. **Arduino CLI Setup**: Fully configured and working
2. **Library Version Matching**: Critical breakthrough - matched Arduino IDE versions exactly
3. **Code Upload**: Successfully uploaded and running on ESP32
4. **Git Repository**: Initialized with project documentation
5. **Hardware Status Confirmed**: VL53L0X sensors and web server working perfectly

### 🔧 KEY TECHNICAL DISCOVERIES
**Library Compatibility Issue Root Cause:**
- Arduino CLI was installing **newer** library versions than Arduino IDE
- **ESP32 Core Version Mismatch**: CLI had 3.2.0, IDE uses 2.0.17
- **ESPAsyncWebServer**: CLI had 3.7.8, IDE uses 3.1.0 (lacamera fork)
- **AsyncTCP**: CLI had 1.1.4 (different fork), IDE uses 1.1.4 (dvarrel fork)

**Working Configuration (CRITICAL - DO NOT CHANGE):**
- **ESP32 Core**: esp32:esp32@2.0.17 (older core, stable mbedtls API)
- **ESPAsyncWebServer**: 3.1.0 (lacamera fork from Arduino IDE libraries)
- **AsyncTCP**: 1.1.4 (dvarrel fork from Arduino IDE libraries)
- **Watchdog Timer**: `esp_task_wdt_init(10, true)` (2.x syntax, not 3.x)

### 🎯 LESSON LEARNED 
**User's Fear Was 100% Justified!** 
- Newer ESP32 core versions (3.x) have **breaking changes** in mbedtls library
- Newer library versions cause compilation failures
- Arduino CLI installs latest by default, Arduino IDE uses pinned working versions
- **Solution**: Copy exact libraries from Arduino IDE to Arduino CLI

### 📋 CURRENT PROJECT STATUS
**Hardware Working:**
- ✅ VL53L0X LIDAR sensors (Red, Yellow, Blue lanes)
- ✅ ESP32 web server (192.168.1.69)
- ✅ WiFi connectivity ("Lawson"/"yamaha350")
- ✅ Race timing and lap detection
- ✅ Session management and user authentication
- ✅ Data persistence (LittleFS)

**Hardware Not Connected:**
- ❌ MAX7219 displays (code disabled with null checks)
- ❌ Lane LEDs (Red/Yellow/Blue GPIO 14/15/16)
- ❌ Manual buttons (Red/Yellow/Blue/Start)

**Next Hardware Connection Priority:**
1. MAX7219 displays (need library selection - LedControl has AVR dependencies)
2. Lane status LEDs (simple GPIO outputs)
3. Manual lap trigger buttons (GPIO inputs with pullups)

### 🚀 READY FOR NEXT SESSION
**Development Environment:**
- Arduino CLI configured with working library versions
- Git repository initialized (local/private)
- Code compiles and uploads successfully
- All sensor and timing functionality operational

**Security Notes:**
- WiFi credentials hardcoded (consider moving to environment variables)
- Git repo is local only (networking info safe)
- Static IP 192.168.1.69 configured

### 💡 USER PREFERENCES NOTED
- **User likes LOTS of detailed notes** 📝
- User was initially afraid to update libraries (fear was completely justified!)
- User prefers thorough documentation of technical decisions
- User values understanding WHY things work/break

### 🔮 FUTURE TASKS
1. **MAX7219 Integration**: Research ESP32-compatible 7-segment library
2. **Hardware Completion**: Connect remaining LEDs and buttons  
3. **Security Enhancement**: Move WiFi credentials to config file
4. **Feature Development**: Race timing enhancements and new features
5. **Code Organization**: Consider breaking into multiple files for maintainability

**Remember: This user appreciates detailed explanations and comprehensive documentation!**

## Session Notes (Evening Session - Web Server Bug Fix)

### 🚨 CRITICAL BUG RESOLVED
**Problem**: ESP32 connected to WiFi and responded to ping, but web server was inaccessible via browser.

**Root Cause**: During user's refactoring of `sendRaceData()` function, critical web server initialization code was accidentally removed from `setup()` function.

**Missing Code**:
```cpp
server.addHandler(&ws);      // WebSocket handler
server.begin();              // Actually start the web server
```

**Impact**: Web server was never started, making it completely inaccessible despite successful WiFi connection.

### 🔧 DEBUGGING PROCESS
1. **Systematic Approach**: Used TodoWrite to track investigation steps
2. **Compilation Errors**: Fixed multiple issues from refactoring:
   - Missing global variable declarations (`lastRaceDataSendTime`, `RACE_DATA_SEND_INTERVAL_MS`)
   - Duplicate `sendRaceData()` functions
   - Code misplaced outside of function scope
3. **Web Server Investigation**: Discovered missing `server.begin()` call

### ✅ FIXES APPLIED
1. **Variable Scope**: Moved timing variables to global scope (lines 178-179)
2. **Code Cleanup**: Removed duplicate `sendRaceData()` function
3. **Server Initialization**: Added missing web server startup code to `setup()` function:
   ```cpp
   server.addHandler(&ws);
   if (WiFi.status() == WL_CONNECTED) {
     server.begin();
     Serial.println("Web server started");
   }
   ```

### 🎯 LESSON LEARNED
**Refactoring Risk**: When optimizing code, even simple changes can accidentally remove critical initialization code. Always verify that moved/removed code isn't essential for system operation.

### 📊 CURRENT STATUS
**Fully Working**:
- ✅ Code compiles and uploads successfully
- ✅ ESP32 connects to WiFi (192.168.1.69)
- ✅ Web server starts and serves pages
- ✅ VL53L0X sensors operational
- ✅ Race timing and session management working
- ✅ WebSocket communication functional

**Still Pending**:
- ❌ MAX7219 displays (hardware not connected)
- ❌ Lane LEDs and manual buttons (hardware not connected)

### 🚀 READY FOR NEXT SESSION
- All core functionality restored and working
- Web interface accessible and responsive
- Ready for hardware completion or feature development
- Git repository ready for commit

**User Satisfaction**: Problem resolved efficiently with systematic debugging approach!