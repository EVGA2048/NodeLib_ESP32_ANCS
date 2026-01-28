/**
 * NodeLib ESP32 ANCS Example
 * 
 * HOW TO USE:
 * 1. Install the library in your Arduino libraries folder.
 * 2. Restart Arduino IDE.
 * 3. Go to File > Examples > NodeLib_ESP32_ANCS > BasicUsage
 */

// Use angle brackets <> when the library is installed in the libraries folder
#include <NodeLib_ESP32_ANCS.h>

// Create the NodeLib ESP32 ANCS Client instance
NodeLib_ESP32_ANCS ancs;

// Callback function: This is called whenever a valid notification arrives
void onNotification(int eventId, uint32_t uid, const char* title, const char* message) {
    Serial.printf(">> [MSG] UID: %d\n", uid);
    Serial.printf("   [Title]: %s\n", title);
    Serial.printf("   [Body]:  %s\n", message);
    
    // You can add your own logic here (e.g., display on OLED, vibrate motor)
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println(">> [BOOT] NodeLib ESP32 ANCS Library Mode");

    // Initialize ANCS with Device Name and Callback
    // Device name will appear as "NodeLib-Link" on your phone
    ancs.begin("NodeLib-Link");
    ancs.setCallback(onNotification);
}

void loop() {
    // Keep the internal state machine running
    ancs.loop();
}