#include "NodeLib_ESP32_ANCS.h"
#include <esp_random.h>

// Static instance pointer
NodeLib_ESP32_ANCS* NodeLib_ESP32_ANCS::instance = nullptr;

// --- Constants ---
static BLEUUID ancsServiceUUID("7905F431-B5CE-4E99-A40F-4B1E122D00D0");
static BLEUUID notificationSourceUUID("9FBF120D-6301-42D9-8C58-25E699A21DBD");
static BLEUUID controlPointUUID("69D1D8F3-45E1-49A8-9821-9BBDFDAAD9D9");
static BLEUUID dataSourceUUID("22EAC6E9-24D6-4BB5-BE44-B36ACE7C7FDB");

#define CP_CMD_GET_NOTIF_ATTRS 0
#define ATTR_ID_TITLE 1
#define ATTR_ID_MESSAGE 3

// --- Security Callbacks ---
class NodeLib_ESP32_ANCS::SecurityCallbacks : public BLESecurityCallbacks {
    uint32_t onPassKeyRequest() { 
        if (NodeLib_ESP32_ANCS::instance) {
            uint32_t pass = NodeLib_ESP32_ANCS::instance->passkey.toInt();
            Serial.printf("\n>> [PAIRING] PASSKEY requested: %06d\n", pass);
            return pass;
        }
        return 123456;  // Fallback
    }
    
    void onPassKeyNotify(uint32_t pass_key) { 
        Serial.printf("\n>> [PAIRING] PASSKEY: %06d\n", pass_key); 
    }
    
    bool onConfirmPIN(uint32_t pass_key) { 
        Serial.printf(">> [PAIRING] Confirm PIN: %06d\n", pass_key);
        return true; 
    }
    
    bool onSecurityRequest() { 
        Serial.println(">> [PAIRING] Security request received");
        return true; 
    }
    
    void onAuthenticationComplete(esp_ble_auth_cmpl_t cmpl) {
        if (cmpl.success) {
            Serial.println(">> [PAIRING] Success!");
            if (NodeLib_ESP32_ANCS::instance) {
                // Update pairing status
            }
        } else {
            Serial.println(">> [PAIRING] Failed!");
        }
    }
};

// --- Server Callbacks ---
class NodeLib_ESP32_ANCS::ServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer, esp_ble_gatts_cb_param_t *param) {
        Serial.println(">> [STATUS] Device connected.");
        if (NodeLib_ESP32_ANCS::instance) {
            NodeLib_ESP32_ANCS::instance->deviceConnected = true;
            NodeLib_ESP32_ANCS::instance->pRemoteAddress = new BLEAddress(param->connect.remote_bda);
            NodeLib_ESP32_ANCS::instance->setState(STATE_CONNECTED_WAITING);
        }
    }
    
    void onDisconnect(BLEServer* pServer) {
        Serial.println(">> [STATUS] Device disconnected.");
        if (NodeLib_ESP32_ANCS::instance) {
            NodeLib_ESP32_ANCS::instance->deviceConnected = false;
            if (NodeLib_ESP32_ANCS::instance->pRemoteAddress) { 
                delete NodeLib_ESP32_ANCS::instance->pRemoteAddress; 
                NodeLib_ESP32_ANCS::instance->pRemoteAddress = nullptr; 
            }
            NodeLib_ESP32_ANCS::instance->setState(STATE_ADVERTISING);
        }
        delay(500);
        BLEDevice::startAdvertising();
    }
};

// --- Implementation ---

NodeLib_ESP32_ANCS::NodeLib_ESP32_ANCS() {
    instance = this;
    generatePasskey();  // 生成随机配对码
}

NodeLib_ESP32_ANCS::~NodeLib_ESP32_ANCS() {
    if (instance == this) {
        instance = nullptr;
    }
}

void NodeLib_ESP32_ANCS::generatePasskey() {
    // 生成6位随机数字
    uint32_t randomNum = esp_random() % 1000000;  // 0-999999
    if (randomNum < 100000) {
        randomNum += 100000;  // 确保是6位数
    }
    passkey = String(randomNum);
}

String NodeLib_ESP32_ANCS::getPasskey() {
    return passkey;
}

void NodeLib_ESP32_ANCS::setNotificationCallback(ANCSNotificationCallback cb) {
    notificationCallback = cb;
}

void NodeLib_ESP32_ANCS::setEventCallback(ANCSEventCallback cb) {
    eventCallback = cb;
}

bool NodeLib_ESP32_ANCS::isConnected() {
    return deviceConnected;
}

bool NodeLib_ESP32_ANCS::isRunning() {
    return currentState == STATE_RUNNING;
}

void NodeLib_ESP32_ANCS::setState(AppState newState) {
    currentState = newState;
    stateStartTime = millis();
    Serial.printf(">> [STATE] Changed to: %d\n", newState);
}

void NodeLib_ESP32_ANCS::addSolicitation(BLEAdvertisementData &adv) {
    uint8_t uuid[] = {0xD0, 0x00, 0x2D, 0x12, 0x1E, 0x4B, 0x0F, 0xA4, 0x99, 0x4E, 0xCE, 0xB5, 0x31, 0xF4, 0x05, 0x79};
    String data = "";
    data += (char)17; data += (char)0x15; 
    for(int i=0; i<16; i++) data += (char)uuid[i];
    adv.addData(data);
}

void NodeLib_ESP32_ANCS::begin(const char* deviceName) {
    Serial.println(">> [BOOT] NodeLib ESP32 ANCS Library");
    Serial.printf(">> [PAIRING] Generated passkey: %s\n", passkey.c_str());
    
    BLEDevice::init(deviceName); 
    BLEDevice::setMTU(517);
    
    BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT_MITM);
    BLEDevice::setSecurityCallbacks(new SecurityCallbacks());
    
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());
    
    BLEService *pAncs = pServer->createService(ancsServiceUUID);
    pAncs->createCharacteristic(notificationSourceUUID, BLECharacteristic::PROPERTY_NOTIFY);
    pAncs->start();
    
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    BLEAdvertisementData oAdvData;
    oAdvData.setFlags(0x06);
    oAdvData.setName(deviceName);
    oAdvData.setCompleteServices(BLEUUID("7905F431-B5CE-4E99-A40F-4B1E122D00D0")); 
    pAdvertising->setAdvertisementData(oAdvData);
    
    BLEAdvertisementData oScanData;
    addSolicitation(oScanData); 
    oScanData.setAppearance(0x00C2); 
    pAdvertising->setScanResponseData(oScanData);
    pAdvertising->setScanResponse(true);
    
    BLESecurity *pSecurity = new BLESecurity();
    pSecurity->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_MITM_BOND);
    pSecurity->setCapability(ESP_IO_CAP_OUT);
    pSecurity->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
    
    BLEDevice::startAdvertising();
    Serial.println(">> [READY] Waiting for iOS device...");
}

void NodeLib_ESP32_ANCS::performRequest(uint32_t uid) {
    if(!pRemoteCP) return;
    
    Serial.printf(">> [REQ] Requesting UID: %d...\n", uid);
    
    activeRequestUID = uid;
    pState = ST_WAIT_CMD;
    tempTitle = "";
    tempMessage = "";
    
    uint8_t command[11];
    command[0] = CP_CMD_GET_NOTIF_ATTRS;
    command[1] = (uint8_t)(uid & 0xFF);
    command[2] = (uint8_t)((uid >> 8) & 0xFF);
    command[3] = (uint8_t)((uid >> 16) & 0xFF);
    command[4] = (uint8_t)((uid >> 24) & 0xFF);
    command[5] = ATTR_ID_TITLE;
    command[6] = 255; command[7] = 0;   
    command[8] = ATTR_ID_MESSAGE;
    command[9] = 255; command[10] = 0;
    
    pRemoteCP->writeValue(command, 11, true); 
}

void NodeLib_ESP32_ANCS::handleDataReceived(uint8_t* pData, size_t length) {
    for (int i = 0; i < length; i++) {
        uint8_t b = pData[i];
        
        switch (pState) {
            case ST_WAIT_CMD:
                if (b == 0) { 
                    pState = ST_CHECK_UID;
                    uidBytesRead = 0;
                    parsedUID = 0;
                }
                break;
                
            case ST_CHECK_UID:
                parsedUID |= ((uint32_t)b << (uidBytesRead * 8));
                uidBytesRead++;
                
                if (uidBytesRead >= 4) {
                    if (parsedUID == activeRequestUID) {
                        Serial.printf("\n>> [MSG] Found Data for UID: %d\n", parsedUID);
                        pState = ST_ATTR_ID;
                    } else {
                        pState = ST_WAIT_CMD;
                    }
                }
                break;
                
            case ST_ATTR_ID:
                currentAttrId = b;
                pState = ST_LEN1;
                break;
                
            case ST_LEN1:
                attrLen = b;
                pState = ST_LEN2;
                break;
                
            case ST_LEN2:
                attrLen |= (b << 8);
                attrBytesRead = 0;
                currentBuffer = ""; 
                
                if (attrLen == 0) {
                    pState = ST_ATTR_ID; 
                } else {
                    pState = ST_DATA;
                }
                break;
                
            case ST_DATA:
                if (b != 0) { 
                    currentBuffer += (char)b;
                }
                
                attrBytesRead++;
                if (attrBytesRead >= attrLen) {
                    if (currentAttrId == ATTR_ID_TITLE) {
                        tempTitle = currentBuffer;
                    } else if (currentAttrId == ATTR_ID_MESSAGE) {
                        tempMessage = currentBuffer;
                        
                        // Call the user callback with the notification
                        if (notificationCallback) {
                            notificationCallback(activeRequestUID, tempTitle.c_str(), tempMessage.c_str());
                        }
                        
                        // Also print to Serial for debugging
                        Serial.printf("   [Title]: %s\n", tempTitle.c_str());
                        Serial.printf("   [Body]: %s\n", tempMessage.c_str());
                    } else {
                        Serial.printf("   [Attr %d]: %s\n", currentAttrId, currentBuffer.c_str());
                    }
                    
                    pState = ST_ATTR_ID; 
                }
                break;
        }
    }
}

void NodeLib_ESP32_ANCS::handleNotificationReceived(uint8_t* pData, size_t length) {
    if (length < 8) return;
    
    uint8_t eventID = pData[0];
    uint32_t uid = (uint32_t)pData[4] | 
                   ((uint32_t)pData[5] << 8) | 
                   ((uint32_t)pData[6] << 16) | 
                   ((uint32_t)pData[7] << 24);
    
    Serial.printf(">> [NOTIF] Event: %d | UID: %d\n", eventID, uid);
    
    // Call event callback
    if (eventCallback) {
        eventCallback(eventID, uid);
    }
    
    if (eventID == 0 || eventID == 1) { // Added or Modified
        targetUID = uid;
        pendingRequest = true; 
    }
}

// Static callback functions
void NodeLib_ESP32_ANCS::staticOnDataReceived(BLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
    if (instance) {
        instance->handleDataReceived(pData, length);
    }
}

void NodeLib_ESP32_ANCS::staticOnNotificationReceived(BLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
    if (instance) {
        instance->handleNotificationReceived(pData, length);
    }
}

void NodeLib_ESP32_ANCS::loop() {
    if (!deviceConnected && currentState != STATE_ADVERTISING) {
        setState(STATE_ADVERTISING);
    }
    
    if (currentState == STATE_RUNNING && pendingRequest) {
        pendingRequest = false; 
        performRequest(targetUID);
        delay(50);
    }
    
    switch (currentState) {
        case STATE_ADVERTISING:
            break;
            
        case STATE_CONNECTED_WAITING:
            if (millis() - stateStartTime > 5000) {
                Serial.println(">> [WAIT] 5s timeout. Connecting Client...");
                setState(STATE_CONNECTING_CLIENT);
            }
            break;
            
        case STATE_CONNECTING_CLIENT:
            if (pClient == nullptr) pClient = BLEDevice::createClient();
            
            if (!pClient->isConnected()) {
                if (pRemoteAddress && pClient->connect(*pRemoteAddress)) {
                    Serial.println(">> [CLIENT] GATT Connected.");
                    setState(STATE_DISCOVERING_SERVICES);
                } else {
                    Serial.println(">> [ERROR] GATT Connect failed. Retrying...");
                    delay(2000);
                }
            } else {
                setState(STATE_DISCOVERING_SERVICES);
            }
            break;
            
        case STATE_DISCOVERING_SERVICES:
            {
                BLERemoteService* pRemoteService = pClient->getService(ancsServiceUUID);
                if (pRemoteService == nullptr) {
                    Serial.println(">> [WAIT] ANCS Service not found...");
                    delay(1000);
                    return; 
                }
                
                std::map<std::string, BLERemoteCharacteristic*>* pCharsMap = pRemoteService->getCharacteristics();
                if (pCharsMap == nullptr) { 
                    delay(1000); 
                    return; 
                }
                
                pRemoteNotif = nullptr;
                pRemoteCP = nullptr;
                pRemoteData = nullptr;
                
                for (auto const& [uuid_str, pChar] : *pCharsMap) {
                    if (pChar->getUUID().equals(notificationSourceUUID)) pRemoteNotif = pChar;
                    else if (pChar->getUUID().equals(controlPointUUID)) pRemoteCP = pChar;
                    else if (pChar->getUUID().equals(dataSourceUUID)) pRemoteData = pChar;
                }
                
                if (pRemoteData == nullptr) {
                    for (auto const& [uuid_str, pChar] : *pCharsMap) {
                        bool isNotif = pRemoteNotif && pChar->getUUID().equals(pRemoteNotif->getUUID());
                        bool isCP = pRemoteCP && pChar->getUUID().equals(pRemoteCP->getUUID());
                        if (!isNotif && !isCP) {
                            pRemoteData = pChar;
                            Serial.printf(">> [FIX] Auto-assigned Data Source.\n");
                            break;
                        }
                    }
                }
                
                if (pRemoteNotif && pRemoteCP && pRemoteData) {
                    Serial.println(">> [DONE] Services Ready.");
                    setState(STATE_SUBSCRIBING);
                } else {
                    Serial.println(">> [RETRY] Missing chars. Retrying...");
                    delay(3000); 
                }
            }
            break;
            
        case STATE_SUBSCRIBING:
            {
                Serial.println(">> [SUB] Subscribing...");
                if (pRemoteData->canNotify()) pRemoteData->registerForNotify(staticOnDataReceived);
                delay(200);
                if (pRemoteNotif->canNotify()) pRemoteNotif->registerForNotify(staticOnNotificationReceived);
                
                Serial.println(">> [SUCCESS] READY TO RECEIVE NOTIFICATIONS!");
                setState(STATE_RUNNING);
            }
            break;
            
        case STATE_RUNNING:
            // Main running state
            break;
    }
    delay(20);
}