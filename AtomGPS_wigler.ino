//use M5 lib or use Adafruit NeoPixel
//#define M5
//scan wifi channels 1, 6, 11, 14 and ble or just wifi (code taken from j.hewitt)
//#define TYPEB

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

#ifdef TYPEB
//copied from j.hewitt rev3

#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

BLEScan* pBLEScan;

#define mac_history_len 256

struct mac_addr {
  unsigned char bytes[6];
};

struct mac_addr mac_history[mac_history_len];
unsigned int mac_history_cursor = 0;

void save_mac(unsigned char* mac) {
  if (mac_history_cursor >= mac_history_len) {
    mac_history_cursor = 0;
  }
  struct mac_addr tmp;
  for (int x = 0; x < 6 ; x++) {
    tmp.bytes[x] = mac[x];
  }

  mac_history[mac_history_cursor] = tmp;
  mac_history_cursor++;
}

boolean seen_mac(unsigned char* mac) {

  struct mac_addr tmp;
  for (int x = 0; x < 6 ; x++) {
    tmp.bytes[x] = mac[x];
  }

  for (int x = 0; x < mac_history_len; x++) {
    if (mac_cmp(tmp, mac_history[x])) {
      return true;
    }
  }
  return false;
}

void print_mac(struct mac_addr mac) {
  for (int x = 0; x < 6 ; x++) {
    Serial.print(mac.bytes[x], HEX);
    Serial.print(":");
  }
}

boolean mac_cmp(struct mac_addr addr1, struct mac_addr addr2) {
  for (int y = 0; y < 6 ; y++) {
    if (addr1.bytes[y] != addr2.bytes[y]) {
      return false;
    }
  }
  return true;
}

void clear_mac_history() {
  struct mac_addr tmp;
  for (int x = 0; x < 6 ; x++) {
    tmp.bytes[x] = 0;
  }

  for (int x = 0; x < mac_history_len; x++) {
    mac_history[x] = tmp;
  }

  mac_history_cursor = 0;
}
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
      unsigned char mac_bytes[6];
      int values[6];

      if (6 == sscanf(advertisedDevice.getAddress().toString().c_str(), "%x:%x:%x:%x:%x:%x%*c", &values[0], &values[1], &values[2], &values[3], &values[4], &values[5])) {
        for (int i = 0; i < 6; ++i ) {
          mac_bytes[i] = (unsigned char) values[i];
        }

        if (!seen_mac(mac_bytes)) {
          save_mac(mac_bytes);

          //Serial.printf("Advertised Device: %s \n", advertisedDevice.toString().c_str());
          char utc[20];
          sprintf(utc, "%04d-%02d-%02d %02d:%02d:%02d",
                  gps.date.year(), gps.date.month(), gps.date.day(),
                  gps.time.hour(), gps.time.minute(), gps.time.second());
          float lat = gps.location.lat();
          float lon = gps.location.lng();
          float altitude = gps.altitude.meters();
          float accuracy = gps.hdop.hdop();

          String out = advertisedDevice.getAddress().toString().c_str() + String(",") + advertisedDevice.getName().c_str() + String(",") + "[BLE]," + utc + String(",0,") + String(advertisedDevice.getRSSI()) + String(",") + String(lat, 6) + String(",") + String(lon, 6) + String(",") + String(altitude, 2) + String(",") + String(accuracy, 2) + String(",BLE");
          logData(out);
        }
      }
    }
};

boolean sd_lock = false; //Set to true when writting to sd card

void await_sd() {
  while (sd_lock) {
    Serial.println("await");
    delay(1);
  }
}
#endif

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

#ifdef TYPEB
  Serial.println("Setting up Bluetooth scanning");
  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan(); //create new scan
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(false); //active scan uses more power, but get results faster
  pBLEScan->setInterval(50);
  pBLEScan->setWindow(40);  // less or equal setInterval value

  Serial.println("M5 Atom GPS Wigler (Type B) started!");
#else
  Serial.println("M5 Atom GPS Wigler (Type A) started!");
#endif
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

#ifdef TYBEB
int wifi_scan_channel = 1; //The channel to scan (increments automatically)
#endif

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

#ifdef TYPEB
    BLEScanResults foundDevices = pBLEScan->start(2.5, false);
    Serial.print("Devices found: ");
    Serial.println(mac_history_cursor);
    Serial.println("Scan done!");
    pBLEScan->clearResults();   // delete results fromBLEScan buffer to release memory
#endif

    int savedNetworks = 0;
#ifdef TYPEB
    for (int y = 0; y < 4; y++) {
      switch (wifi_scan_channel) {
        case 1:
          wifi_scan_channel = 6;
          break;
        case 6:
          wifi_scan_channel = 11;
          break;
        case 11:
          wifi_scan_channel = 14;
          break;
        default:
          wifi_scan_channel = 1;
      }
#endif

      //scanNetworks(bool async, bool show_hidden, bool passive, uint32_t max_ms_per_chan)
#ifdef TYPEB
      int numNetworks = WiFi.scanNetworks(false, true, false, 110, wifi_scan_channel);  //credit J.Hewitt
#else
      int numNetworks = WiFi.scanNetworks(false, true, false, 110);  //credit J.Hewitt
#endif
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
#ifdef TYPEB
    }
#endif
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
#ifdef TYPEB
  await_sd();
  sd_lock = true;
  int i = logData1(data);
  sd_lock = false;
  return i;
}

int logData1(const String& data) {
#endif
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
