#include <M5Atom.h>
#include <SD.h>
#include <SPI.h>
#include <TinyGPS++.h>
#include <WiFi.h>

// LED
bool ledState = false;
bool buttonLedState = true;

#define RED 0xff0000
#define GREEN 0x00ff00
#define BLUE 0x0000ff
#define YELLOW 0xffff00
#define PURPLE 0x800080
#define CYAN 0x00ffff
#define WHITE 0xffffff
#define OFF 0x000000

// GPS and Filesys
TinyGPSPlus gps;
char fileName[50];
const int maxMACs = 150;  // TESTING: buffer size
char macAddressArray[maxMACs][20];
int macArrayIndex = 0;

// Network Scanning
const int popularChannels[] = { 1, 6, 11 };
const int standardChannels[] = { 2, 3, 4, 5, 7, 8, 9, 10 };
const int rareChannels[] = { 12, 13, 14 };  // Depending on region
int timePerChannel[14] = { 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 50, 50, 50 }; // min 50 max 500ms

void setup() {
  Serial.begin(115200);
  Serial.println("Starting...");
  M5.begin(true, false, true);
  SPI.begin(23, 33, 19, -1);              // investigate the -1 assignment and esp32 boards
  while (!SD.begin(-1, SPI, 40000000)) {  // params throw a bunch of gpio warnings, TODO assign ss
    Serial.println("SD Card initialization failed! Retrying...");
    blinkLED(RED, 500);  // will hang here until SD is readable
    delay(1000);
  }
  Serial.println("SD Card initialized.");

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  Serial.println("WiFi initialized.");

  Serial1.begin(9600, SERIAL_8N1, 22, -1);
  Serial.println("GPS Serial initialized.");
  waitForGPSFix();
  initializeFile();
}

void loop() {
  M5.update();
  if (M5.Btn.wasPressed()) {
    buttonLedState = !buttonLedState;
    delay(50);
  }

  while (Serial1.available() > 0) {
    gps.encode(Serial1.read());
  }

  if (gps.location.isValid()) {
    // Block for 80ms if enabled
    if (buttonLedState == true) {
      M5.dis.drawpix(0, GREEN);  // Flash green
      delay(80);
      M5.dis.clear();
    }

    float lat = gps.location.lat();
    float lon = gps.location.lng();
    float altitude = gps.altitude.meters();
    float accuracy = gps.hdop.hdop();
    char utc[21];
    sprintf(utc, "%04d-%02d-%02d %02d:%02d:%02d", gps.date.year(), gps.date.month(), gps.date.day(), gps.time.hour(), gps.time.minute(), gps.time.second());
    // scan hidden, adaptive channel dwell times
    for (int channel = 1; channel <= 14; channel++) {
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
      updateTimePerChannel(channel, numNetworks); // comment this out to use the static settings above
    }
  } else {
    blinkLED(PURPLE, 250);
  }
  delay(75);
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

void waitForGPSFix() {
  Serial.println("Waiting for GPS fix...");
  while (!gps.location.isValid()) {
    if (Serial1.available() > 0) {
      gps.encode(Serial1.read());
    }
    blinkLED(PURPLE, 250);
  }
  M5.dis.clear();
  Serial.println("GPS fix obtained.");
}

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
      dataFile.println("WigleWifi-1.4,appRelease=1.300000,model=GPS Kit,release=1.100000F+00,device=M5ATOM,display=NONE,board=ESP32,brand=M5");
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
      return "[UNDEFINED]";
  }
}


bool findInArray(int value, const int* array, int size) {
  for (int i = 0; i < size; i++) {
    if (array[i] == value) return true;
  }
  return false;
}

void updateTimePerChannel(int channel, int networksFound) { // BETA feature
  const int FEW_NETWORKS_THRESHOLD = 1;
  const int MANY_NETWORKS_THRESHOLD = 5;
  const int TIME_INCREMENT = 50;
  const int MAX_TIME = 300;
  const int MIN_TIME = 50;

  // Adjust time based on the number of networks found
  if (networksFound >= MANY_NETWORKS_THRESHOLD) {
    timePerChannel[channel - 1] = min(timePerChannel[channel - 1] + TIME_INCREMENT, MAX_TIME);
  } else if (networksFound <= FEW_NETWORKS_THRESHOLD) {
    timePerChannel[channel - 1] = max(timePerChannel[channel - 1] - TIME_INCREMENT, MIN_TIME);
  }
}
