#include <M5Atom.h>
#include <SD.h>
#include <SPI.h>
#include <TinyGPS++.h>
#include <WiFi.h>

const String BUILD = "1.6.4";
const String VERSION = "1.6";

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

// Quad boundaries, change to activate orange LED when outside it
float quadLatMin = -90.0;    // Minimum latitude boundary 
float quadLatMax = 90.0;     // Maximum latitude boundary 
float quadLonMin = -180.0;   // Minimum longitude boundary 
float quadLonMax = 180.0;    // Maximum longitude boundary 

// Scan & GPS
TinyGPSPlus gps;
char fileName[50];
const int maxMACs = 150;  // TESTING
char macAddressArray[maxMACs][20];
int macArrayIndex = 0;
int timePerChannel[14] = { 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 50, 50, 50 };  // change for your region
float lat;
float lon;
float altitude;
float accuracy;
int numSatellites;

// Speed-based scan vars, overrides scanDelay set from SD
double speed = -1;
static int stop = 500;           // 1s delay while stopped
static int slow = 250;           // 400ms delay < 15mph
static int fast = 100;           // 100ms delay > 15mph
static int uninitialized = 250;  // No GPS fix delay catch

// Configurable vars from SD
bool speedBased = false;
int scanDelay = 150;
bool adaptiveScan = true;
int channels[14] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };  // Jurisdictional, using US regs 1-11. Set up to 14.

void setup() {
  // Init connection & filesys
  Serial.begin(115200);
  delay(1000);
  Serial.println("Starting AtomWigler...");
  M5.begin(true, false, true);
  SPI.begin(23, 33, 19, -1);
  delay(1000);  // let system catch up

  while (!SD.begin(15, SPI, 40000000)) {
    Serial.println("SD Card initialization failed! Retrying...");
    blinkLED(RED, 500);
    delay(1000);
  }

  Serial.println("SD Card initialized.");

  // Read and parse config file
  if (SD.exists("/config.txt")) {
    File configFile = SD.open("/config.txt");
    if (configFile) {
      parseConfigFile(configFile);
      configFile.close();

      // Print parsed values
      Serial.println("Configuration values:");
      Serial.print("speedBased: ");
      Serial.println(speedBased ? "true" : "false");
      Serial.print("scanDelay: ");
      Serial.println(scanDelay);
      Serial.print("adaptiveScan: ");
      Serial.println(adaptiveScan ? "true" : "false");
      Serial.print("channels: ");
      for (int i = 0; i < sizeof(channels) / sizeof(channels[0]); i++) {
        if (channels[i] != 0) {  // Print only valid channels
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

  // Init WiFi
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  Serial.println("WiFi initialized.");

  // Init GPS
  Serial1.begin(9600, SERIAL_8N1, 22, -1);
  delay(1000);  // Allow time for GPS to initialize
  Serial.println("GPS Serial initialized.");
  waitForGPSFix();
  initializeFile();  // Have fix, write the file and begin scan
}


void loop() {
  static unsigned long lastBlinkTime = 0;
  const unsigned long blinkInterval = 2500;

  // Button blink toggle
  M5.update();
  if (M5.Btn.wasPressed()) {
    buttonLedState = !buttonLedState;
    delay(50);
  }

  // Scan while we have a fix
  while (Serial1.available() > 0) {
    gps.encode(Serial1.read());
  }

  if (gps.location.isValid()) {
    unsigned long currentMillis = millis();  // get the time here for accurate blinks
    if (currentMillis - lastBlinkTime >= blinkInterval && buttonLedState) {
      M5.dis.drawpix(0, GREEN);  // Flash green
      delay(60);
      M5.dis.clear();
      lastBlinkTime = currentMillis;
    }

    lat = gps.location.lat();
    lon = gps.location.lng();
    altitude = gps.altitude.meters();
    accuracy = gps.hdop.hdop();
    speed = gps.speed.mph();

    char utc[21];
    sprintf(utc, "%04d-%02d-%02d %02d:%02d:%02d", gps.date.year(), gps.date.month(), gps.date.day(), gps.time.hour(), gps.time.minute(), gps.time.second());

    if (isOutsideQuad(lat, lon)) {
      flashLEDOutsideQuad();
    }

    for (int i = 0; i < sizeof(channels) / sizeof(channels[0]); i++) {  // scan wifi
      int channel = channels[i];

      int numNetworks = WiFi.scanNetworks(false, true, false, timePerChannel[channel - 1], channel);
      for (int i = 0; i < numNetworks; i++) {
        char currentMAC[20];
        strcpy(currentMAC, WiFi.BSSIDstr(i).c_str());
        if (!isMACSeen(currentMAC)) {
          strcpy(macAddressArray[macArrayIndex++], currentMAC);
          if (macArrayIndex >= maxMACs) macArrayIndex = 0;
          char dataString[300];
          snprintf(dataString, sizeof(dataString), "%s,\"%s\",%s,%s,%d,%d,%.6f,%.6f,%.2f,%.2f,WIFI", currentMAC, WiFi.SSID(i).c_str(), getAuthType(WiFi.encryptionType(i)), utc, WiFi.channel(i), WiFi.RSSI(i), lat, lon, altitude, accuracy);
          logData(dataString);
        }
      }
      if (adaptiveScan) {
        updateTimePerChannel(channel, numNetworks);  // Adaptive scan timing
      }
    }
  } else {
    speed = -1;  // GPS lost, reset speed var
    blinkLED(PURPLE, 250);
  }

  if (speedBased) {
    delay(*getSpeed(speed));  // Speed based delay
  } else {
    delay(scanDelay);  // Static delay
  }
}

void updateTimePerChannel(int channel, int networksFound) {  // BETA feature, adjust as desired
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

// GPS
bool isOutsideQuad(float latitude, float longitude) {
  return (latitude < quadLatMin || latitude > quadLatMax || longitude < quadLonMin || longitude > quadLonMax);
}

void waitForGPSFix() {
  Serial.println("Waiting for GPS fix...");
  while (!gps.location.isValid()) {
   numSatellites = gps.satellites.value();
    if (Serial1.available() > 0) {
      gps.encode(Serial1.read());
    }    
    // Serial.println("Sats: " + String(numSatellites));
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

// LED
void flashLEDOutsideQuad() {
    M5.dis.drawpix(0, ORANGE);
    delay(200);
    M5.dis.clear();
    delay(200);
}

void blinkLEDFaster(int numSatellites) {
  unsigned long interval;
  if (numSatellites <= 1) {
    interval = 1000;  // Slow blink for 0 or 1 satellites
  } else {
    interval = max(50, 1000 / numSatellites);  // Blink interval decreases as the number of satellites increases
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

// Filesys
void initializeFile() {
  int fileNumber = 0;
  bool isNewFile = false;
  char fileDateStamp[16];
  sprintf(fileDateStamp, "%04d-%02d-%02d-", gps.date.year(), gps.date.month(), gps.date.day());
  do {
    snprintf(fileName, sizeof(fileName), "/AtomWigler-%s%d.csv", fileDateStamp, fileNumber);
    isNewFile = !SD.exists(fileName);
    fileNumber++;
  } while (!isNewFile);
  if (isNewFile) {
    File dataFile = SD.open(fileName, FILE_WRITE);
    if (dataFile) {
      dataFile.println("WigleWifi-1.4,appRelease=" + BUILD + ",model=AtomWigler,release=" + VERSION + ",device=M5ATOMGPS,display=NONE,board=ESP32,brand=M5");
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

// SD Config
void parseConfigFile(File file) {
  char line[80];
  int lineIndex = 0;
  while (file.available()) {
    char c = file.read();
    if (c == '\n' || c == '\r') {
      line[lineIndex] = '\0';  // Terminate the string
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
  char key[50];     // Increased buffer size for key
  char value[150];  // Increased buffer size for value

  sscanf(line, "%49[^=]=%149[^\n]", key, value);  // Ensure no buffer overflow

  if (strcmp(key, "speedBased") == 0) {
    speedBased = (strcmp(value, "true") == 0);
  } else if (strcmp(key, "scanDelay") == 0) {
    scanDelay = atoi(value);
  } else if (strcmp(key, "adaptiveScan") == 0) {
    adaptiveScan = (strcmp(value, "true") == 0);
  } else if (strcmp(key, "channels") == 0) {
    parseChannels(value);
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

  // Clear the rest of the channels array if fewer channels are defined
  while (index < sizeof(channels) / sizeof(channels[0])) {
    channels[index++] = 0;
  }
}
