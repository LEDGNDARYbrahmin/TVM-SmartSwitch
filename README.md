# Traivimiya-SmartSwitch

Overview

Traivimiya Smart Switch is a comprehensive IoT home automation solution built on the ESP32 platform. It provides intelligent control of up to 5 AC/DC appliances with advanced features including environmental monitoring, scheduling, temperature-based automation, and seamless integration with Alexa and Google Home.

## ✨ Features

### 🔌 Core Functionality
- **5-Channel Relay Control**: Independent control of up to 5 appliances (16A capacity per relay)
- **Web-Based Interface**: Intuitive web dashboard for device management
- **RESTful API**: Complete API for third-party integrations
- **WiFi Configuration**: Captive portal for easy WiFi setup
- **State Persistence**: Automatic saving/restoring of switch states across power cycles

### 📊 Environmental Monitoring
- **Temperature & Humidity Sensing**: Real-time DHT22 sensor readings
- **OLED Display**: 3 rotating screens showing:
  - Environment data (Temperature, Humidity)
  - Device status (Switch states)
  - Network information (IP, WiFi status)

### ⏰ Automation & Scheduling
- **Time Schedules**: Up to 10 programmable schedules per device
- **Alarms**: 3 configurable alarms with buzzer notifications
- **Temperature-Based Control**: Automatic relay control based on temperature thresholds
- **Countdown Timers**: Per-switch countdown timers for auto-off functionality

### 🏡 Smart Home Integration
- **Alexa Integration**: Full Amazon Alexa support via fauxmoESP
- **Google Home Compatible**: Works with Google Assistant
- **UDP Discovery**: Automatic device discovery on local network
- **Voice Control**: Control all switches via voice commands

### 🔒 Security Features
- **HTTP Authentication**: User/password protection for all endpoints
- **Configurable Credentials**: Change admin password via API
- **WiFi Credential Management**: Secure storage in NVS
- **Client Activity Tracking**: Monitor connected clients

### 🔧 Advanced Features
- **OTA Updates**: Wireless firmware updates via web interface
- **NTP Time Sync**: Automatic time synchronization (every 24 hours)
- **Watchdog Timer**: 30-second watchdog for system stability
- **WiFi Auto-Reconnect**: Automatic reconnection on network loss
- **Multi-Screen OLED**: Rotating display with real-time information

### Required Components

| Component | Specification | Quantity | Purpose |
|-----------|--------------|----------|---------|
| **ESP32 DevKit** | 240MHz dual-core, WiFi/BT | 1 | Main controller |
| **5V Relay Module** | 5-channel, optocoupled | 1 | AC/DC switching |
| **DHT22 Sensor** | Temperature/Humidity | 1 | Environmental sensing |
| **OLED Display** | SSD1306, 128x64, I2C | 1 | Status display |
| **Passive Buzzer** | 5V | 1 | Audio alerts |
| **LED** | 3mm/5mm | 1 | Status indicator |
| **Push Button** | Tactile switch | 1 | Configuration mode |
| **Power Supply** | 5V 2A minimum | 1 | System power |
| **Resistors** | 10kΩ pull-up | 2-3 | Button/sensor |


### Key Libraries

|       Library       |  Version |         Purpose         |
|---------------------|----------|-------------------------|
| `ESPAsyncWebServer` | Latest   | Non-blocking web server |
| `AsyncTCP` | Latest | Asynchronous TCP library |
| `ArduinoJson` | v6.x | JSON parsing/serialization |
| `fauxmoESP` | Latest | Alexa/Google Home emulation |
| `DHT` | Latest | DHT22 sensor interface |
| `Adafruit_SSD1306` | Latest | OLED display driver |
| `Preferences` | Built-in | NVS storage wrapper |
| `ArduinoOTA` | Built-in |      Over-the-air updates |

### Library Installation

#### Arduino Library Manager
Tools → Manage Libraries → Search and install:
ESPAsyncWebServer
AsyncTCP
ArduinoJson
fauxmoESP
DHT sensor library
Adafruit SSD1306
Adafruit GFX Library


---

## 📱 Usage Guide

### Initial Setup

1. **Power On Device**
   - LED will flash RED during boot
   - OLED displays "TRAIVIMIYA SWITCH"

2. **WiFi Configuration**
   
   **Option A: Using Captive Portal**
   - If no credentials saved, device creates WiFi AP
   - Connect to `traivimiya_switch` (password: `traivimiya12`)
   - Captive portal opens automatically
   - Enter your home WiFi credentials
   
   **Option B: Factory Reset**
   - Hold BOOT button during power-on
   - Credentials cleared, AP mode activated

3. **Access Web Interface**
   - Device connects to WiFi (LED turns YELLOW, then GREEN)
   - Find IP address from:
     - Serial monitor
     - OLED display (Network screen)
     - Router DHCP list

### Voice Control Setup

**Alexa:**
1. Say "Alexa, discover devices"
2. Wait 45 seconds
3. Switches appear as "Switch 1", "Switch 2", etc.
4. Rename via API for custom names

**Google Home:**
1. Open Google Home app
2. Add device → Works with Google
3. Search for "TVM Smart Switch"
4. Link account and discover devices

### Scheduling Examples

**Turn on bedroom light at 7:00 AM:**


---

## 🔧 Troubleshooting

### Common Issues

**1. Device won't connect to WiFi**
- Hold BOOT button during power-on to reset credentials
- Check SSID/password spelling
- Ensure 2.4GHz WiFi (ESP32 doesn't support 5GHz)
- Move device closer to router

**2. Web interface not accessible**
- Check serial monitor for IP address
- Verify device on same network
- Check firewall settings
- Try `http://traivimiya-switch.local` (mDNS)

**3. Relays not switching**
- Check relay module power (5V required)
- Verify GPIO connections
- Test with multimeter
- Check relay coil voltage

**4. OLED display blank**
- Verify I2C address (0x3C default)
- Check SDA/SCL connections
- Test with I2C scanner sketch

**5. Temperature sensor not working**
- Check DHT22 connections
- Verify 10kΩ pull-up resistor on data line
- Replace sensor if faulty


---

## 📊 Performance Metrics

| Metric                 | Value        |
|------------------------|--------------|
| Boot Time              | ~3-5 seconds |
| WiFi Connect           | ~2-8 seconds |
| API Response           | <100ms       |
| Switch Latency         | <50ms        |
| Schedule Accuracy      | ±1 second    |
| Memory Usage (Static)  | ~20KB        |
| Memory Usage (Dynamic) | ~40KB peak   |
| Max Concurrent Clients | 4            |
| NTP Sync Interval      | 24 hours     |
| Sensor Read Interval   | 5 seconds    |

---

### Development Guidelines
- Follow existing code style
- Add comments for complex logic
- Test all features before PR
- Update documentation

---

## 📝 Changelog

### v1.0.0 (Current)
- Initial release
- 5-channel relay control
- Web interface
- Scheduling & automation
- Alexa/Google Home integration
- OTA updates
- Temperature monitoring


## 📝 Note
-Android and IOS Application will be Available Soon..

---

## 📄 License

This project is licensed under the MIT License - see LICENSE file for details.



---

## 👨‍💻 Author

**Traivimiya**
- GitHub: [@LEDGNDARYbrahmin](https://github.com/LEDGNDARYbrahmin)
- Project Link: [TVM-Smart_Switch](https://github.com/LEDGNDARYbrahmin/TVM-Smart_Switch)

---

## 🙏 Acknowledgments

- [ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer) - Async web server library
- [fauxmoESP](https://github.com/vintlabs/fauxmoESP) - Alexa integration
- [ArduinoJson](https://arduinojson.org/) - JSON parsing
- [Adafruit](https://www.adafruit.com/) - OLED and sensor libraries
- ESP32 Community - Invaluable support and resources

---

## 🌐 Additional Resources

- [ESP32 Datasheet](https://www.espressif.com/sites/default/files/documentation/esp32_datasheet_en.pdf)
- [Arduino ESP32 Documentation](https://docs.espressif.com/projects/arduino-esp32/en/latest/)
- [Home Assistant Integration Guide](https://www.home-assistant.io/integrations/esp32/)
- [ESPHome Alternative](https://esphome.io/)

---

<p align="center">
  Made with ❤️ for the IoT community
</p>

<p align="center">
  ⭐ Star this repo if you find it helpful!
</p>

