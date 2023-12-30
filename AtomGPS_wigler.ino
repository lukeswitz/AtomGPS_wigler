#include <Adafruit_NeoPixel.h>
#include <SPI.h>
#include "FS.h"
#include "SD.h"
#include <WiFi.h>
#include <TinyGPS++.h>

TinyGPSPlus gps;
String fileName;

Adafruit_NeoPixel led = Adafruit_NeoPixel(1, 27, NEO_GRB + NEO_KHZ800);

const int maxMACs = 500;
String macAddressArray[maxMACs];
int macArrayIndex = 0;

void setup() {
  // Initialize Serial for debugging
  Serial.begin(115200);
  Serial.println("Starting...");

  led.begin();
  led.setPixelColor(0, 0x000000);
  led.show();
  
  SPI.begin(23, 33, 19, -1); // Adjust for your SPI pins

  // Check if SD Card is present
  if (!SD.begin(-1, SPI, 40000000)) {
    Serial.println("SD Card initialization failed!");
    led.setPixelColor(0, 0xff0000); // Flash Red if TF card is missing
    led.show();
    delay(1000);
    return;
  }
  Serial.println("SD Card initialized.");

  // Initialize WiFi
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  Serial.println("WiFi initialized.");

  // Initialize GPS
  Serial1.begin(9600, SERIAL_8N1, 22, -1); // GPS Serial
  Serial.println("GPS Serial initialized.");

  // Find a new file name and create CSV header if the file is new
  int fileNumber = 0;
  bool isNewFile = false;
  do {
    fileName = "/wifi-scans" + String(fileNumber) + ".csv"; // Ensure path starts with '/'
    isNewFile = !SD.exists(fileName);
    fileNumber++;
  } while (!isNewFile);
  
  if (isNewFile) {
    File dataFile = SD.open(fileName, FILE_WRITE);
    if (dataFile) {
      dataFile.println("MAC,SSID,RSSI,Lat,Lon,UTC");
      dataFile.close();
      Serial.println("New file created: " + fileName);
    }
  } else {
    Serial.println("Using existing file: " + fileName);
  }

  // Clear the MAC address array
  for (int i = 0; i < maxMACs; i++) {
    macAddressArray[i] = "";
  }
}

void loop() {
//  led.setPixelColor(0, 0xff00ff); // Purple
//  led.show();
//  delay(200);
  
  while (Serial1.available() > 0) {
    if (gps.encode(Serial1.read())) {
      if (gps.location.isUpdated()) {
        led.setPixelColor(0, 0x00ff00); //  Pulse Green when GPS is locked
        led.show();
        Serial.println("GPS Location Updated.");
      } else {
        led.setPixelColor(0, 0x000000); // Clear LED if GPS not locked
        led.show();
        Serial.println("Waiting for GPS lock...");
        continue;
      }

      float lat = gps.location.lat();
      float lon = gps.location.lng();
      
      // Format the time as HH:MM:SS
      char utc[11];
      sprintf(utc, "%02d:%02d:%02d", gps.time.hour(), gps.time.minute(), gps.time.second());

      Serial.printf("Lat: %f, Lon: %f, UTC: %s\n", lat, lon, utc);

      int numNetworks = WiFi.scanNetworks();
      for (int i = 0; i < numNetworks; i++) {
        String currentMAC = WiFi.BSSIDstr(i);
        
        // Check if MAC is in the array
        bool macFound = false;
        for (int j = 0; j < maxMACs; j++) {
          if (macAddressArray[j] == currentMAC) {
            macFound = true;
            break;
          }
        }

        // Skip if MAC is found
        if (macFound) {
          continue;
        }

        // Add MAC to the top of the array and shift others down
        for (int j = maxMACs - 1; j > 0; j--) {
          macAddressArray[j] = macAddressArray[j - 1];
        }
        macAddressArray[0] = currentMAC;

        // Record data to SD card
        String dataString = currentMAC + "," + WiFi.SSID(i) + "," +
                            String(WiFi.RSSI(i)) + "," + String(lat, 6) + "," + 
                            String(lon, 6) + "," + utc;

        File dataFile = SD.open(fileName, FILE_APPEND);
        if (dataFile) {
          dataFile.println(dataString);
          dataFile.close();
          led.setPixelColor(0, 0x0000ff); // Flash Blue for each new Wi-Fi network
          led.show();
          delay(200);
          led.setPixelColor(0, 0x000000); // Clear the LED after flashing
          led.show();
          Serial.println("Data written: " + dataString);
        } else {
          Serial.println("Error opening " + fileName);
        }
      }
    }
  }
}
