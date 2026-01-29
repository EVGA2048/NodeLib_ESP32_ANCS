/**
 * NodeLib ESP32 - ANCS & AMS Client
 * 
 * Features:
 * - Reads Apple Notifications (ANCS)
 * - Reads Media State (AMS)
 * - Robust Service Discovery (Handles UUID variations)
 */

#include <NodeLib_ESP32_ANCS.h>

NodeLib_ESP32_ANCS ancs;

void onNotification(int eventId, uint32_t uid, const char* appId, const char* title, const char* message) {
    Serial.println("\n>>> [NOTIFICATION] <<<");
    Serial.printf("App: %s\n", appId);
    Serial.printf("Title: %s\n", title);
    Serial.printf("Message: %s\n", message);
    Serial.println("------------------------\n");
}

void onMedia(const char* title, const char* artist, const char* album, bool isPlaying) {
    Serial.printf(">>> [MEDIA] %s | %s | %s (%s)\n", title, artist, album, isPlaying ? "PLAYING" : "PAUSED");
}

void setup() {
    Serial.begin(115200);
    delay(1000); 
    Serial.println("\n\n=== ESP32 ANCS/AMS CLIENT STARTED ===");

    ancs.begin("ESP32-S3-Gateway");
    ancs.setCallback(onNotification);
    ancs.setMediaCallback(onMedia);
}

void loop() {
    ancs.loop();
};