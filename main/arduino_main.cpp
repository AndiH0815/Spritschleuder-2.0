// Arduino Sketch für Pumpensteuerung
#include <Arduino.h>

#define MAX_PUMPS 8

const int pumpPins[MAX_PUMPS] = {2, 3, 4, 5, 6, 7, 8, 9};
unsigned long pumpStops[MAX_PUMPS] = {};

void setup() {
    Serial.begin(115200);
    
    for (int i = 0; i < MAX_PUMPS; i++) {
        pinMode(pumpPins[i], OUTPUT);
        digitalWrite(pumpPins[i], LOW);
    }
}

void loop() {
    while (Serial.available() >= 5) {
        // Protokoll: 0xAA [PUMP_ID] [DURATION_HI] [DURATION_LO] 0x55
        if (Serial.read() == 0xAA) {
            uint8_t pumpId = Serial.read();
            uint8_t durationHi = Serial.read();
            uint8_t durationLo = Serial.read();
            uint8_t endByte = Serial.read();
            
            if (endByte == 0x55 && pumpId < MAX_PUMPS) {
                uint16_t duration = (durationHi << 8) | durationLo;
                
                if (duration == 0) {
                    digitalWrite(pumpPins[pumpId], LOW);
                    pumpStops[pumpId] = 0;
                } else {
                    digitalWrite(pumpPins[pumpId], HIGH);
                    pumpStops[pumpId] = millis() + duration;
                }
                
                // Bestätigung senden
                Serial.write(0xBB);
                Serial.write(pumpId);
                Serial.write(0x55);
            }
        }
    }
    const unsigned long now = millis();
    for (int pump = 0; pump < MAX_PUMPS; ++pump) {
        if (pumpStops[pump] && static_cast<long>(now - pumpStops[pump]) >= 0) {
            digitalWrite(pumpPins[pump], LOW); pumpStops[pump] = 0;
        }
    }
}
