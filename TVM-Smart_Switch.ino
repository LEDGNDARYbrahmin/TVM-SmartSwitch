
#define ASYNC_TCP_MAX_CLIENTS 4
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <DHT.h>
#include <time.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <WiFiUdp.h>
#include <esp_task_wdt.h>
#include <fauxmoESP.h>
#include <map>

// --- LIBRARIES FOR OLED DISPLAY ---
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// --- LIBRARY FOR OTA UPDATES ---
#include <ESPmDNS.h>
#include <HTTPUpdate.h>

// --- Definitions ---
#define AP_SSID "traivimiya_switch"
#define AP_PASSWORD "traivimiya12"
#define BUZZER_PIN 23 
#define LED_PIN 13
#define CONFIG_BUTTON_PIN 0
#define HOSTNAME "traivimiya-switch"
#define OLED_SDA 22
#define OLED_SCL 21

// --- Pin and Device Configuration ---
const int RELAY_PINS[] = {16, 17, 18, 19, 5};
const int NUM_SWITCHES = 5;
#define DHT_PIN 15
#define DHT_TYPE DHT22
#define MAX_ALARMS 3
#define MAX_SCHEDULES 10

// --- OLED Display Configuration ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// --- Global Objects ---
AsyncWebServer server(80); 
DNSServer dnsServer;
Preferences preferences;
WiFiUDP udp;
fauxmoESP fauxmo;
DHT dht(DHT_PIN, DHT_TYPE);

// --- Global State Variables ---
String switchNames[NUM_SWITCHES];
bool relayStates[NUM_SWITCHES] = {false, false, false, false, false};
float temperature = 0.0;
float humidity = 0.0;
std::map<String, unsigned long> activeClients;
const char* http_user = "admin";
String http_pass = "password";

struct Alarm {
  bool enabled;
  int hour;
  int minute;
};
Alarm alarms[MAX_ALARMS];

bool tempControlEnabled = false;
int tempControlSwitchId = 0;
float tempControlThreshold = 25.0;

struct Schedule {
  int hour;
  int minute;
  bool state;
  bool enabled;
  int switchId;
};
Schedule schedules[MAX_SCHEDULES] = {0};

bool alarmTriggered[MAX_ALARMS] = {false};
unsigned long alarmCycleStart[MAX_ALARMS] = {0};
long timerMillis[NUM_SWITCHES] = {0};
bool timerEnabled[NUM_SWITCHES] = {false};

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 19800;
const int daylightOffset_sec = 0;

unsigned long lastSensorRead = 0;
const long sensorReadInterval = 5000;
unsigned long lastScheduleCheck = 0;
const long scheduleCheckInterval = 1000;
unsigned long lastNtpSync = 0;
const long ntpSyncInterval = 24 * 3600 * 1000;
unsigned long connectionAttemptStart = 0;
const long reconnectInterval = 30000;
unsigned long lastBroadcastTime = 0;
const long broadcastInterval = 3000;
const int udpPort = 12345;
bool inConfigMode = false;
bool shouldRestart = false;
bool timeSynced = false;
bool switchStateChanged = false;
unsigned long lastSwitchSave = 0;
const long switchSaveInterval = 5000;
unsigned long lastDisplayUpdate = 0;
const long displayUpdateInterval = 1000;

enum DisplayScreen { SCREEN_ENVIRONMENT, SCREEN_STATUS, SCREEN_NETWORK };
DisplayScreen currentScreen = SCREEN_ENVIRONMENT;
unsigned long lastScreenChange = 0;
const long screenChangeInterval = 5000;

const char ap_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
  <title>Wi-Fi Setup</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: sans-serif; text-align: center; background-color: #333; color: #fff; }
    .container { max-width: 400px; margin: 50px auto; padding: 20px; border-radius: 10px; background-color: #444; box-shadow: 0 4px 6px rgba(0,0,0,0.1); }
    h1 { color: #fff; }
    input[type="text"], input[type="password"] { width: 90%; padding: 10px; margin: 10px 0; border: 1px solid #555; border-radius: 5px; background-color: #555; color: #fff; }
    button { width: 90%; padding: 12px; margin-top: 20px; border: none; border-radius: 5px; background-color: #007BFF; color: #fff; font-size: 16px; cursor: pointer; }
    button:hover { background-color: #0056b3; }
    #status-message { margin-top: 15px; font-weight: bold; }
    .success { color: #28a745; }
    .error { color: #dc3545; }
    .show-password-container { text-align: left; max-width: 90%; margin: 0 auto 10px auto; font-size: 0.9em; }
  </style>
</head><body>
  <div class="container">
    <h1>Connect to Wi-Fi</h1>
    <p>Please enter your home Wi-Fi details below.</p>
    <div id="status-message"></div>
    <form id="wifi-form">
      <input type="text" id="ssid" name="ssid" placeholder="Wi-Fi SSID">
      <input type="password" id="password" name="password" placeholder="Password">
      <div class="show-password-container">
        <input type="checkbox" id="show-password">
        <label for="show-password">Show Password</label>
      </div>
      <button type="submit">Connect</button>
    </form>
    <p style="margin-top: 20px; font-size: 0.8em; color: #bbb;">If you are unable to connect, press and hold the 'BOOT' button during power-on to reset credentials.</p>
  </div>
  <script>
    document.getElementById('show-password').addEventListener('change', function() {
        const passwordInput = document.getElementById('password');
        passwordInput.type = this.checked ? 'text' : 'password';
    });
    document.getElementById('wifi-form').addEventListener('submit', async function(event) {
      event.preventDefault();
      const ssid = document.getElementById('ssid').value;
      const password = document.getElementById('password').value;
      const statusMessage = document.getElementById('status-message');
      const container = document.querySelector('.container');
      statusMessage.textContent = 'Connecting...';
      statusMessage.className = '';
      try {
        const response = await fetch('/connect', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ ssid: ssid, password: password })
        });
        if (response.ok) {
          container.innerHTML = '<h1>Success!</h1><p class="success">Credentials saved. The device will now restart to connect to your Wi-Fi.</p><p>You may now close this page.</p>';
        } else {
          statusMessage.textContent = 'Failed to save credentials. Try again.';
          statusMessage.className = 'error';
        }
      } catch (e) {
        statusMessage.textContent = 'An error occurred. Please try again.';
        statusMessage.className = 'error';
        console.error(e);
      }
    });
  </script>
</body></html>
)rawliteral";

void beep(int count) {
  for (int i = 0; i < count; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(100);
    digitalWrite(BUZZER_PIN, LOW);
    if (i < count - 1) {
      delay(150);
    }
  }
}

void loadAdminPassword() {
  preferences.begin("auth", true);
  http_pass = preferences.getString("admin_pass", "password");
  preferences.end();
  Serial.print("Admin password loaded. Length: ");
  Serial.println(http_pass.length());
}

void saveAdminPassword(String newPass) {
  preferences.begin("auth", false);
  preferences.putString("admin_pass", newPass);
  preferences.end();
  http_pass = newPass;
  Serial.println("Admin password updated.");
}

void saveAutomationSettings() {
  preferences.begin("automation", false);
  preferences.putBytes("alarms", &alarms, sizeof(alarms));
  preferences.putBool("tempEnabled", tempControlEnabled);
  preferences.putInt("tempSwitchId", tempControlSwitchId);
  preferences.putFloat("tempThreshold", tempControlThreshold);
  preferences.end();
}

void loadAutomationSettings() {
  preferences.begin("automation", true);
  if (preferences.getBytesLength("alarms") == sizeof(alarms)) {
    preferences.getBytes("alarms", &alarms, sizeof(alarms));
  } else {
    for (int i = 0; i < MAX_ALARMS; i++) alarms[i] = {false, 0, 0};
  }
  tempControlEnabled = preferences.getBool("tempEnabled", false);
  tempControlSwitchId = preferences.getInt("tempSwitchId", 0);
  tempControlThreshold = preferences.getFloat("tempThreshold", 25.0);
  preferences.end();
}

void saveSwitchData() {
  preferences.begin("switch-data", false);
  for(int i = 0; i < NUM_SWITCHES; i++) {
    String keyName = "name" + String(i);
    String keyState = "state" + String(i);
    preferences.putString(keyName.c_str(), switchNames[i]);
    preferences.putBool(keyState.c_str(), relayStates[i]);
  }
  preferences.end();
  Serial.println("Switch data saved to flash.");
}

void loadSwitchData() {
  preferences.begin("switch-data", true);
  for(int i = 0; i < NUM_SWITCHES; i++) {
    String keyName = "name" + String(i);
    String keyState = "state" + String(i);
    switchNames[i] = preferences.getString(keyName.c_str(), "Switch " + String(i+1));
    relayStates[i] = preferences.getBool(keyState.c_str(), false);
  }
  preferences.end();
}

void saveSchedules() {
  preferences.begin("schedules", false);
  preferences.putBytes("list", &schedules, sizeof(schedules));
  preferences.end();
}

void loadSchedules() {
  preferences.begin("schedules", true);
  size_t bytesRead = preferences.getBytes("list", &schedules, sizeof(schedules));
  if (bytesRead == 0) {
    for (int i = 0; i < MAX_SCHEDULES; i++) schedules[i].enabled = false;
  }
  preferences.end();
}

void saveWifiCredentials(const char* ssid, const char* password) {
  preferences.begin("wifi-creds", false);
  preferences.putString("ssid", ssid);
  preferences.putString("password", password);
  preferences.end();
  Serial.println("Wi-Fi credentials saved to memory.");
}

void clearWifiCredentials() {
  preferences.begin("wifi-creds", false);
  preferences.clear();
  preferences.end();
  Serial.println("Cleared saved Wi-Fi credentials.");
}

bool loadWifiCredentials(String& ssid_out, String& password_out) {
  preferences.begin("wifi-creds", true);
  ssid_out = preferences.getString("ssid", "");
  password_out = preferences.getString("password", "");
  preferences.end();
  return ssid_out != "";
}

void setupFauxmo() {
  fauxmo.setPort(80);
  fauxmo.enable(true);
  for (int i = 0; i < NUM_SWITCHES; i++) {
    fauxmo.addDevice(switchNames[i].c_str());
  }
  fauxmo.onSetState([](unsigned char device_id, const char * device_name, bool state, unsigned char value) {
    Serial.printf("[Voice Control] Device #%u (%s) state: %s\n", device_id, device_name, state ? "ON" : "OFF");
    if (device_id < NUM_SWITCHES) {
      digitalWrite(RELAY_PINS[device_id], state ? LOW : HIGH);
      relayStates[device_id] = state;
      switchStateChanged = true;
    }
  });
}

void setupRelays() {
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  for (int i = 0; i < NUM_SWITCHES; i++) {
    pinMode(RELAY_PINS[i], OUTPUT);
    digitalWrite(RELAY_PINS[i], HIGH);
  }
}

void setRelayState(int relayId, bool state) {
  if (relayId >= 0 && relayId < NUM_SWITCHES) {
    digitalWrite(RELAY_PINS[relayId], state ? LOW : HIGH);
    relayStates[relayId] = state;
    switchStateChanged = true;
    Serial.printf("[RELAY] Switch %d (%s) turned %s\n", relayId, switchNames[relayId].c_str(), state ? "ON" : "OFF");
  }
}

void handleTempHumidity() {
  float newHumidity = dht.readHumidity();
  float newTemperature = dht.readTemperature();
  if (!isnan(newHumidity) && !isnan(newTemperature)) {
    humidity = newHumidity;
    temperature = newTemperature;
  }
}

void handleBuzzer() {
    if (!timeSynced) return;
    
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) return;

    for (int i=0; i < MAX_ALARMS; i++) {
      if (alarms[i].enabled && timeinfo.tm_hour == alarms[i].hour && timeinfo.tm_min == alarms[i].minute) {
          if (!alarmTriggered[i]) {
              alarmTriggered[i] = true;
              alarmCycleStart[i] = millis();
              Serial.printf("Alarm %d triggered!\n", i);
          }
          unsigned long elapsed = millis() - alarmCycleStart[i];
          if ((elapsed % 4000) < 1000) {
              digitalWrite(BUZZER_PIN, (elapsed % 250 < 125) ? HIGH : LOW);
          } else {
              digitalWrite(BUZZER_PIN, LOW);
          }
      } else {
          if (alarmTriggered[i]) {
              digitalWrite(BUZZER_PIN, LOW);
              alarmTriggered[i] = false;
              Serial.printf("Alarm %d time passed.\n", i);
          }
      }
    }
}

void handleSchedules() {
  if (millis() - lastScheduleCheck > scheduleCheckInterval) {
    lastScheduleCheck = millis();
    struct tm timeinfo;
    if(!getLocalTime(&timeinfo)){
      return;
    }
    for (int i = 0; i < MAX_SCHEDULES; i++) {
      if (schedules[i].enabled && schedules[i].hour == timeinfo.tm_hour && schedules[i].minute == timeinfo.tm_min) {
        if (relayStates[schedules[i].switchId] != schedules[i].state) {
          Serial.printf("Schedule triggered for Switch %d.\n", schedules[i].switchId);
          setRelayState(schedules[i].switchId, schedules[i].state);
          beep(1);
        }
      }
    }
  }
}

void handleTimers() {
  for(int i = 0; i < NUM_SWITCHES; i++) {
    if (timerEnabled[i] && millis() >= timerMillis[i]) {
      Serial.printf("Timer expired for Switch %d.\n", i);
      setRelayState(i, !relayStates[i]);
      timerEnabled[i] = false;
      timerMillis[i] = 0;
      beep(1);
    }
  }
}

void handleTemperatureControl() {
  if (!tempControlEnabled) return;
  
  if (temperature > tempControlThreshold && !relayStates[tempControlSwitchId]) {
    Serial.printf("[TEMP] Threshold %.1fC exceeded. Turning ON switch %d.\n", tempControlThreshold, tempControlSwitchId);
    setRelayState(tempControlSwitchId, true);
  } else if (temperature <= tempControlThreshold && relayStates[tempControlSwitchId]) {
    Serial.printf("[TEMP] Temperature is below %.1fC. Turning OFF switch %d.\n", tempControlThreshold, tempControlSwitchId);
    setRelayState(tempControlSwitchId, false);
  }
}

void drawEnvironmentScreen() {
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.print("Environment");

  display.setTextSize(1);
  if (timeSynced) {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      char timeString[12];
      int hour12 = timeinfo.tm_hour % 12;
      if (hour12 == 0) hour12 = 12; // Handle midnight
      sprintf(timeString, "%02d:%02d:%02d %s", hour12, timeinfo.tm_min, timeinfo.tm_sec, timeinfo.tm_hour < 12 ? "AM" : "PM");
      
      display.setCursor(0, 20);
      display.print("Time:     "); // Clear previous text
      display.setCursor(0, 20);
      display.print("Time: ");
      display.print(timeString);
    }
  } else {
    display.setCursor(0, 20);
    display.print("Time: Syncing...");
  }
  
  display.setCursor(0, 35);
  display.print("Temp:     "); // Clear previous text
  display.setCursor(0, 35);
  display.print("Temp: ");
  display.print(temperature, 1);
  display.drawCircle(display.getCursorX() + 5, display.getCursorY() + 1, 2, SSD1306_WHITE); // Degree symbol
  display.print(" C");

  display.setCursor(0, 50);
  display.print("Humidity: ");
  display.print(humidity, 1);
  display.print(" %");
}

void drawStatusScreen() {
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.print("Status");

  for (int i = 0; i < NUM_SWITCHES; i++) {
    int x = 10 + (i * 24);
    int y = 40;
    
    display.setTextSize(1);
    display.setCursor(x - 3, y + 15);
    display.print(i + 1);

    if (relayStates[i]) {
      display.fillCircle(x, y, 6, SSD1306_WHITE);
    } else {
      display.drawCircle(x, y, 6, SSD1306_WHITE);
    }
  }
}

void drawNetworkScreen() {
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.print("Network");

  display.setTextSize(1);
  display.setCursor(0, 25);
  if (WiFi.status() == WL_CONNECTED) {
    display.print("SSID: ");
    display.println(WiFi.SSID());
    display.setCursor(0, 45); // Added space
    display.print("IP: ");
    display.println(WiFi.localIP().toString());
  } else {
    display.println("Status: Disconnected");
    display.setCursor(0, 45); // Added space
    display.print("AP Mode: ");
    display.println(AP_SSID);
  }
}

void updateDisplay() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);


  if (WiFi.status() == WL_CONNECTED) {

    display.fillCircle(124, 3, 3, SSD1306_WHITE);
  }
  
  switch (currentScreen) {
    case SCREEN_ENVIRONMENT:
      drawEnvironmentScreen();
      break;
    case SCREEN_STATUS:
      drawStatusScreen();
      break;
    case SCREEN_NETWORK:
      drawNetworkScreen();
      break;
  }

  display.display();
}

void updateClientActivity(AsyncWebServerRequest *request);

void setupServerHandlers() {
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-control-allow-methods", "GET, POST, PUT, DELETE, OPTIONS");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
  
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", ap_html);
  });

  server.on("/connect", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, (const char*)data, len) != DeserializationError::Ok) { request->send(400, "text/plain", "Invalid JSON"); return; }
    String ssid_str = doc["ssid"].as<String>();
    String password_str = doc["password"].as<String>();
    if (ssid_str.length() == 0) { request->send(400, "text/plain", "SSID cannot be empty."); return; }
    Serial.println("Credentials received. Saving and restarting...");
    saveWifiCredentials(ssid_str.c_str(), password_str.c_str());
    request->send(200, "text/plain", "Credentials saved. Restarting.");
    shouldRestart = true;
  });

  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(http_user, http_pass.c_str())) return request->requestAuthentication();
    updateClientActivity(request); 
    StaticJsonDocument<2048> doc;
    doc["temperature"] = temperature;
    doc["humidity"] = humidity;

    JsonObject config = doc.createNestedObject("config");
    config["numSwitches"] = NUM_SWITCHES;
    config["maxSchedules"] = MAX_SCHEDULES;
    config["maxAlarms"] = MAX_ALARMS;
    
    JsonArray alarmsArray = doc.createNestedArray("alarms");
    for(int i = 0; i < MAX_ALARMS; i++) {
      JsonObject alarmObj = alarmsArray.createNestedObject();
      alarmObj["enabled"] = alarms[i].enabled;
      alarmObj["hour"] = alarms[i].hour;
      alarmObj["minute"] = alarms[i].minute;
    }
    
    JsonObject tempObj = doc.createNestedObject("temp_control");
    tempObj["enabled"] = tempControlEnabled;
    tempObj["switchId"] = tempControlSwitchId;
    tempObj["threshold"] = tempControlThreshold;

    JsonArray switchesArray = doc.createNestedArray("switches");
    for (int i = 0; i < NUM_SWITCHES; i++) {
      JsonObject sw = switchesArray.createNestedObject();
      sw["id"] = i;
      sw["name"] = switchNames[i];
      sw["state"] = relayStates[i];
    }
    
    JsonArray schedulesArray = doc.createNestedArray("schedules");
    for (int i = 0; i < MAX_SCHEDULES; i++) {
        JsonObject schedule = schedulesArray.createNestedObject();
        schedule["id"] = i;
        schedule["switchId"] = schedules[i].switchId;
        schedule["hour"] = schedules[i].hour;
        schedule["minute"] = schedules[i].minute;
        schedule["state"] = schedules[i].state;
        schedule["enabled"] = schedules[i].enabled;
    }

    doc["connected"] = (WiFi.status() == WL_CONNECTED);
    String jsonResponse;
    serializeJson(doc, jsonResponse);
    request->send(200, "application/json", jsonResponse);
  });

  server.on("/clients", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(http_user, http_pass.c_str())) return request->requestAuthentication();
    
    unsigned long now = millis();
    for (auto it = activeClients.cbegin(); it != activeClients.cend(); ) {
      if (now - it->second > 30000) {
        it = activeClients.erase(it);
      } else {
        ++it;
      }
    }
    
    StaticJsonDocument<512> doc;
    JsonArray clientsArray = doc.createNestedArray("clients");
    for (auto const& [ip, lastSeen] : activeClients) {
      clientsArray.add(ip);
    }
    
    String jsonResponse;
    serializeJson(doc, jsonResponse);
    request->send(200, "application/json", jsonResponse);
  });

  server.on("/toggle", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(http_user, http_pass.c_str())) return request->requestAuthentication();
    updateClientActivity(request);
    if (request->hasParam("id")) {
      int id = request->getParam("id")->value().toInt();
      setRelayState(id, !relayStates[id]);
      request->send(200, "text/plain", "OK");
    } else {
      request->send(400, "text/plain", "Missing switch ID.");
    }
  });

  server.on("/rename", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (!request->authenticate(http_user, http_pass.c_str())) return request->requestAuthentication();
    updateClientActivity(request);
    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, (const char*)data, len) != DeserializationError::Ok) { request->send(400, "text/plain", "Invalid JSON"); return; }
    if (!doc.containsKey("id") || !doc.containsKey("name")) { request->send(400, "text/plain", "Missing id or name."); return; }
    int id = doc["id"];
    String name = doc["name"].as<String>();
    if (id >= 0 && id < NUM_SWITCHES && name.length() > 0) {
      switchNames[id] = name;
      saveSwitchData();
      
      fauxmo.enable(false);
      setupFauxmo();
      
      request->send(200, "text/plain", "OK");
    } else {
      request->send(400, "text/plain", "Invalid switch ID or name.");
    }
  });

  server.on("/addSchedule", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (!request->authenticate(http_user, http_pass.c_str())) return request->requestAuthentication();
    updateClientActivity(request);
    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, (const char*)data, len) != DeserializationError::Ok) { request->send(400, "text/plain", "Invalid JSON"); return; }
    if (!doc.containsKey("id") || !doc.containsKey("hour") || !doc.containsKey("minute") || !doc.containsKey("switchId") || !doc.containsKey("state") || !doc.containsKey("enabled")) {
        request->send(400, "text/plain", "Missing schedule parameters."); return;
    }
    int id = doc["id"];
    int hour = doc["hour"];
    int minute = doc["minute"];
    int switchId = doc["switchId"];
    if (id >= 0 && id < MAX_SCHEDULES && hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59 && switchId >= 0 && switchId < NUM_SWITCHES) {
      schedules[id].hour = hour;
      schedules[id].minute = minute;
      schedules[id].state = doc["state"];
      schedules[id].enabled = doc["enabled"];
      schedules[id].switchId = switchId;
      saveSchedules();
      Serial.printf("[SCHEDULE] Schedule %d updated: Switch %d to %s at %02d:%02d. Enabled: %s\n", id, switchId, schedules[id].state ? "ON" : "OFF", hour, minute, schedules[id].enabled ? "Yes" : "No");
      request->send(200, "text/plain", "OK");
    } else {
      request->send(400, "text/plain", "Invalid schedule parameters.");
    }
  });

  server.on("/deleteSchedule", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (!request->authenticate(http_user, http_pass.c_str())) return request->requestAuthentication();
    updateClientActivity(request);
    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, (const char*)data, len) != DeserializationError::Ok) { request->send(400, "text/plain", "Invalid JSON"); return; }
    if (!doc.containsKey("id")) { request->send(400, "text/plain", "Missing id."); return; }
    int id = doc["id"];
    if (id >= 0 && id < MAX_SCHEDULES) {
      schedules[id].enabled = false;
      saveSchedules();
      Serial.printf("[SCHEDULE] Schedule %d deleted (disabled).\n", id);
      request->send(200, "text/plain", "OK");
    } else {
      request->send(400, "text/plain", "Invalid schedule ID.");
    }
  });

  server.on("/startTimer", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (!request->authenticate(http_user, http_pass.c_str())) return request->requestAuthentication();
    updateClientActivity(request);
    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, (const char*)data, len) != DeserializationError::Ok) { request->send(400, "text/plain", "Invalid JSON"); return; }
    if (!doc.containsKey("id") || !doc.containsKey("duration")) { request->send(400, "text/plain", "Missing 'id' or 'duration'."); return; }
    int id = doc["id"];
    long duration_sec = doc["duration"];
    if (id >= 0 && id < NUM_SWITCHES && duration_sec > 0) {
        timerMillis[id] = millis() + (duration_sec * 1000);
        timerEnabled[id] = true;
        Serial.printf("Timer started for switch %d for %ld seconds.\n", id, duration_sec);
        request->send(200, "text/plain", "Timer started");
    } else {
        request->send(400, "text/plain", "Invalid ID or duration.");
    }
  });

  server.on("/clearCredentials", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(http_user, http_pass.c_str())) return request->requestAuthentication();
    updateClientActivity(request);
    clearWifiCredentials();
    request->send(200, "text/plain", "Wi-Fi credentials cleared. Restarting.");
    shouldRestart = true;
  });
  
  server.on("/setAlarms", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (!request->authenticate(http_user, http_pass.c_str())) return request->requestAuthentication();
    updateClientActivity(request);
    StaticJsonDocument<512> doc;
    if (deserializeJson(doc, (const char*)data, len) != DeserializationError::Ok) { request->send(400, "text/plain", "Invalid JSON"); return; }
    JsonArray alarmsArray = doc["alarms"];
    if (alarmsArray.size() > MAX_ALARMS) { request->send(400, "text/plain", "Too many alarms."); return; }
    
    Serial.println("[ALARM] Alarms settings received. Updating...");
    int i = 0;
    for (JsonVariant v : alarmsArray) {
        if (i < MAX_ALARMS) {
            alarms[i].enabled = v["enabled"];
            alarms[i].hour = v["hour"];
            alarms[i].minute = v["minute"];
            if (alarms[i].enabled) {
              Serial.printf("  - Alarm %d set for %02d:%02d\n", i, alarms[i].hour, alarms[i].minute);
            }
            i++;
        }
    }
    for (; i < MAX_ALARMS; i++) {
        if (alarms[i].enabled) {
          Serial.printf("  - Alarm %d disabled.\n", i);
        }
        alarms[i] = {false, 0, 0};
    }

    saveAutomationSettings();
    Serial.println("[ALARM] Alarms updated.");
    request->send(200, "text/plain", "OK");
  });

  server.on("/setTempControl", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(http_user, http_pass.c_str())) return request->requestAuthentication();
    updateClientActivity(request);
    if (request->hasParam("enabled") && request->hasParam("switchId") && request->hasParam("threshold")) {
      tempControlEnabled = request->getParam("enabled")->value().toInt() == 1;
      tempControlSwitchId = request->getParam("switchId")->value().toInt();
      tempControlThreshold = request->getParam("threshold")->value().toFloat();
      saveAutomationSettings();
      Serial.printf("[TEMP] Control updated. Enabled: %s, Switch: %d, Threshold: %.1fC\n", tempControlEnabled ? "Yes" : "No", tempControlSwitchId, tempControlThreshold);
      request->send(200, "text/plain", "OK");
    } else {
      request->send(400, "text/plain", "Missing temp control parameters.");
    }
  });

  server.on("/updateWifi", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (!request->authenticate(http_user, http_pass.c_str())) return request->requestAuthentication();
    updateClientActivity(request);
    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, (const char*)data, len) != DeserializationError::Ok) { request->send(400, "text/plain", "Invalid JSON"); return; }
    String ssid_str = doc["ssid"].as<String>();
    String password_str = doc["password"].as<String>();
    if (ssid_str.length() == 0) { request->send(400, "text/plain", "SSID cannot be empty."); return; }
    saveWifiCredentials(ssid_str.c_str(), password_str.c_str());
    request->send(200, "text/plain", "OK. Restarting to connect to new network.");
    shouldRestart = true;
  });
  
  server.on("/updateAuth", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (!request->authenticate(http_user, http_pass.c_str())) return request->requestAuthentication();
    updateClientActivity(request);
    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, (const char*)data, len) != DeserializationError::Ok) { request->send(400, "text/plain", "Invalid JSON"); return; }
    if (!doc.containsKey("password")) { request->send(400, "text/plain", "Missing password."); return; }
    String newPass = doc["password"].as<String>();
    if (newPass.length() > 0) {
      saveAdminPassword(newPass);
      request->send(200, "text/plain", "Password updated.");
    } else {
      request->send(400, "text/plain", "Password cannot be empty.");
    }
  });

  server.on("/firmware-update", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (!request->authenticate(http_user, http_pass.c_str())) return request->requestAuthentication();
    
    StaticJsonDocument<512> doc;
    if (deserializeJson(doc, (const char*)data, len) != DeserializationError::Ok) {
      request->send(400, "text/plain", "Invalid JSON");
      return;
    }
    
    if (!doc.containsKey("url")) {
      request->send(400, "text/plain", "Missing firmware URL.");
      return;
    }

    String url = doc["url"];
    if (url.length() == 0 || !url.startsWith("http")) {
      request->send(400, "text/plain", "Invalid URL.");
      return;
    }

    request->send(200, "text/plain", "Update command received. Device will now attempt to update and restart.");
    
    delay(1000);

    WiFiClient client;
    t_httpUpdate_return ret = httpUpdate.update(client, url);

    switch (ret) {
      case HTTP_UPDATE_FAILED:
        Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
        break;
      case HTTP_UPDATE_NO_UPDATES:
        Serial.println("HTTP_UPDATE_NO_UPDATES");
        break;
      case HTTP_UPDATE_OK:
        Serial.println("HTTP_UPDATE_OK");
        break;
    }
  });

  server.onNotFound([](AsyncWebServerRequest *request) {
    if (WiFi.getMode() == WIFI_AP) { request->redirect("/"); } else { request->send(404, "text/plain", "Not found"); }
  });
}

void updateClientActivity(AsyncWebServerRequest *request) {
  if (request->authenticate(http_user, http_pass.c_str())) {
    String clientIp = request->client()->remoteIP().toString();
    activeClients[clientIp] = millis();
  }
}

void startApMode() {
  Serial.println("--- Starting AP Mode ---");
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  Serial.print("Access Point IP address: ");
  Serial.println(WiFi.softAPIP());
  dnsServer.start(53, "*", WiFi.softAPIP());
  inConfigMode = true;
  beep(3);
}

void syncTime() {
  Serial.println("Attempting to sync time with NTP server...");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 10000)) {
    Serial.println("...Time synchronized successfully!");
    timeSynced = true;
    lastNtpSync = millis();
  } else {
    Serial.println("...Failed to get time.");
    timeSynced = false;
  }
}

void setup() {
  Serial.begin(115200);

  Wire.begin(OLED_SDA, OLED_SCL);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 28);
  display.println("TRAIVIMIYA SWITCH");
  display.display();
  delay(1500);

  delay(1000);
  Serial.println("\n\n--- Device Booting Up ---");
  
  pinMode(LED_PIN, OUTPUT);
  pinMode(CONFIG_BUTTON_PIN, INPUT_PULLUP);
  
  Serial.println("Initializing Watchdog Timer...");
  esp_task_wdt_deinit();
  esp_task_wdt_config_t twdt_config = { .timeout_ms = 30000, .idle_core_mask = (1 << portNUM_PROCESSORS) - 1, .trigger_panic = true };
  esp_task_wdt_init(&twdt_config);
  esp_task_wdt_add(NULL);
  Serial.println("...Watchdog initialized.");

  if (digitalRead(CONFIG_BUTTON_PIN) == LOW) {
    Serial.println("Config button pressed. Clearing credentials...");
    clearWifiCredentials();
    delay(1000);
    ESP.restart();
  }

  Serial.println("Initializing hardware...");
  setupRelays();
  dht.begin();
  loadAdminPassword();
  loadSwitchData();
  loadSchedules();
  loadAutomationSettings();
  for(int i = 0; i < NUM_SWITCHES; i++) {
    digitalWrite(RELAY_PINS[i], relayStates[i] ? LOW : HIGH);
  }
  Serial.println("...Hardware initialized.");
  
  String loaded_ssid, loaded_password;
  if (loadWifiCredentials(loaded_ssid, loaded_password)) {
    Serial.print("Found saved credentials. Connecting to ");
    Serial.println(loaded_ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(loaded_ssid.c_str(), loaded_password.c_str());
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40) {
      delay(500);
      Serial.print(".");
      attempts++;
    }
    Serial.println();
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\n--- Wi-Fi Connected! ---");
      Serial.print("IP Address: ");
      Serial.println(WiFi.localIP());
      beep(2);
      syncTime();
      Serial.println("Initializing OTA...");
      ArduinoOTA.setHostname(HOSTNAME).begin();
      Serial.println("...OTA initialized.");
    } else {
        Serial.println("--- Wi-Fi Connection Failed ---");
        WiFi.disconnect(true, true);
        startApMode();
    }
  } else {
    Serial.println("--- No credentials found ---");
    startApMode();
  }

  Serial.println("Setting up server handlers...");
  setupServerHandlers();
  Serial.println("...Server handlers set up.");

  Serial.println("Starting web server on port 80...");
  server.begin();
  Serial.println("--- Web Server Started ---");

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Initializing fauxmoESP (Alexa/Google)...");
    setupFauxmo();
    Serial.println("...fauxmoESP initialized.");
  }

  Serial.println("--- Setup Complete. Entering main loop. ---");
}

void loop() {
  esp_task_wdt_reset();
  
  if (millis() - lastDisplayUpdate > displayUpdateInterval) {
    updateDisplay();
    lastDisplayUpdate = millis();
  }

  if (millis() - lastScreenChange > screenChangeInterval) {
    currentScreen = static_cast<DisplayScreen>((static_cast<int>(currentScreen) + 1) % 3);
    lastScreenChange = millis();
    updateDisplay();
  }

  if (shouldRestart) {
    Serial.println("Restarting device...");
    delay(1000);
    ESP.restart();
  }

  if (switchStateChanged && (millis() - lastSwitchSave > switchSaveInterval)) {
    saveSwitchData();
    switchStateChanged = false;
    lastSwitchSave = millis();
  }

  if (inConfigMode) {
    digitalWrite(LED_PIN, (millis() / 200) % 2 == 0 ? LOW : HIGH);
    dnsServer.processNextRequest();
  } else {
    if (WiFi.status() == WL_CONNECTED) {
      digitalWrite(LED_PIN, HIGH);
      fauxmo.handle(); 
      ArduinoOTA.handle();

      if (millis() - lastBroadcastTime > broadcastInterval) {
        lastBroadcastTime = millis();
        String message = "ESP32_DEVICE:" + String(HOSTNAME) + ":" + WiFi.localIP().toString();
        udp.beginPacket("255.255.255.255", udpPort);
        udp.print(message);
        udp.endPacket();
      }

      if (!timeSynced || (millis() - lastNtpSync > ntpSyncInterval)) {
        syncTime();
      }
      
      if (timeSynced) {
        if (millis() - lastSensorRead > sensorReadInterval) {
          handleTempHumidity();
          lastSensorRead = millis();
        }
        handleSchedules();
        handleTimers();
        handleTemperatureControl();
        handleBuzzer();
      }
    } else {
      digitalWrite(LED_PIN, (millis() / 500) % 2 == 0 ? LOW : HIGH);
      
      if (millis() - connectionAttemptStart > reconnectInterval) {
        Serial.println("Wi-Fi connection lost. Attempting non-blocking reconnect...");
        WiFi.disconnect(true);
        delay(100);
        String loaded_ssid, loaded_password;
        if (loadWifiCredentials(loaded_ssid, loaded_password)) {
            WiFi.begin(loaded_ssid.c_str(), loaded_password.c_str());
        }
        connectionAttemptStart = millis();
      }
    }
  }
}

