#include <Arduino.h>
#include <BH1750.h>
#include <Wire.h>
#include <WiFi.h>
#include <time.h>

// Run duration (3x60 = 3 m, 12x3600 = 12 h)
static constexpr uint32_t RUN_TIME = 12 * 3600;

// Device ID
static constexpr int DEVICE_ID = 1;

// Wi-Fi user & pass
const char* SSID = "";
const char* PASS = "";

// Timezone and NTP pool
const char* TZ_STRING = "EST5EDT,M3.2.0/2,M11.1.0/2";
const char* NTP_POOL = "pool.ntp.org";

// Telnet server
static const uint16_t TCP_PORT = 23;
WiFiServer server(TCP_PORT);
WiFiClient client;

// Sensor object
BH1750 sensor;

// Timing variables
time_t startTime;
time_t endTime;
uint32_t directSunSec = 0;
bool testComplete = false;

// Helpful function: Waits for Wi-Fi
void initTime() {
  WiFi.begin(SSID, PASS);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  Serial.print("Connected to ");
  Serial.println(WiFi.localIP());

  // Start SNTP and apply timezone
  configTzTime(TZ_STRING, NTP_POOL);

  // Wait until a valid time is fetched
  struct tm tmNow;
  Serial.print("Syncing time");
  while (!getLocalTime(&tmNow, 10000)) {
    Serial.print('.');
  }
  Serial.println();
  Serial.println("Time synced");
}

void setup() {
  // Ol' reliable 9600 baud
  Serial.begin(9600);

  // Wi-Fi setup in station mode
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  // BH1750 setup
  Wire.begin();
  if (!sensor.begin()) {
    Serial.println("BH1750 init failed. Check for loose wires!");

    // Halts everything if the wiring is bad
    while (true) {
      delay(1000);
    }
  }

  // Wi-Fi and NTP sync
  initTime();

  // Start record
  time(&startTime);
  endTime = startTime + RUN_TIME;

  // Start TCP server
  server.begin();
  Serial.printf("TCP server starting on port %u\n", TCP_PORT);
}

void loop() {
  // If Wi-Fi drops, re-attempt connection and resync
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connection lost. Attempting to reconnect...");
    if (client && client.connected()) {
      client.println("\nWi-Fi connection Lost. Attempting to reconnect...");
    }

    // Re-start Wi-Fi
    WiFi.disconnect();
    WiFi.begin(SSID, PASS);

    // Waits up to a minute for a reconnection
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 60000) {
      Serial.print('.');
      delay(500);
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println();
      Serial.print("Wi-Fi reconnected! IP address: ");
      Serial.println(WiFi.localIP());
      if (client && client.connected()) {
        client.println("\nWi-Fi reconnected! IP address: ");
        client.println(WiFi.localIP());
        client.print("\n");
      }

      // Resync time
      configTzTime(TZ_STRING, NTP_POOL);
      struct tm tmp;
      Serial.print("Re-synchronizing time");
      if (client && client.connected()) {
        client.println("\nRe-synchronizing time");
      }
      while (!getLocalTime(&tmp, 10000)) {
        Serial.print('.');
        delay(500);
      }
      Serial.println();
      Serial.println("Time re-synced");
      if (client && client.connected()) {
        client.println("Time re-synced via NTP");
      }
    } else {
      Serial.println();
      Serial.println("Failed to re-sync. Will re-try next loop");
      if (client && client.connected()) {
        client.println("Failed to re-sync. Will re-try next loop");
      }
    }
  }

  // If test isn't complete, get another sample
  if (!testComplete) {
    time_t currentTime;
    time(&currentTime);

    // Check if we're done
    if (currentTime >= endTime) {
      testComplete = true;
      uint32_t hours = directSunSec / 3600;
      uint32_t minutes = (directSunSec % 3600) / 60;
      uint32_t seconds = (directSunSec % 3600) % 60;

      char summary[128];
      snprintf(summary, sizeof(summary), "DEVICE_%d: Test Complete\n"
                                         "\nTotal direct sunlight: %u hours, %u minutes, and %u seconds\n",
                                         DEVICE_ID, hours, minutes, seconds);

      // Prints to Serial
      Serial.println();
      Serial.println(summary);

      // Prints to telnet client
      if (client && client.connected()) {
        client.print("\n");
        client.print(summary);
      }
      return;
    }

    // Get current time
    struct tm now;
    if (getLocalTime(&now)) {
      // Read light level
      float lux = sensor.readLightLevel();

      // Direct sunlight = 20000+ lux (adds 2 seconds to total)
      if (lux >= 20000.0) {
        directSunSec += 2;
      }

      // Print timestamp & reading
      char lineBuffer[64];
      snprintf(lineBuffer, sizeof(lineBuffer),
        "[%02u:%02u:%02u] lux: %.2f\n",
        now.tm_hour,
        now.tm_min,
        now.tm_sec,
        lux);

      // Prints to USB Serial
      Serial.print(lineBuffer);

      // If the TCP client is connected, sends the same line there too
      if (client && client.connected()) {
        client.print(lineBuffer);
      }
    } else {
      Serial.println("Could not sync. Skipping this reading");
      if (client && client.connected()) {
        client.println("Could not sync. Skipping this reading");
      }
    }
  }

  // Accepts a new TCP client if one isn't established
  if (!client || !client.connected()) {
    WiFiClient newClient = server.available();
    if (newClient) {
      client = newClient;
      Serial.println("TCP client connected");
      client.println("TCP client connected");
    }
  }

  // Read every 2 seconds
  delay(2000);
}