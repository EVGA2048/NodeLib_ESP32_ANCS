#ifndef NODELIB_ESP32_ANCS_H
#define NODELIB_ESP32_ANCS_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <BLEClient.h>
#include "esp_mac.h"
#include <string>

// Callback function types
typedef void (*ANCSNotificationCallback)(uint32_t uid, const char* title, const char* message);
typedef void (*ANCSEventCallback)(uint8_t eventId, uint32_t uid);

class NodeLib_ESP32_ANCS {
public:
    NodeLib_ESP32_ANCS();
    ~NodeLib_ESP32_ANCS();
    
    // Core functions
    void begin(const char* deviceName = "ESP32-ANCS");
    void loop();
    
    // Callback setters
    void setNotificationCallback(ANCSNotificationCallback cb);
    void setEventCallback(ANCSEventCallback cb);
    
    // Utility functions
    bool isConnected();
    bool isRunning();
    String getPasskey();  // 新增：获取配对码
    
private:
    // BLE Objects
    BLEServer* pServer = nullptr;
    BLEClient* pClient = nullptr;
    BLEAddress* pRemoteAddress = nullptr;
    BLERemoteCharacteristic* pRemoteNotif = nullptr;
    BLERemoteCharacteristic* pRemoteCP = nullptr;
    BLERemoteCharacteristic* pRemoteData = nullptr;
    
    // State variables
    bool deviceConnected = false;
    bool isPaired = false;
    String passkey = "";  // 存储配对码
    
    // State machine
    enum AppState {
        STATE_ADVERTISING,
        STATE_CONNECTED_WAITING, 
        STATE_CONNECTING_CLIENT,
        STATE_DISCOVERING_SERVICES,
        STATE_SUBSCRIBING,
        STATE_RUNNING
    };
    AppState currentState = STATE_ADVERTISING;
    unsigned long stateStartTime = 0;
    
    // Parser state
    enum ParseState {
        ST_WAIT_CMD,
        ST_CHECK_UID,
        ST_ATTR_ID,
        ST_LEN1,
        ST_LEN2,
        ST_DATA
    };
    ParseState pState = ST_WAIT_CMD;
    
    // Parser variables
    int uidBytesRead = 0;
    uint32_t parsedUID = 0;
    uint16_t attrLen = 0;
    uint16_t attrBytesRead = 0;
    uint8_t currentAttrId = 0;
    std::string currentBuffer = "";
    std::string tempTitle = "";
    std::string tempMessage = "";
    
    // Request queue
    volatile bool pendingRequest = false;
    volatile uint32_t targetUID = 0;
    volatile uint32_t activeRequestUID = 0;
    
    // Callbacks
    ANCSNotificationCallback notificationCallback = nullptr;
    ANCSEventCallback eventCallback = nullptr;
    
    // Private methods
    void setState(AppState newState);
    void addSolicitation(BLEAdvertisementData &adv);
    void performRequest(uint32_t uid);
    void generatePasskey();  // 生成随机配对码
    
    // Internal handlers
    void handleDataReceived(uint8_t* pData, size_t length);
    void handleNotificationReceived(uint8_t* pData, size_t length);
    
    // Static callbacks (trampoline functions)
    static void staticOnDataReceived(BLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify);
    static void staticOnNotificationReceived(BLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify);
    
    // Security callbacks
    class SecurityCallbacks;
    class ServerCallbacks;
    
    // Instance pointer for static callbacks
    static NodeLib_ESP32_ANCS* instance;
};

#endif