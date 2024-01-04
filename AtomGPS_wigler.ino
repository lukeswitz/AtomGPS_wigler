//#define M5
#ifdef M5
#include "M5Atom.h"
#else
#include <Adafruit_NeoPixel.h>
#endif

#include <SPI.h>
#include "FS.h"
#include "SD.h"
#include <WiFi.h>
#include <TinyGPS++.h>

TinyGPSPlus gps;
String fileName;

#ifdef M5
#else
Adafruit_NeoPixel led = Adafruit_NeoPixel(1, 27, NEO_GRB + NEO_KHZ800);
#endif



const int maxMACs = 75;
String macAddressArray[maxMACs];
int macArrayIndex = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("Starting...");

#ifdef M5
  M5.begin(true, false, true);
#else
  led.begin();
  led.setPixelColor(0, 0x000000);
  led.show();
#endif
  SPI.begin(23, 33, 19, -1);

  unsigned long startMillis = millis();
  const unsigned long blinkInterval = 500;
  bool ledState = false;

  if (!SD.begin(-1, SPI, 40000000)) {
    Serial.println("SD Card initialization failed!");
    while (millis() - startMillis < 5000) {  // Continue blinking for 5 seconds
      if (millis() - startMillis > blinkInterval) {
        startMillis = millis();
        ledState = !ledState;
#ifdef M5
        M5.dis.drawpix(0, ledState ? 0xff0000 : 0x000000);  // Toggle red LED
#else
        led.setPixelColor(0, ledState ? 0xff0000 : 0x000000); // Flash Red if TF card is missing
        led.show();
#endif
      }
    }
    return;
  }

#ifdef M5
  M5.dis.clear();  // Clear LED after blinking
#else
  led.setPixelColor(0, 0x000000);  // Clear LED after blinking
  led.show();
#endif
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

void waitForGPSFix() {
  unsigned long lastBlink = 0;
  const unsigned long blinkInterval = 300;  // Time interval for LED blinking
  bool ledState = false;

  Serial.println("Waiting for GPS fix...");
  while (!gps.location.isValid()) {
    if (Serial1.available() > 0) {
      gps.encode(Serial1.read());
    }

    // Non-blocking LED blink
    if (millis() - lastBlink >= blinkInterval) {
      lastBlink = millis();
      ledState = !ledState;
#ifdef M5
      M5.dis.drawpix(0, ledState ? 0x800080 : 0x000000);  // Toggle purple LED
#else
      led.setPixelColor(0, ledState ? 0x800080 : 0x000000);
      led.show();
#endif
    }
  }

  // Green LED indicates GPS fix
#ifdef M5
  M5.dis.drawpix(0, 0x00ff00);
  delay(200);  // Short delay to show green LED
  M5.dis.clear();
#else
  led.setPixelColor(0, 0x00ff00);
  led.show();
  delay(200);  // Short delay to show green LED
  led.setPixelColor(0, 0x000000);
  led.show();
#endif
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
      dataFile.println("WigleWifi-1.4,appRelease=1.300000,model=GPS Kit,release=1.100000F+00,device=M5ATOM,display=NONE,board=ESP32,brand=M5");
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
#ifdef M5
    M5.dis.drawpix(0, 0x00ff00);
    delay(180);  // scan delay
    M5.dis.clear();
#else
    led.setPixelColor(0, 0x00ff00);
    led.show();
    delay(180);  // scan delay
    led.setPixelColor(0, 0x000000);
    led.show();
#endif

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
    int savedNetworks = 0;
    for (int i = 0; i < numNetworks; i++) {
      String currentMAC = WiFi.BSSIDstr(i);
      if (isMACSeen(currentMAC)) {
        continue;
      }
      macAddressArray[macArrayIndex++] = currentMAC;
      if (macArrayIndex >= maxMACs) macArrayIndex = 0;

      String ssid = "\"" + WiFi.SSID(i) + "\"";  //sanitize SSID
      String capabilities = getAuthType(WiFi.encryptionType(i));
      int channel = WiFi.channel(i);
      int rssi = WiFi.RSSI(i);

      String dataString = currentMAC + "," + ssid + "," + capabilities + "," + utc + "," + String(channel) + "," + String(rssi) + "," + String(lat, 6) + "," + String(lon, 6) + "," + String(altitude, 2) + "," + String(accuracy, 2) + ",WIFI";

      savedNetworks += logData(dataString);
    }
    if (savedNetworks > 0) {
#ifdef M5
      M5.dis.drawpix(0, 0x0000ff); // Blue LED for successful write
      delay(150);
      M5.dis.clear();
#else
      led.setPixelColor(0, 0x0000ff);
      led.show();
      delay(150);
      led.setPixelColor(0, 0x000000);
      led.show();
#endif
    }
  } else {
#ifdef M5
    M5.dis.drawpix(0, 0x800080);  // Purple LED if waiting for GPS fix
    delay(150);
    M5.dis.clear();  // Clear LED after waiting
#else
    led.setPixelColor(0, 0x800080);
    led.show();
    delay(150);
    led.setPixelColor(0, 0x000000);
    led.show();
#endif
    delay(150);
  }
}

bool isMACSeen(const String& mac) {
  for (int i = 0; i < macArrayIndex; i++) {
    if (macAddressArray[i] == mac) {
      return true;
    }
  }
  return false;
}

int logData(const String& data) {
  File dataFile = SD.open(fileName, FILE_APPEND);
  if (dataFile) {
    dataFile.println(data);
    dataFile.close();
    // Serial.println("Data written: " + data);
    return 1;
  } else {
    Serial.println("Error opening " + fileName);
#ifdef M5
    M5.dis.drawpix(0, 0xff0000);
    delay(500);
    M5.dis.clear();  // Red flash for file write error
#else
    led.setPixelColor(0, 0xff0000);
    led.show();
    delay(150);
    led.setPixelColor(0, 0x000000);
    led.show();
#endif
    delay(500);
    return 0;
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
