#include <M5Atom.h>
#include <SD.h>
#include <SPI.h>
#include <TinyGPS++.h>
#include <WiFi.h>
#include <NimBLEDevice.h>

const String BUILD = "1.7.0";
const String VERSION = "1.7";

// LED
bool ledState = false;
bool buttonLedState = true;
#define RED 0xff0000
#define GREEN 0x00ff00
#define BLUE 0x0000ff
#define YELLOW 0xffff00
#define ORANGE 0xffa500
#define PURPLE 0x800080
#define CYAN 0x00ffff
#define WHITE 0xffffff
#define OFF 0x000000

// Scan & GPS
TinyGPSPlus gps;
char fileName[50];
const int maxMACs = 150;
char macAddressArray[maxMACs][20];
int macArrayIndex = 0;
int timePerChannel[14] = { 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 50, 50, 50 };
float lat;
float lon;
float altitude;
float accuracy;
int numSatellites;

// BLE Scanning
NimBLEScan* pBLEScan;
const int BLE_SCAN_TIME = 0;
const int BLE_MAX_RESULTS = 50;
bool isMACSeen(const char* mac);
void logData(const char* data);
String sanitizeCSVField(String field);

// Speed-based scan vars
double speed = -1;
static int stop = 500;
static int slow = 250;
static int fast = 100;
static int uninitialized = 250;

// Configurable vars from SD
bool speedBased = false;
int scanDelay = 150;
bool adaptiveScan = true;
bool bleScanEnabled = true;
int channels[14] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 };
char filePrefix[50] = "AtomWigler";

String sanitizeCSVField(String field) {
  field.replace(",", "_");
  field.replace("\"", "\"\"");
  return field;
}

class BLEScanCallbacks : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice* advertisedDevice) override {
    if (!gps.location.isValid()) return;

    char currentMAC[20];
    strcpy(currentMAC, advertisedDevice->getAddress().toString().c_str());
    
    if (!isMACSeen(currentMAC)) {
      strcpy(macAddressArray[macArrayIndex++], currentMAC);
      if (macArrayIndex >= maxMACs) macArrayIndex = 0;

      char utc[21];
      sprintf(utc, "%04d-%02d-%02d %02d:%02d:%02d", 
              gps.date.year(), gps.date.month(), gps.date.day(), 
              gps.time.hour(), gps.time.minute(), gps.time.second());

      String deviceName = advertisedDevice->haveName() ? 
                          advertisedDevice->getName().c_str() : "";
      deviceName = sanitizeCSVField(deviceName);

      char dataString[300];
      snprintf(dataString, sizeof(dataString), 
               "%s,\"%s\",[BLE],%s,0,%d,%.6f,%.6f,%.2f,%.2f,BLE", 
               currentMAC, 
               deviceName.c_str(),
               utc, 
               advertisedDevice->getRSSI(), 
               lat, lon, altitude, accuracy);
      logData(dataString);
    }
  }

  void onScanEnd(const NimBLEScanResults& results, int reason) override {
    if (bleScanEnabled && pBLEScan) {
      pBLEScan->clearResults();
    }
  }
};

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Starting AtomWigler...");
  M5.begin(true, false, true);
  SPI.begin(23, 33, 19, -1);
  delay(1000);

  while (!SD.begin(15, SPI, 40000000)) {
    Serial.println("SD Card initialization failed! Retrying...");
    blinkLED(RED, 500);
    delay(1000);
  }

  Serial.println("SD Card initialized.");

  if (SD.exists("/config.txt")) {
    File configFile = SD.open("/config.txt");
    if (configFile) {
      parseConfigFile(configFile);
      configFile.close();

      Serial.println("Configuration values:");
      Serial.print("speedBased: ");
      Serial.println(speedBased ? "true" : "false");
      Serial.print("scanDelay: ");
      Serial.println(scanDelay);
      Serial.print("adaptiveScan: ");
      Serial.println(adaptiveScan ? "true" : "false");
      Serial.print("filePrefix: ");
      Serial.println(filePrefix);
      Serial.print("bleScanEnabled: ");
      Serial.println(bleScanEnabled ? "true" : "false");
      Serial.print("channels: ");
      for (int i = 0; i < sizeof(channels) / sizeof(channels[0]); i++) {
        if (channels[i] != 0) {
          Serial.print(channels[i]);
          if (i < sizeof(channels) / sizeof(channels[0]) - 1 && channels[i + 1] != 0) {
            Serial.print(", ");
          }
        } else {
          break;
        }
      }
      Serial.println();
    } else {
      Serial.println("Failed to open config file.");
    }
  } else {
    Serial.println("Config file not found.");
  }

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  Serial.println("WiFi initialized.");

  if (bleScanEnabled) {
    NimBLEDevice::init("");
    pBLEScan = NimBLEDevice::getScan();
    pBLEScan->setScanCallbacks(new BLEScanCallbacks());
    pBLEScan->setActiveScan(true);
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);
    pBLEScan->setMaxResults(BLE_MAX_RESULTS);
    pBLEScan->start(BLE_SCAN_TIME, false);
    Serial.println("BLE scan initialized.");
  } else {
    Serial.println("BLE scan disabled by config.");
  }

  Serial1.begin(9600, SERIAL_8N1, 22, -1);
  delay(1000);
  Serial.println("GPS Serial initialized.");
  waitForGPSFix();
  initializeFile();
  
  Serial.print("Free heap after setup: ");
  Serial.println(ESP.getFreeHeap());
}

void loop() {
  static unsigned long lastBlinkTime = 0;
  const unsigned long blinkInterval = 2500;
  static unsigned long lastMemCheck = 0;
  const unsigned long memCheckInterval = 30000;

  M5.update();
  if (M5.Btn.wasPressed()) {
    buttonLedState = !buttonLedState;
    delay(50);
  }

  while (Serial1.available() > 0) {
    gps.encode(Serial1.read());
  }

  if (gps.location.isValid()) {
    unsigned long currentMillis = millis();
    if (currentMillis - lastBlinkTime >= blinkInterval && buttonLedState) {
      M5.dis.drawpix(0, GREEN);
      delay(60);
      M5.dis.clear();
      lastBlinkTime = currentMillis;
    }

    if (currentMillis - lastMemCheck >= memCheckInterval) {
      uint32_t freeHeap = ESP.getFreeHeap();
      Serial.print("Free heap: ");
      Serial.println(freeHeap);
      if (freeHeap < 20000) {
        Serial.println("WARNING: Low memory detected!");
      }
      lastMemCheck = currentMillis;
    }

    lat = gps.location.lat();
    lon = gps.location.lng();
    altitude = gps.altitude.meters();
    accuracy = gps.hdop.hdop();
    speed = gps.speed.mph();

    char utc[21];
    sprintf(utc, "%04d-%02d-%02d %02d:%02d:%02d", 
            gps.date.year(), gps.date.month(), gps.date.day(), 
            gps.time.hour(), gps.time.minute(), gps.time.second());

    for (int i = 0; i < sizeof(channels) / sizeof(channels[0]); i++) {
      int channel = channels[i];
      if (channel == 0) break;
      
      int numNetworks = WiFi.scanNetworks(false, true, false, timePerChannel[channel - 1], channel);
      for (int j = 0; j < numNetworks; j++) {
        char currentMAC[20];
        strcpy(currentMAC, WiFi.BSSIDstr(j).c_str());
        if (!isMACSeen(currentMAC)) {
          strcpy(macAddressArray[macArrayIndex++], currentMAC);
          if (macArrayIndex >= maxMACs) macArrayIndex = 0;
          
          String ssid = WiFi.SSID(j);
          ssid = sanitizeCSVField(ssid);
          
          char dataString[300];
          snprintf(dataString, sizeof(dataString), 
                   "%s,\"%s\",%s,%s,%d,%d,%.6f,%.6f,%.2f,%.2f,WIFI", 
                   currentMAC, ssid.c_str(), 
                   getAuthType(WiFi.encryptionType(j)), utc, 
                   WiFi.channel(j), WiFi.RSSI(j), 
                   lat, lon, altitude, accuracy);
          logData(dataString);
        }
      }
      WiFi.scanDelete();
      
      if (adaptiveScan) {
        updateTimePerChannel(channel, numNetworks);
      }
    }
  } else {
    speed = -1;
    blinkLED(PURPLE, 250);
  }

  if (speedBased) {
    delay(*getSpeed(speed));
  } else {
    delay(scanDelay);
  }
}

void updateTimePerChannel(int channel, int networksFound) {
  const int FEW_NETWORKS_THRESHOLD = 1;
  const int MANY_NETWORKS_THRESHOLD = 7;
  const int TIME_INCREMENT = 50;
  const int MAX_TIME = 500;
  const int MIN_TIME = 50;
  if (networksFound >= MANY_NETWORKS_THRESHOLD) {
    timePerChannel[channel - 1] = min(timePerChannel[channel - 1] + TIME_INCREMENT, MAX_TIME);
  } else if (networksFound <= FEW_NETWORKS_THRESHOLD) {
    timePerChannel[channel - 1] = max(timePerChannel[channel - 1] - TIME_INCREMENT, MIN_TIME);
  }
}

void waitForGPSFix() {
  Serial.println("Waiting for GPS fix...");
  while (!gps.location.isValid()) {
    numSatellites = gps.satellites.value();
    if (Serial1.available() > 0) {
      gps.encode(Serial1.read());
    }
    blinkLEDFaster(numSatellites);
  }
  M5.dis.clear();
  Serial.println("GPS fix obtained.");
}

const int* getSpeed(double speed) {
  if (speed == -1) {
    return &uninitialized;
  } else if (speed < 1) {
    return &stop;
  } else if (speed <= 15) {
    return &slow;
  } else {
    return &fast;
  }
}

void blinkLEDFaster(int numSatellites) {
  unsigned long interval;
  if (numSatellites <= 1) {
    interval = 1000;
  } else {
    interval = max(50, 1000 / numSatellites);
  }
  
  static unsigned long previousBlinkMillis = 0;
  unsigned long currentMillis = millis();

  if (currentMillis - previousBlinkMillis >= interval) {
    ledState = !ledState;
    M5.dis.drawpix(0, ledState ? PURPLE : OFF);
    previousBlinkMillis = currentMillis;
  }
}

void blinkLED(uint32_t color, unsigned long interval) {
  static unsigned long previousBlinkMillis = 0;
  unsigned long currentMillis = millis();

  if (currentMillis - previousBlinkMillis >= interval) {
    ledState = !ledState;
    M5.dis.drawpix(0, ledState ? color : OFF);
    previousBlinkMillis = currentMillis;
  }
}

void initializeFile() {
  int fileNumber = 0;
  bool isNewFile = false;
  char fileDateStamp[16];
  sprintf(fileDateStamp, "%04d-%02d-%02d-", gps.date.year(), gps.date.month(), gps.date.day());
  do {
    snprintf(fileName, sizeof(fileName), "/%s-%s%d.csv", filePrefix, fileDateStamp, fileNumber);
    isNewFile = !SD.exists(fileName);
    fileNumber++;
  } while (!isNewFile);
  if (isNewFile) {
    File dataFile = SD.open(fileName, FILE_WRITE);
    if (dataFile) {
      dataFile.print("WigleWifi-1.4,appRelease=");
      dataFile.print(BUILD);
      dataFile.print(",model=AtomWigler,release=");
      dataFile.print(VERSION);
      dataFile.println(",device=M5ATOMGPS,display=NONE,board=ESP32,brand=M5");
      dataFile.println("MAC,SSID,AuthMode,FirstSeen,Channel,RSSI,CurrentLatitude,CurrentLongitude,AltitudeMeters,AccuracyMeters,Type");
      dataFile.close();
      Serial.println("New file created: " + String(fileName));
    }
  } else {
    Serial.println("Using existing file: " + String(fileName));
  }
}

bool isMACSeen(const char* mac) {
  for (int i = 0; i < macArrayIndex; i++) {
    if (strcmp(macAddressArray[i], mac) == 0) {
      return true;
    }
  }
  return false;
}

void logData(const char* data) {
  File dataFile = SD.open(fileName, FILE_APPEND);
  if (dataFile && data) {
    dataFile.println(data);
    dataFile.close();
  } else {
    Serial.println("Error opening " + String(fileName));
    blinkLED(RED, 500);
  }
}

const char* getAuthType(uint8_t wifiAuth) {
  switch (wifiAuth) {
    case WIFI_AUTH_OPEN:
      return "[OPEN]";
    case WIFI_AUTH_WEP:
      return "[WEP]";
    case WIFI_AUTH_WPA_PSK:
      return "[WPA_PSK]";
    case WIFI_AUTH_WPA2_PSK:
      return "[WPA2_PSK]";
    case WIFI_AUTH_WPA_WPA2_PSK:
      return "[WPA_WPA2_PSK]";
    case WIFI_AUTH_WPA2_ENTERPRISE:
      return "[WPA2_ENTERPRISE]";
    case WIFI_AUTH_WPA3_PSK:
      return "[WPA3_PSK]";
    case WIFI_AUTH_WPA2_WPA3_PSK:
      return "[WPA2_WPA3_PSK]";
    case WIFI_AUTH_WAPI_PSK:
      return "[WAPI_PSK]";
    default:
      return "[UNKNOWN]";
  }
}

void parseConfigFile(File file) {
  char line[80];
  int lineIndex = 0;
  while (file.available()) {
    char c = file.read();
    if (c == '\n' || c == '\r') {
      line[lineIndex] = '\0';
      if (lineIndex > 0) {
        processConfigLine(line);
      }
      lineIndex = 0;
    } else {
      line[lineIndex++] = c;
    }
  }
  if (lineIndex > 0) {
    line[lineIndex] = '\0';
    processConfigLine(line);
  }
}

void processConfigLine(const char* line) {
  char key[50];
  char value[150];

  sscanf(line, "%49[^=]=%149[^\n]", key, value);

  if (strcmp(key, "speedBased") == 0) {
    speedBased = (strcmp(value, "true") == 0);
  } else if (strcmp(key, "scanDelay") == 0) {
    scanDelay = atoi(value);
  } else if (strcmp(key, "adaptiveScan") == 0) {
    adaptiveScan = (strcmp(value, "true") == 0);
  } else if (strcmp(key, "bleScanEnabled") == 0) {
    bleScanEnabled = (strcmp(value, "true") == 0);
  } else if (strcmp(key, "channels") == 0) {
    parseChannels(value);
  } else if (strcmp(key, "filePrefix") == 0) {
    strcpy(filePrefix,value);
  }
}

void parseChannels(const char* value) {
  int index = 0;
  char* token = strtok(const_cast<char*>(value), ",");
  while (token != NULL) {
    if (index < sizeof(channels) / sizeof(channels[0])) {
      channels[index++] = atoi(token);
    }
    token = strtok(NULL, ",");
  }

  while (index < sizeof(channels) / sizeof(channels[0])) {
    channels[index++] = 0;
  }
}