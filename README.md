# NodeLib_ESP32_ANCS
一个用于ESP32的ANCS (Apple Notification Center Service) 客户端库，支持通过BLE接收iOS设备通知。


[已知问题]
iOS26无法搜索到蓝牙，显示一个不正确的名称


功能特性
✅ 通过BLE接收iOS设备通知
✅ 支持中文、Emoji等UTF-8字符
✅ 自动配对和重连
✅ 状态机管理，稳定可靠
✅ 回调函数处理通知事件
✅ 随机6位配对码，提高安全性
✅ 兼容ESP32、ESP32-S3等系列


硬件要求
ESP32开发板 (测试使用ESP32-S3 N16R8)
USB数据线
iOS设备 (iPhone/iPad, 测试使用iOS16 iPadOS16)
软件依赖


Arduino IDE
安装 Arduino IDE 测试使用版本2.3.6
ESP32开发板支持
打开Arduino IDE
文件 > 首选项
在"附加开发板管理器网址"中添加：
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
工具 > 开发板 > 开发板管理器
搜索 "ESP32"，安装 "ESP32 by Espressif Systems"
必需的库
工具 > 管理库...
搜索并安装以下库：
ESP32 BLE Arduino by Neil Kolban
如果需要显示OLED，可安装：Adafruit SSD1306, Adafruit GFX Library
安装库


方法1：手动安装
下载本库的ZIP文件
Arduino IDE: 项目 > 加载库 > 添加.ZIP库...
选择下载的ZIP文件
方法2：复制到库文件夹
克隆或下载本仓库
复制 NodeLib_ESP32_ANCS文件夹到Arduino的libraries目录
Windows: `Documents\Arduino\libraries`
Mac: ~/Documents/Arduino/libraries/
Linux: ~/Arduino/libraries/
快速开始[注：下列程序使用AI生成，尚未验证，请使用项目中的NodeLib_Example.ino工程]

1. 基本示例


/**
 * NodeLib ESP32 ANCS 基础示例
 * 接收iOS通知并通过串口打印
 */

#include <NodeLib_ESP32_ANCS.h>

// 创建ANCS实例
NodeLib_ESP32_ANCS ancs;

// 通知回调函数
void onNotificationReceived(uint32_t uid, const char* title, const char* message) {
    Serial.println("\n=== 新通知 ===");
    Serial.printf("UID: %u\n", uid);
    Serial.printf("标题: %s\n", title);
    Serial.printf("内容: %s\n", message);
    Serial.println("==============\n");
}

// 事件回调函数
void onEventReceived(uint8_t eventId, uint32_t uid) {
    const char* eventName = "未知";
    switch(eventId) {
        case 0: eventName = "添加"; break;
        case 1: eventName = "修改"; break;
        case 2: eventName = "移除"; break;
    }
    Serial.printf(">> 事件: %s (UID: %u)\n", eventName, uid);
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n=== ESP32 ANCS 通知接收器 ===");
    
    // 获取配对码
    String passkey = ancs.getPasskey();
    Serial.printf("配对码: %s\n", passkey.c_str());
    
    // 设置回调函数
    ancs.setNotificationCallback(onNotificationReceived);
    ancs.setEventCallback(onEventReceived);
    
    // 初始化ANCS
    ancs.begin("ESP32-ANCS设备");
    
    Serial.println("\n配对说明:");
    Serial.println("1. iPhone进入 设置 > 蓝牙");
    Serial.println("2. 连接 'ESP32-ANCS设备'");
    Serial.println("3. 输入配对码: " + passkey);
    Serial.println("4. 在iPhone通知设置中确保'显示预览'设为'始终'");
}

void loop() {
    ancs.loop();  // 必须调用，保持库运行
}







2. 带OLED显示的示例

/**
 * NodeLib ESP32 ANCS OLED显示示例
 * 接收通知并在OLED屏幕上显示
 */

#include <NodeLib_ESP32_ANCS.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
NodeLib_ESP32_ANCS ancs;

// 在OLED上显示通知
void displayNotification(const char* title, const char* message) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    
    // 显示标题
    display.setCursor(0, 0);
    display.println("=== 新通知 ===");
    
    // 显示标题内容
    display.setCursor(0, 20);
    display.print("标题: ");
    display.println(title);
    
    // 显示消息内容
    display.setCursor(0, 40);
    display.print("内容: ");
    display.println(message);
    
    display.display();
}

// 通知回调函数
void onNotificationReceived(uint32_t uid, const char* title, const char* message) {
    Serial.println("\n=== 新通知 ===");
    Serial.printf("标题: %s\n", title);
    Serial.printf("内容: %s\n", message);
    
    // 在OLED上显示
    displayNotification(title, message);
    
    // 可以添加其他功能，如振动马达
    // digitalWrite(VIBRATOR_PIN, HIGH);
    // delay(200);
    // digitalWrite(VIBRATOR_PIN, LOW);
}

void setup() {
    Serial.begin(115200);
    
    // 初始化OLED
    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println(F("SSD1306分配失败"));
        for(;;);
    }
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("ANCS通知接收器");
    display.display();
    
    // 获取配对码
    String passkey = ancs.getPasskey();
    
    // 设置回调
    ancs.setNotificationCallback(onNotificationReceived);
    
    // 初始化ANCS
    ancs.begin("ESP32通知屏");
    
    // 显示配对信息
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("配对码:");
    display.println(passkey);
    display.println("等待iPhone连接...");
    display.display();
}

void loop() {
    ancs.loop();
}








API文档

NodeLib_ESP32_ANCS 类

构造函数

NodeLib_ESP32_ANCS ancs;
公共方法

begin(const char* deviceName = "ESP32-ANCS")
初始化ANCS客户端
deviceName: 蓝牙设备名称
loop()
主循环函数，必须在loop()中调用

setNotificationCallback(ANCSNotificationCallback cb)
设置通知接收回调
cb: 回调函数，类型为void (*)(uint32_t uid, const char* title, const char* message)
setEventCallback(ANCSEventCallback cb)
设置事件接收回调
cb: 回调函数，类型为void (*)(uint8_t eventId, uint32_t uid)
bool isConnected()
检查是否连接到iOS设备
返回: true如果已连接
bool isRunning()
检查ANCS是否正在运行
返回: true如果处于运行状态
String getPasskey()
获取当前配对码
返回: 6位数字配对码字符串
回调函数类型

// 通知回调
typedef void (*ANCSNotificationCallback)(uint32_t uid, const char* title, const char* message);
// uid: 通知唯一标识符
// title: 通知标题
// message: 通知内容

// 事件回调
typedef void (*ANCSEventCallback)(uint8_t eventId, uint32_t uid);
// eventId: 事件类型 (0=添加, 1=修改, 2=移除)
// uid: 通知唯一标识符


高级用法

1. 发送通知到服务器

#include <WiFi.h>
#include <HTTPClient.h>
#include <NodeLib_ESP32_ANCS.h>

NodeLib_ESP32_ANCS ancs;
const char* webhookUrl = "https://your-webhook.url";

void sendToServer(const char* title, const char* message) {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin(webhookUrl);
        http.addHeader("Content-Type", "application/json");
        
        String jsonPayload = "{\"title\":\"" + String(title) + 
                            "\",\"message\":\"" + String(message) + "\"}";
        
        int httpCode = http.POST(jsonPayload);
        if (httpCode > 0) {
            Serial.printf("HTTP响应代码: %d\n", httpCode);
        }
        http.end();
    }
}

void onNotificationReceived(uint32_t uid, const char* title, const char* message) {
    Serial.printf("收到通知: %s - %s\n", title, message);
    sendToServer(title, message);
}


2. 多设备支持

// 可以创建多个实例连接不同设备
NodeLib_ESP32_ANCS ancsPhone;
NodeLib_ESP32_ANCS ancsPad;

void setup() {
    ancsPhone.begin("ESP32-Phone");
    ancsPad.begin("ESP32-Pad");
}
故障排除

常见问题

1. 编译错误
错误: fatal error: BLEDevice.h: No such file or directory
解决: 安装ESP32 BLE Arduino库
错误: esp_bt.h: No such file or directory
解决: 确保选择了正确的ESP32开发板
2. 连接问题
无法连接iPhone
检查iPhone蓝牙是否开启
在iPhone上"忘记此设备"，重新连接
重启ESP32和iPhone蓝牙
配对失败
确保输入正确的6位配对码
检查配对码在串口监视器中的显示
3. 收不到通知
iPhone设置 > 通知 > 确保应用通知已开启
iPhone设置 > 通知 > 应用 > 显示预览 > 始终
确保iPhone已解锁
发送测试通知给自己
4. 中文乱码
确保串口监视器设置为UTF-8编码
Arduino IDE: 文件 > 首选项 > 启用"代码UTF-8编码"
调试信息

库会输出调试信息到串口：
>> [STATE]- 状态机状态变化
>> [PAIRING]- 配对相关信息
>> [NOTIF]- 收到通知事件
>> [MSG]- 完整通知内容

iOS设置说明
启用蓝牙: 设置 > 蓝牙 > 开启
连接设备: 在蓝牙设备列表中找到ESP32设备
输入配对码: 出现提示时输入串口显示的6位数字
启用通知预览:
设置 > 通知
选择应用 (如信息、微信等)
点击进入
开启"允许通知"
选择"显示预览" > "始终"
测试: 向自己发送测试消息

遇到问题尝试重启板子或者开关手机蓝牙

许可证

MIT License

v1.0.0 (2026-01-29)
初始版本发布
支持ANCS v1.0
随机6位配对码
中文和Emoji支持
基本示例程序

注意: 本库仍在开发中，API可能会有变化。建议定期检查更新。

致谢
Apple Notification Center Service 规范
ESP32 BLE Arduino库
所有贡献者和测试者
本库与Apple Inc.无关，ANCS是Apple Inc.的商标。
