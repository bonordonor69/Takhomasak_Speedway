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