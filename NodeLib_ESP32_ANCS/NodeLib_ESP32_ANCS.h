#ifndef NODELIB_ESP32_ANCS_H
#define NODELIB_ESP32_ANCS_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <BLEClient.h>
#include <map> 

// Callback types
typedef void (*NodeLibNotificationCallback)(int eventId, uint32_t uid, const char* appId, const char* title, const char* message);
typedef void (*NodeLibMediaCallback)(const char* title, const char* artist, const char* album, bool isPlaying);

class NodeLib_ESP32_ANCS {
public:
    NodeLib_ESP32_ANCS();
    void begin(const char* deviceName = "NodeLib-ESP32");
    void loop();
    void setCallback(NodeLibNotificationCallback cb);
    void setMediaCallback(NodeLibMediaCallback cb);
    
    // Internal callbacks
    void _onAncsDataReceived(uint8_t* pData, size_t length);
    void _onAncsNotificationReceived(uint8_t* pData, size_t length);
    void _onAmsUpdateReceived(uint8_t* pData, size_t length);
    
    void _handleConnect(esp_ble_gatts_cb_param_t *param);
    void _handleDisconnect();
    void _onSecurityComplete(bool success);

private:
    NodeLibNotificationCallback _cbNotify;
    NodeLibMediaCallback _cbMedia;
    
    enum AppState {
        STATE_ADVERTISING,
        STATE_CONNECTED_WAITING, 
        STATE_CONNECTING_CLIENT,
        STATE_WAIT_FOR_SECURITY, 
        STATE_DISCOVERING_SERVICES,
        STATE_SUBSCRIBING,
        STATE_RUNNING
    };
    AppState _currentState;
    unsigned long _stateStartTime;

    // ANCS Parsing State
    enum ParseState {
        ST_WAIT_CMD, ST_CHECK_UID, ST_ATTR_ID, ST_LEN1, ST_LEN2, ST_DATA
    };
    ParseState _pState;
    
    // BLE Objects
    BLEServer* _pServer;
    BLEClient* _pClient;
    BLEAddress* _pRemoteAddress;
    
    // ANCS Characteristics
    BLERemoteCharacteristic* _pRemoteNotif; 
    BLERemoteCharacteristic* _pRemoteCP;    
    BLERemoteCharacteristic* _pRemoteData;  
    bool _ancsAvailable;

    // AMS Characteristics
    BLERemoteCharacteristic* _pRemoteCmd;          
    BLERemoteCharacteristic* _pRemoteEntityUpdate; 
    BLERemoteCharacteristic* _pRemoteEntityAttr;   
    bool _amsAvailable;

    // ANCS Parsing Variables
    int _uidBytesRead;
    uint32_t _parsedUID;
    uint16_t _attrLen;
    uint16_t _attrBytesRead;
    uint8_t _currentAttrId;
    String _currentBuffer; 
    bool _pendingRequest;
    uint32_t _targetUID;
    uint32_t _activeRequestUID;
    String _tempAppId;     
    String _tempTitle;     
    String _tempMessage;   

    // AMS Storage
    String _mediaTitle;    
    String _mediaArtist;   
    String _mediaAlbum;    
    bool _mediaPlaying;
    int _lastPlaybackState;

    bool _securityDone; 
    bool _servicesDumped;
    bool _ancsCharsDumped; 

    // Internal Helpers
    void setState(AppState newState);
    void performAncsRequest(uint32_t uid);
    void subscribeToAms();
    void addSolicitation(BLEAdvertisementData &adv, BLEUUID uuid);
    BLERemoteCharacteristic* findChar(BLERemoteService* pService, BLEUUID uuid);
    BLERemoteService* findService(BLEUUID uuid); 
    void dumpVisibleServices(); 
    void dumpServiceCharacteristics(BLERemoteService* pService);
};

#endif