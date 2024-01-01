#include "M5Atom.h"
#include <SPI.h>
#include "FS.h"
#include "SD.h"
#include <WiFi.h>
#include <TinyGPS++.h>

TinyGPSPlus gps;
String fileName;

const int maxMACs = 75;
String macAddressArray[maxMACs];
int macArrayIndex = 0;

// define colors for pixel display
int RGB_COLOR_WHITE = 0xffffff;
int RGB_COLOR_BLACK = 0x000000;
int RGB_COLOR_RED = 0xff0000;
int RGB_COLOR_GREEN = 0x00ff00;
int RGB_COLOR_BLUE = 0x0000ff;
int RGB_COLOR_PURPLE = 0x800080;
int RGB_COLOR_YELLOW = 0xffff00;
int RGB_COLOR_AQUA = 0x00ffff;
// define colors for conditions
int LED_GPSSYNC = RGB_COLOR_PURPLE;
int LED_GPSFIX = RGB_COLOR_GREEN;
int LED_SDCARDFAIL = RGB_COLOR_RED;
int LED_NEWNET = RGB_COLOR_BLUE;

// If you want to flash on new nets recorded, set this to true
// ** NOTE - this will cost you an extra 100msec pause in the data write!
bool ledFlashOnNewNet = false;

void setup() {
  Serial.begin(115200);
  Serial.println("Starting...");

  M5.begin(true, false, true);
  SPI.begin(23, 33, 19, -1);

  if (!SD.begin(-1, SPI, 40000000)) {
    Serial.println("SD Card initialization failed!");
    M5.dis.drawpix(0, LED_SDCARDFAIL);  // LED indicates SD card issue
    return;
  }
  Serial.println("SD Card initialized.");

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  Serial.println("WiFi initialized.");

  Serial1.begin(9600, SERIAL_8N1, 22, -1); // there is no TX pin on the GPS unit, it's not even wired
  Serial.println("GPS Serial initialized.");

  waitForGPSFix();

  initializeFile();
}

void waitForGPSFix() {
  Serial.println("Waiting for GPS fix...");
  while (!gps.location.isValid()) {
    while (Serial1.available() > 0) {
      gps.encode(Serial1.read());
    }
    M5.dis.drawpix(0, LED_GPSSYNC);  // LED while waiting for GPS fix
    delay(500);
    M5.dis.clear();  // Clear LED after waiting
    delay(500);
  }
  M5.dis.drawpix(0, LED_GPSFIX);  // LED indicates GPS fix
  Serial.println("GPS fix obtained.");
}

void initializeFile() {
  int fileNumber = 0;
  bool isNewFile = false;

  // create a date stamp for the filename
  char fileDateStamp[16];
  sprintf(fileDateStamp, "%04d-%02d-%02d-",
          gps.date.year(), gps.date.month(), gps.date.day());

  do {
    fileName = "/wifi-scans-" + String(fileDateStamp) + String(fileNumber) + ".csv";
    isNewFile = !SD.exists(fileName);
    fileNumber++;
  } while (!isNewFile);

  if (isNewFile) {
    File dataFile = SD.open(fileName, FILE_WRITE);
    if (dataFile) {
      dataFile.println("WigleWifi-1.4,appRelease=1.100000,model=Developer Kit,release=1.100000F+00,device=M5ATOM,display=NONE,board=ESP32,brand=M5");
      dataFile.println("MAC,SSID,AuthMode,FirstSeen,Channel,RSSI,CurrentLatitude,CurrentLongitude,AltitudeMeters,AccuracyMeters,Type");
      dataFile.close();
      Serial.println("New file created: " + fileName);
    }
  } else {
    Serial.println("Using existing file: " + fileName);
  }
}

void loop() {
  while (Serial1.available() > 0) {
    gps.encode(Serial1.read());
  }

  if (gps.location.isValid()) {
    float lat = gps.location.lat();
    float lon = gps.location.lng();
    float altitude = gps.altitude.meters();
    float accuracy = gps.hdop.hdop();

    char utc[20];
    sprintf(utc, "%04d-%02d-%02d %02d:%02d:%02d",
            gps.date.year(), gps.date.month(), gps.date.day(),
            gps.time.hour(), gps.time.minute(), gps.time.second());
    //scanNetworks(bool async, bool show_hidden, bool passive, uint32_t max_ms_per_chan)
    int numNetworks = WiFi.scanNetworks(false, true, false, 110);  //credit J.Hewitt
    for (int i = 0; i < numNetworks; i++) {
      String currentMAC = WiFi.BSSIDstr(i);
      if (!isMACSeen(currentMAC)) {
        macAddressArray[macArrayIndex++] = currentMAC;
        if (macArrayIndex >= maxMACs) macArrayIndex = 0;

        String ssid = "\"" + WiFi.SSID(i) + "\"";  //sanitize SSID
        String capabilities = getAuthType(WiFi.encryptionType(i));
        int channel = WiFi.channel(i);
        int rssi = WiFi.RSSI(i);

        String dataString = currentMAC + "," + ssid + "," + capabilities + "," + utc + "," + String(channel) + "," + String(rssi) + "," + String(lat, 6) + "," + String(lon, 6) + "," + String(altitude, 2) + "," + String(accuracy, 2) + ",WIFI";

        logData(dataString);
      }
    }
    
    // check for a button press and toggle the ledFlashOnNewNet boolean - if pressed for 1 second, toggle
    M5.update(); // get status update to read button
    
    if (M5.Btn.pressedFor(1000)) {  // if the button was down for 1 second
      ledFlashOnNewNet = !ledFlashOnNewNet;
      delay(20); // let that sync in...
      Serial.println("Detected button press... Flash_On_New_Net is now: " + ledFlashOnNewNet);
      Serial.println(M5.Btn.read());
    }
    
  } else {
    M5.dis.drawpix(0, LED_GPSSYNC);  // LED if waiting for GPS fix
    delay(500);
    M5.dis.clear();  // Clear LED after waiting
    delay(500);
  }
  delay(250);  // Short delay for loop iteration
}

bool isMACSeen(const String& mac) {
  for (int i = 0; i < macArrayIndex; i++) {
    if (macAddressArray[i] == mac) {
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

    if (ledFlashOnNewNet) {
      M5.dis.drawpix(0, LED_NEWNET); // LED for successful write
      delay(100);
 //      M5.dis.clear();
 // put the appropriate GPS color back in place
      if (gps.location.isValid()) {
        M5.dis.drawpix(0,LED_GPSFIX);
      } else {
        M5.dis.drawpix(0,LED_GPSSYNC);
      }
    }
    // Serial.println("Data written: " + data);
  } else {
    Serial.println("Error opening " + fileName);
    M5.dis.drawpix(0, LED_SDCARDFAIL);  // LED indicates SD card issue
    delay(500);
    M5.dis.clear();  // LED flash for file write error
    delay(500);
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
