#include <Arduino.h>
#include <BH1750.h>
#include <Wire.h>
#include <WiFi.h>
#include <time.h>

// WiFi user & pass
const char* SSID = "";
const char* PASS = "";

// Timezone and NTP pool
const char* TZ_STRING = "EST5EDT,M3.2.0/2,M11.1.0/2";
const char* NTP_POOL = "pool.ntp.org";

// Sensor object
BH1750 sensor;

// Helpful function: Waits for WiFi
void initTime() {
    WiFi.begin(SSID, PASS);
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(500);
    }
    Serial.println("Connected");

    // Start SNTP and apply timezone
    configTzTime(TZ_STRING, NTP_POOL);

    // Wait until a valid time is fetched
    struct tm tmNow;
    Serial.print("Syncing time");
    while (!getLocalTime(&tmNow, 10000)) {
        Serial.print('.');
    }
    Serial.printf("Time synced");
}

void setup() {
    Serial.begin(9600);

    // WiFi setup
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    // BH1750 setup
    Wire.begin();
    sensor.begin();
    initTime();
}

void loop() {

    struct tm now;
    if (getLocalTime(&now)) {
         // Read light level
        float lux = sensor.readLightLevel();

        // Print timestamp & reading
        Serial.printf("[%02u:%02u:%02u] lux: %.2f\n", now.tm_hour, now.tm_min, now.tm_sec, lux);

        if (lux > 20000) {
            Serial.println("Adequate Light Detected!");
        }
    } else {
        Serial.println("Time could not sync");
    }

    // Read every 2 seconds
    delay(2000);
}