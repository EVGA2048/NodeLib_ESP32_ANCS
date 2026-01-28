/**
 * NodeLib ESP32 ANCS Example
 * 
 * This example shows how to use the NodeLib_ESP32_ANCS library
 * to receive iOS notifications via BLE ANCS protocol.
 * 
 * Features:
 * - Receives notifications from paired iOS device
 * - Supports Chinese/Emoji characters
 * - Automatic reconnection
 * - Callback-based notification handling
 */

 #include <NodeLib_ESP32_ANCS.h>

 // Create ANCS instance
 NodeLib_ESP32_ANCS ancs;
 
 // Notification callback - called when a complete notification is received
 void onNotificationReceived(uint32_t uid, const char* title, const char* message) {
     Serial.println("\n=== NEW NOTIFICATION ===");
     Serial.printf("UID: %u\n", uid);
     Serial.printf("Title: %s\n", title);
     Serial.printf("Message: %s\n", message);
     Serial.println("=====================\n");
     
     // You can add your own logic here, for example:
     // - Display on OLED screen
     // - Trigger vibration motor
     // - Send to another device via MQTT/HTTP
     // - Save to SD card
 }
 
 // Event callback - called when a notification event occurs (added/modified/removed)
 void onEventReceived(uint8_t eventId, uint32_t uid) {
     const char* eventName = "Unknown";
     switch(eventId) {
         case 0: eventName = "Added"; break;
         case 1: eventName = "Modified"; break;
         case 2: eventName = "Removed"; break;
     }
     Serial.printf(">> Event: %s (UID: %u)\n", eventName, uid);
 }
 
 void setup() {
     // Initialize Serial for debugging
     Serial.begin(115200);
     while (!Serial) {
         ; // Wait for serial port to connect
     }
     delay(1000);
     
     Serial.println("\n========================================");
     Serial.println("     NodeLib ESP32 ANCS Example");
     Serial.println("========================================\n");
     
     // Set up callbacks
     ancs.setNotificationCallback(onNotificationReceived);
     ancs.setEventCallback(onEventReceived);
     
     // Initialize ANCS
     // You can customize the device name that appears on your iPhone
     ancs.begin("ESP32-ANCS-Demo");
     
     Serial.println("\nSetup complete. Waiting for iOS device...");
     Serial.println("1. On your iPhone, go to Settings > Bluetooth");
     Serial.println("2. Find and connect to 'ESP32-ANCS-Demo'");
     Serial.println("3. When prompted, enter passkey: 123456");
     Serial.println("4. Notifications will appear here automatically\n");
 }
 
 void loop() {
     // Keep the ANCS library running
     ancs.loop();
     
     // You can add your own code here, but avoid blocking delays
     // Example: Check connection status periodically
     static unsigned long lastStatusCheck = 0;
     if (millis() - lastStatusCheck > 10000) { // Every 10 seconds
         lastStatusCheck = millis();
         
         if (ancs.isConnected()) {
             Serial.println("[STATUS] Connected to iOS device");
         } else {
             Serial.println("[STATUS] Waiting for connection...");
         }
         
         if (ancs.isRunning()) {
             Serial.println("[STATUS] ANCS is running and ready");
         }
     }
 }