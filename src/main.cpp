#include <Arduino.h>
#include <BH1750.h>
#include <Wire.h>

BH1750 light;

void setup() {
    Serial.begin(9600);
    Wire.begin();
    light.begin();
}

void loop() {
    float lux = light.readLightLevel();
    Serial.print("Lx: ");
    Serial.println(lux);
    if (lux > 1000) {
        Serial.println("High lux!");
    }
    delay(2000);
}