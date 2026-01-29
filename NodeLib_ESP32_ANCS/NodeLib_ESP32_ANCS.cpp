#include "NodeLib_ESP32_ANCS.h"
#include "esp_mac.h"
#include "esp_gap_ble_api.h"

// --- UUID CONSTANTS ---
static BLEUUID ANCS_SERVICE_UUID("7905F431-B5CE-4E99-A40F-4B1E122D00D0");
static BLEUUID ANCS_NOTIF_UUID("9FBF120D-6301-42D9-8C58-25E699A21DBD");
static BLEUUID ANCS_CP_UUID("69D1D8F3-45E1-49A8-9821-9BBDFDAAD9D9");
static BLEUUID ANCS_DATA_UUID("22EAC6E9-24D6-4BB5-BE44-B36ACE7C7FDB");
// Fallback for non-standard or corrupted UUIDs seen in logs
static BLEUUID ANCS_DATA_UUID_ALT("22eac6e9-24d6-4bb5-be44-b36ace7c7bfb");

static BLEUUID AMS_SERVICE_UUID("89D3502B-0F36-433A-8EF4-C502AD55F8DC");
static BLEUUID AMS_REMOTE_CMD_UUID("9B3C81D8-57B1-4A8A-B8DF-0E56F7CA51C2");
static BLEUUID AMS_ENTITY_UPDATE_UUID("2F7CABCE-808D-411F-9A0C-BB92BA96C102");
static BLEUUID AMS_ENTITY_ATTR_UUID("C6B2F38C-23AB-46D8-A6AB-A3A870BBD5D7");

// ANCS Commands
#define CP_CMD_GET_NOTIF_ATTRS 0
#define ATTR_ID_APP_ID 0
#define ATTR_ID_TITLE 1
#define ATTR_ID_SUBTITLE 2
#define ATTR_ID_MESSAGE 3
#define ATTR_ID_DATE 5

// AMS Entity IDs
#define AMSID_Player 0
#define AMSID_Queue  1
#define AMSID_Track  2

// AMS Player Attributes
#define AMSPlayerAttr_Name 0
#define AMSPlayerAttr_PlaybackInfo 1
#define AMSPlayerAttr_Volume 2

// AMS Track Attributes
#define AMSTrackAttr_Artist 0
#define AMSTrackAttr_Album  1
#define AMSTrackAttr_Title  2

NodeLib_ESP32_ANCS* globalNodeLibInstance = nullptr;

// --- SECURITY CALLBACKS ---
class NodeLibSecurityCallbacks : public BLESecurityCallbacks {
  uint32_t onPassKeyRequest(){ return 0; }
  void onPassKeyNotify(uint32_t pass_key){ Serial.printf(">> [PAIRING] PIN: %06d\n", pass_key); }
  bool onConfirmPIN(uint32_t pass_key){ return true; } 
  bool onSecurityRequest(){ return true; }
  void onAuthenticationComplete(esp_ble_auth_cmpl_t cmpl){
    if(cmpl.success){
        Serial.println(">> [PAIRING] Success!");
    } else {
        Serial.printf(">> [PAIRING] Fail: %d\n", cmpl.fail_reason);
    }
    if(globalNodeLibInstance) globalNodeLibInstance->_onSecurityComplete(cmpl.success);
  }
};

// --- SERVER CALLBACKS ---
class NodeLibServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer, esp_ble_gatts_cb_param_t *param) {
    if (globalNodeLibInstance) globalNodeLibInstance->_handleConnect(param);
  }
  void onDisconnect(BLEServer* pServer) {
    if (globalNodeLibInstance) globalNodeLibInstance->_handleDisconnect();
  }
};

// --- STATIC WRAPPERS ---
static void staticOnAncsData(BLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
    if (globalNodeLibInstance) globalNodeLibInstance->_onAncsDataReceived(pData, length);
}
static void staticOnAncsNotif(BLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
    if (globalNodeLibInstance) globalNodeLibInstance->_onAncsNotificationReceived(pData, length);
}
static void staticOnAmsUpdate(BLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
    if (globalNodeLibInstance) globalNodeLibInstance->_onAmsUpdateReceived(pData, length);
}

// --- IMPLEMENTATION ---

NodeLib_ESP32_ANCS::NodeLib_ESP32_ANCS() {
    globalNodeLibInstance = this;
    _currentState = STATE_ADVERTISING;
    _pState = ST_WAIT_CMD;
    _cbNotify = nullptr;
    _cbMedia = nullptr;
    _pRemoteAddress = nullptr;
    _pClient = nullptr;
    
    _amsAvailable = false;
    _ancsAvailable = false;
    _mediaPlaying = false;
    _lastPlaybackState = -1;
    _mediaTitle = "Unknown";
    _mediaArtist = "Unknown";
    _mediaAlbum = "Unknown";
    _servicesDumped = false;
    _ancsCharsDumped = false;
}

void NodeLib_ESP32_ANCS::setCallback(NodeLibNotificationCallback cb) { _cbNotify = cb; }
void NodeLib_ESP32_ANCS::setMediaCallback(NodeLibMediaCallback cb) { _cbMedia = cb; }

void NodeLib_ESP32_ANCS::setState(AppState newState) {
    _currentState = newState;
    _stateStartTime = millis();
    Serial.printf(">> [STATE] -> %d\n", newState);
}

void NodeLib_ESP32_ANCS::_onSecurityComplete(bool success) { _securityDone = success; }

void NodeLib_ESP32_ANCS::addSolicitation(BLEAdvertisementData &adv, BLEUUID uuid) {
    uint8_t d[18];
    d[0] = 17; d[1] = 0x15; // Len, Type
    
    esp_bt_uuid_t *raw = uuid.getNative();
    for(int i=0; i<16; i++) {
        d[2+i] = raw->uuid.uuid128[15-i]; 
    }
    String s = "";
    for(int i=0; i<18; i++) s += (char)d[i];
    adv.addData(s);
}

void NodeLib_ESP32_ANCS::begin(const char* deviceName) {
    BLEDevice::init(deviceName); 
    BLEDevice::setMTU(517);
    
    BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT_MITM);
    BLEDevice::setSecurityCallbacks(new NodeLibSecurityCallbacks());
  
    _pServer = BLEDevice::createServer();
    _pServer->setCallbacks(new NodeLibServerCallbacks()); 
  
    // Device Info & HID
    _pServer->createService(BLEUUID("180A"))->start();
    _pServer->createService(BLEUUID((uint16_t)0x1812))->start();
  
    BLEAdvertising *pAdv = BLEDevice::getAdvertising();
    
    BLEAdvertisementData oAdvData;
    oAdvData.setFlags(0x06); 
    oAdvData.setName(deviceName);
    oAdvData.setAppearance(0x00C2); 
    oAdvData.setCompleteServices(BLEUUID((uint16_t)0x1812)); 
    pAdv->setAdvertisementData(oAdvData);
  
    BLEAdvertisementData oScanData;
    addSolicitation(oScanData, ANCS_SERVICE_UUID);
    pAdv->setScanResponseData(oScanData);
    pAdv->setScanResponse(true);
    
    BLESecurity *pSec = new BLESecurity();
    pSec->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_MITM_BOND);
    pSec->setCapability(ESP_IO_CAP_IO); 
    pSec->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
  
    BLEDevice::startAdvertising();
}

void NodeLib_ESP32_ANCS::_handleConnect(esp_ble_gatts_cb_param_t *param) {
    if (_pRemoteAddress) delete _pRemoteAddress;
    _pRemoteAddress = new BLEAddress(param->connect.remote_bda);
    Serial.printf(">> [CONN] %s\n", _pRemoteAddress->toString().c_str());
    _securityDone = false;
    _servicesDumped = false;
    _ancsCharsDumped = false;
    setState(STATE_CONNECTED_WAITING);
}

void NodeLib_ESP32_ANCS::_handleDisconnect() {
    Serial.println(">> [DISC] Disconnected");
    _securityDone = false;
    _amsAvailable = false;
    _ancsAvailable = false;
    _servicesDumped = false;
    _ancsCharsDumped = false;
    setState(STATE_ADVERTISING);
    BLEDevice::startAdvertising();
}

// FIX: Case-insensitive service lookup
BLERemoteService* NodeLib_ESP32_ANCS::findService(BLEUUID uuid) {
    BLERemoteService* pS = _pClient->getService(uuid);
    if (pS) return pS;

    std::map<std::string, BLERemoteService*>* pServices = _pClient->getServices();
    if (!pServices) return nullptr;
    
    for (auto const& [uuid_str, service] : *pServices) {
        if (service->getUUID().equals(uuid)) {
            return service;
        }
    }
    return nullptr;
}

void NodeLib_ESP32_ANCS::dumpVisibleServices() {
    if(!_pClient) return;
    std::map<std::string, BLERemoteService*>* pServices = _pClient->getServices();
    if (pServices == nullptr) {
        Serial.println(">> [DEBUG] getServices() returned null!");
        return;
    }
    
    Serial.println(">> [DEBUG] --- Remote Service Dump ---");
    for (auto const& [uuid_str, service] : *pServices) {
        Serial.printf("   - UUID: %s\n", service->getUUID().toString().c_str());
        if (service->getUUID().equals(ANCS_SERVICE_UUID)) Serial.println("     ^-- THIS IS ANCS!");
    }
    Serial.println(">> [DEBUG] ---------------------------");
}

void NodeLib_ESP32_ANCS::dumpServiceCharacteristics(BLERemoteService* pService) {
    if (!pService) return;
    std::map<std::string, BLERemoteCharacteristic*>* pChars = pService->getCharacteristics();
    if (!pChars) {
        Serial.println(">> [DEBUG] Service has NO characteristics map.");
        return;
    }
    Serial.printf(">> [DEBUG] Dump Chars for Service: %s\n", pService->getUUID().toString().c_str());
    for (auto const& [uuid_str, pChar] : *pChars) {
         Serial.printf("   - Char UUID: %s\n", pChar->getUUID().toString().c_str());
    }
    Serial.println(">> [DEBUG] ---------------------------");
}

BLERemoteCharacteristic* NodeLib_ESP32_ANCS::findChar(BLERemoteService* pService, BLEUUID uuid) {
    if (!pService) return nullptr;
    BLERemoteCharacteristic* pChar = pService->getCharacteristic(uuid);
    if (pChar) return pChar;
    auto* m = pService->getCharacteristics();
    if (!m) return nullptr;
    for (auto& entry : *m) {
        if (entry.second->getUUID().equals(uuid)) return entry.second;
    }
    return nullptr;
}

void NodeLib_ESP32_ANCS::loop() {
  if (_currentState == STATE_RUNNING) {
      if (_pendingRequest) {
          _pendingRequest = false; 
          performAncsRequest(_targetUID); 
          delay(50); 
      }
  }

  switch (_currentState) {
    case STATE_ADVERTISING: break;
        
    case STATE_CONNECTED_WAITING:
      if (millis() - _stateStartTime > 2000) setState(STATE_CONNECTING_CLIENT);
      break;
      
    case STATE_CONNECTING_CLIENT:
       if (!_pClient) _pClient = BLEDevice::createClient();
       if (!_pClient->isConnected()) {
           if (_pRemoteAddress) {
               Serial.println(">> [CLIENT] Connecting to phone...");
               if (_pClient->connect(*_pRemoteAddress)) {
                   Serial.println(">> [CLIENT] Connected. Negotiating Security...");
                   esp_bd_addr_t remoteAddr; memcpy(remoteAddr, _pRemoteAddress->getNative(), 6);
                   esp_ble_set_encryption(remoteAddr, ESP_BLE_SEC_ENCRYPT_MITM);
                   setState(STATE_WAIT_FOR_SECURITY); 
               }
           }
       } else {
           setState(STATE_WAIT_FOR_SECURITY);
       }
       break;

    case STATE_WAIT_FOR_SECURITY:
       if (_securityDone) {
           Serial.println(">> [SECURE] Encrypted. Looking for Services...");
           setState(STATE_DISCOVERING_SERVICES);
       } else if (millis() - _stateStartTime > 15000) {
           Serial.println(">> [SECURE] Warning: Timeout waiting for security callback. Continuing anyway...");
           setState(STATE_DISCOVERING_SERVICES);
       }
       break;
       
    case STATE_DISCOVERING_SERVICES:
       {
           if (!_pClient->isConnected()) return;

           if (!_servicesDumped) {
               dumpVisibleServices();
               _servicesDumped = true;
           }
           
           // ANCS Discovery (Enhanced with manual fallback)
           if (!_ancsAvailable) {
               BLERemoteService* pAncs = findService(ANCS_SERVICE_UUID);
               if (pAncs) {
                    _pRemoteNotif = findChar(pAncs, ANCS_NOTIF_UUID);
                    _pRemoteCP    = findChar(pAncs, ANCS_CP_UUID);
                    _pRemoteData  = findChar(pAncs, ANCS_DATA_UUID);
                    
                    // Fallback for weird data UUID
                    if (!_pRemoteData) {
                       _pRemoteData = findChar(pAncs, ANCS_DATA_UUID_ALT);
                       if (_pRemoteData) Serial.println(">> [ANCS] Found Data Source with ALT UUID.");
                    }
                    
                    if (!_pRemoteNotif || !_pRemoteCP || !_pRemoteData) {
                         if (!_ancsCharsDumped) {
                             if (!_pRemoteNotif) Serial.println(">> [ERR] ANCS Notification Char missing");
                             if (!_pRemoteCP) Serial.println(">> [ERR] ANCS Control Point Char missing");
                             if (!_pRemoteData) Serial.println(">> [ERR] ANCS Data Source Char missing");
                             
                             Serial.println(">> [DEBUG] Dumping ALL characteristics found in ANCS Service:");
                             dumpServiceCharacteristics(pAncs);
                             _ancsCharsDumped = true;
                         }
                    } else {
                         Serial.println(">> [ANCS] Service FOUND! (Notifications Enabled)");
                         _ancsAvailable = true;
                    }
               }
           }

           // AMS Discovery (Enhanced with manual fallback)
           if (!_amsAvailable) {
               BLERemoteService* pAms = findService(AMS_SERVICE_UUID);
               if (pAms) {
                   _pRemoteCmd = findChar(pAms, AMS_REMOTE_CMD_UUID);
                   _pRemoteEntityUpdate = findChar(pAms, AMS_ENTITY_UPDATE_UUID);
                   _pRemoteEntityAttr = findChar(pAms, AMS_ENTITY_ATTR_UUID);
                   if (_pRemoteCmd && _pRemoteEntityUpdate && _pRemoteEntityAttr) {
                       _amsAvailable = true;
                       Serial.println(">> [AMS] Service FOUND! (Media Enabled)");
                   }
               } 
           }

           // Retry / Timeout logic
           bool readyToSubscribe = false;
           
           if (_ancsAvailable && _amsAvailable) {
               readyToSubscribe = true;
           } else if (_ancsAvailable && !_amsAvailable) {
               readyToSubscribe = true; 
           } else if (!_ancsAvailable && _amsAvailable) {
               if (millis() - _stateStartTime > 8000) {
                   static bool warned = false;
                   if(!warned) { Serial.println(">> [WARN] ANCS Service STILL NOT found after retry. Continuing with partial features."); warned=true; }
                   readyToSubscribe = true; 
               }
           } else {
               if (millis() - _stateStartTime > 15000) {
                   Serial.println(">> [ERR] No Services found. Disconnecting.");
                   _handleDisconnect();
               }
           }

           if (readyToSubscribe) {
               setState(STATE_SUBSCRIBING);
           }
       }
       break;
       
    case STATE_SUBSCRIBING:
       Serial.println(">> [SUB] Subscribing to characteristics...");
       // ANCS
       if(_ancsAvailable && _pRemoteData && _pRemoteData->canNotify()) _pRemoteData->registerForNotify(staticOnAncsData);
       if(_ancsAvailable && _pRemoteNotif && _pRemoteNotif->canNotify()) _pRemoteNotif->registerForNotify(staticOnAncsNotif);
       
       // AMS
       if (_amsAvailable && _pRemoteEntityUpdate && _pRemoteEntityUpdate->canNotify()) {
           _pRemoteEntityUpdate->registerForNotify(staticOnAmsUpdate);
           subscribeToAms();
       }
       
       Serial.println(">> [READY] Listening for Events.");
       setState(STATE_RUNNING);
       break;
       
    case STATE_RUNNING: break;
  }
  delay(10);
}

// --- ANCS ---

void NodeLib_ESP32_ANCS::performAncsRequest(uint32_t uid) {
    if(!_pRemoteCP) return;
    
    // Debug print
    Serial.printf(">> [ANCS] Requesting details for UID: %d\n", uid);

    _activeRequestUID = uid;
    _pState = ST_WAIT_CMD; 
    _tempAppId = ""; _tempTitle = ""; _tempMessage = "";

    uint8_t command[14];
    command[0] = CP_CMD_GET_NOTIF_ATTRS;
    command[1] = (uint8_t)(uid & 0xFF); command[2] = (uint8_t)((uid >> 8) & 0xFF);
    command[3] = (uint8_t)((uid >> 16) & 0xFF); command[4] = (uint8_t)((uid >> 24) & 0xFF);
    
    command[5] = ATTR_ID_APP_ID; command[6] = 255; command[7] = 0;
    command[8] = ATTR_ID_TITLE; command[9] = 255; command[10] = 0;
    command[11] = ATTR_ID_MESSAGE; command[12] = 255; command[13] = 0;

    _pRemoteCP->writeValue(command, 14, true); 
}

void NodeLib_ESP32_ANCS::_onAncsNotificationReceived(uint8_t* pData, size_t length) {
    if (length < 8) return;
    uint8_t eventID = pData[0];
    uint8_t eventFlags = pData[1];
    uint8_t catID = pData[2];
    uint32_t uid = (uint32_t)pData[4] | ((uint32_t)pData[5] << 8) | ((uint32_t)pData[6] << 16) | ((uint32_t)pData[7] << 24);
    
    Serial.printf(">> [ANCS EVENT] ID:%d Cat:%d UID:%d\n", eventID, catID, uid);

    // EventID: 0=Added, 1=Modified, 2=Removed
    if (eventID == 0 || eventID == 1) { 
        _targetUID = uid; 
        _pendingRequest = true; 
    }
}

void NodeLib_ESP32_ANCS::_onAncsDataReceived(uint8_t* pData, size_t length) {
    for (int i = 0; i < length; i++) {
        uint8_t b = pData[i];
        switch (_pState) {
            case ST_WAIT_CMD: 
                if (b == 0) { 
                    _pState = ST_CHECK_UID; _uidBytesRead = 0; _parsedUID = 0; 
                } 
                break;
                
            case ST_CHECK_UID:
                _parsedUID |= ((uint32_t)b << (_uidBytesRead * 8)); _uidBytesRead++;
                if (_uidBytesRead >= 4) {
                    if (_parsedUID == _activeRequestUID) {
                        _pState = ST_ATTR_ID; 
                    } else {
                        _pState = ST_WAIT_CMD; 
                    }
                }
                break;
                
            case ST_ATTR_ID: 
                _currentAttrId = b; 
                _pState = ST_LEN1; 
                break;
                
            case ST_LEN1: 
                _attrLen = b; 
                _pState = ST_LEN2; 
                break;
                
            case ST_LEN2:
                _attrLen |= (b << 8); 
                _attrBytesRead = 0; 
                _currentBuffer = ""; 
                if (_attrLen == 0) {
                     _pState = ST_ATTR_ID; 
                } else {
                     _pState = ST_DATA;
                }
                break;
                
            case ST_DATA:
                if (b != 0) _currentBuffer += (char)b; 
                _attrBytesRead++;
                
                if (_attrBytesRead >= _attrLen) {
                    if (_currentAttrId == ATTR_ID_APP_ID) _tempAppId = _currentBuffer;
                    else if (_currentAttrId == ATTR_ID_TITLE) _tempTitle = _currentBuffer;
                    else if (_currentAttrId == ATTR_ID_MESSAGE) _tempMessage = _currentBuffer;
                    
                    if (_currentAttrId == ATTR_ID_MESSAGE) {
                        if (_cbNotify) {
                            _cbNotify(0, _activeRequestUID, _tempAppId.c_str(), _tempTitle.c_str(), _tempMessage.c_str());
                        }
                    }
                    _pState = ST_ATTR_ID; 
                }
                break;
        }
    }
}

// --- AMS ---

void NodeLib_ESP32_ANCS::subscribeToAms() {
    if(!_pRemoteEntityUpdate || !_pRemoteEntityAttr) return;
    
    uint8_t cmdPlayer[] = { AMSID_Player, AMSPlayerAttr_PlaybackInfo }; 
    _pRemoteEntityUpdate->writeValue(cmdPlayer, 2, true);
    
    delay(500); 
    
    uint8_t cmdTrack[] = { 
        AMSID_Track, AMSTrackAttr_Artist, 
        AMSID_Track, AMSTrackAttr_Album, 
        AMSID_Track, AMSTrackAttr_Title 
    }; 
    _pRemoteEntityUpdate->writeValue(cmdTrack, 6, true);
}

void NodeLib_ESP32_ANCS::_onAmsUpdateReceived(uint8_t* pData, size_t length) {
    if (length < 3) return;
    
    uint8_t entityID = pData[0];
    uint8_t attrID = pData[1];
    uint8_t flags = pData[2]; 
    
    String valueStr = "";
    if (length > 3) {
        for(size_t i=3; i<length; i++) valueStr += (char)pData[i];
    }
    if(flags & 1) valueStr += " (trunc)";

    bool isTimeUpdate = (entityID == AMSID_Player && attrID == AMSPlayerAttr_PlaybackInfo);
    bool stateChanged = false;

    if (isTimeUpdate) {
        int comma = valueStr.indexOf(',');
        int newState = -1;
        if(comma != -1) newState = valueStr.substring(0, comma).toInt();
        else if (valueStr.length() > 0) newState = valueStr.toInt();
        
        if (newState != _lastPlaybackState) {
            stateChanged = true;
            _lastPlaybackState = newState;
        }
    }
    
    // Logic for callbacks
    if (entityID == AMSID_Player && attrID == AMSPlayerAttr_PlaybackInfo && stateChanged) {
        _mediaPlaying = (_lastPlaybackState == 1);
        if (_cbMedia) _cbMedia(_mediaTitle.c_str(), _mediaArtist.c_str(), _mediaAlbum.c_str(), _mediaPlaying);
    }
    else if (entityID == AMSID_Track) {
        if (attrID == AMSTrackAttr_Artist) _mediaArtist = valueStr;
        else if (attrID == AMSTrackAttr_Album) _mediaAlbum = valueStr;
        else if (attrID == AMSTrackAttr_Title) _mediaTitle = valueStr;
        
        if (_cbMedia) _cbMedia(_mediaTitle.c_str(), _mediaArtist.c_str(), _mediaAlbum.c_str(), _mediaPlaying);
    }
}
