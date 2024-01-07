#include <M5Atom.h>
#include <SD.h>
#include <SPI.h>
#include <TinyGPS++.h>
#include <WiFi.h>

#define RED 0xff0000
#define GREEN 0x00ff00
#define BLUE 0x0000ff
#define YELLOW 0xffff00
#define PURPLE 0x800080
#define CYAN 0x00ffff
#define WHITE 0xffffff
#define BLACK 0x000000


TinyGPSPlus gps;
char fileName[50];
const int maxMACs = 150;  // buffer size in testing phase
char macAddressArray[maxMACs][18];
int macArrayIndex = 0;


void setup() {
  Serial.begin(115200);
  Serial.println("Starting...");
  M5.begin(true, false, true);
  SPI.begin(23, 33, 19, -1);  // investigate the -1 assignment

  unsigned long startMillis = millis();
  const unsigned long blinkInterval = 500;
  bool ledState = false;

  if (!SD.begin(-1, SPI, 40000000)) {
    Serial.println("SD Card initialization failed!");
    blinkLED(RED, 500, 5000);
    return;
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
  while (Serial1.available() > 0) {
    gps.encode(Serial1.read());
  }
  if (gps.location.isValid()) {
    blinkLED(GREEN, 180, 180);  // quick green flash
    float lat = gps.location.lat();
    float lon = gps.location.lng();
    float altitude = gps.altitude.meters();
    float accuracy = gps.hdop.hdop();
    char utc[20];
    sprintf(utc, "%04d-%02d-%02d %02d:%02d:%02d", gps.date.year(), gps.date.month(), gps.date.day(), gps.time.hour(), gps.time.minute(), gps.time.second());
    int numNetworks = WiFi.scanNetworks(false, true, false, 60);
    for (int i = 0; i < numNetworks; i++) {
      char currentMAC[18];
      strcpy(currentMAC, WiFi.BSSIDstr(i).c_str());
      if (!isMACSeen(currentMAC)) {
        strcpy(macAddressArray[macArrayIndex++], currentMAC);
        if (macArrayIndex >= maxMACs) macArrayIndex = 0;
        char dataString[200];
        snprintf(dataString, sizeof(dataString), "%s,\"%s\",%s,%s,%d,%d,%.6f,%.6f,%.2f,%.2f,WIFI", currentMAC, WiFi.SSID(i).c_str(), getAuthType(WiFi.encryptionType(i)), utc, WiFi.channel(i), WiFi.RSSI(i), WiFi.RSSI(i), lat, lon, altitude, accuracy);
        logData(dataString);
        macArrayIndex = (macArrayIndex + 1) % maxMACs;
      }
    }
  } else {
    blinkLED(PURPLE, 500, 1000);
  }
}

void blinkLED(uint32_t color, unsigned long interval, unsigned long duration) {
  unsigned long startMS = millis();
  bool ledState = false;
  while (millis() - startMS < duration) {
    if (millis() - startMS >= interval) {
      ledState = !ledState;
      M5.dis.drawpix(0, ledState ? color : 0x000000);
      startMS = millis();
    }
  }
  M5.dis.clear();
}

void waitForGPSFix() {
  unsigned long lastBlink = 0;
  const unsigned long blinkInterval = 300;  // Time interval for LED blinking
  bool ledState = false;

  Serial.println("Waiting for GPS fix...");
  while (!gps.location.isValid()) {
    if (Serial1.available() > 0) {
      gps.encode(Serial1.read());
    }
    blinkLED(PURPLE, 300, 300);
  }
  blinkLED(GREEN, 200, 200);
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

void logData(const String& data) {
  File dataFile = SD.open(fileName, FILE_APPEND);
  if (dataFile) {
    dataFile.println(data);
    dataFile.close();
  } else {
    Serial.println("Error opening " + String(fileName));
    blinkLED(RED, 500, 2000);
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