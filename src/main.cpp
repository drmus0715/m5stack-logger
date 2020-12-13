#define M5STACK_MPU6886
#include <M5Stack.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <DNSServer.h>
#include <WebSocketsServer.h> // arduinoWebSocketsライブラリ
#include <elapsedMillis.h>    // elapsedMillisライブラリ
#include <SPIFFS.h>
#include "index_html.h" // web server root index
#include "bmm150.h"
#include "bmm150_defs.h"

//無線LANの設定　アクセスポイントのSSIDとパスワード
const char *ap_ssid = "ESP32AP";      // APのSSID
const char *ap_password = "12345678"; // APのパスワード 8文字以上
IPAddress ip(192, 168, 1, 100);
IPAddress subnet(255, 255, 255, 0);
const byte DNS_PORT = 53;
DNSServer dnsServer;
// Webサーバー 192.168.x.x:80
WebServer server(80);
// Websocketサーバー 192.68.x.x:81
WebSocketsServer webSocket = WebSocketsServer(81); // 81番ポート

// サンプリング周期
elapsedMillis sensorElapsed;
const unsigned long DELAY = 100; // ms

// Webコンテンツのイベントハンドラ
void handleRoot() {
    String s = INDEX_HTML; // index_html.hより読み込み
    server.send(200, "text/html", s);
}
void handleNotFound() { server.send(404, "text/plain", "File not found."); }

void displayLcd(float gyroX, float gyroY, float gyroZ, float accX, float accY,
                float accZ, float geoX, float geoY, float geoZ) {
    float pitch = 0.0F;
    float roll = 0.0F;
    float yaw = 0.0F;
    M5.IMU.getAhrsData(&pitch, &roll, &yaw);

    M5.Lcd.setCursor(0, 0);
    M5.Lcd.printf("%6.2f  %6.2f  %6.2f o/s", gyroX, gyroY, gyroZ);
    M5.Lcd.setCursor(0, 22);
    M5.Lcd.printf(" %5.2f   %5.2f   %5.2f   G", accX, accY, accZ);
    // M5.Lcd.print(" o/s");
    M5.Lcd.setCursor(0, 45);
    // M5.Lcd.printf(" %5.2f   %5.2f   %5.2f   G", accX, accY, accZ);
    M5.Lcd.printf(" %5.2f   %5.2f   %5.2f  uT", geoX, geoY, geoZ);
    M5.Lcd.setCursor(0, 70);
    M5.Lcd.printf(" %5.2f   %5.2f   %5.2f   ", pitch, roll, yaw);
    // M5.Lcd.print(" G");
    M5.Lcd.setCursor(220, 92);
    // M5.Lcd.printf(" %5.2f   %5.2f   %5.2f  uT", geoX, geoY, geoZ);
    M5.Lcd.print(" degree");
    // M5.Lcd.setCursor(220, 112);
    // M5.Lcd.print(" uT");
    M5.Lcd.setCursor(0, 130);
    // M5.Lcd.printf(" %5.2f   %5.2f   %5.2f   ", pitch, roll, yaw);
    M5.Lcd.printf("USB-UART: baud: 115200");
    M5.Lcd.setCursor(0, 152);
    // M5.Lcd.print(" degree");
    // M5.Lcd.printf("Temperature : %.2f C", temp);
    M5.Lcd.printf("WiFi AP: SSID:%s", ap_ssid);
    M5.Lcd.setCursor(0, 170);
    M5.Lcd.printf("         PASS:%s", ap_password);
    M5.Lcd.setCursor(0, 192);
    M5.Lcd.printf("Webserver: %s", WiFi.softAPIP().toString().c_str());
    M5.Lcd.setCursor(0, 214);
}

// センサのデータ(JSON形式)
const char SENSOR_JSON[] PROGMEM =
    R"=====({"Accelerometer":{"x":%.2f,"y":%.2f,"z":%.2f},"Gyro":{"x":%.2f,"y":%.2f,"z":%.2f},"Geomagnetic":{"x":%.2f,"y":%.2f,"z":%.2f}})=====";

// データの更新
void sensor_loop() {

    float acc[3] = {};
    float gyro[3] = {};
    float geo[3] = {};
    char payload[1024];

    // Get Sensor value
    M5.IMU.getGyroData(&gyro[0], &gyro[1], &gyro[2]);
    M5.IMU.getAccelData(&acc[0], &acc[1], &acc[2]);
    // TODO: 地磁気センサの読み出しを追加したい

    displayLcd(gyro[0], gyro[1], gyro[2], acc[0], acc[1], acc[2], geo[0],
               geo[1], geo[2]);

    // float temp = (float)random(0, 100);
    //=============================================
    // (4) センシング
    snprintf_P(payload, sizeof(payload), SENSOR_JSON, acc[0], acc[1], acc[2],
               gyro[0], gyro[1], gyro[2], geo[0], geo[1], geo[2]);
    //=============================================

    // WebSocketでデータ送信(全端末へブロードキャスト)
    webSocket.broadcastTXT(payload, strlen(payload));
    Serial.println(payload);
}

void setup() {
    // Initialize the M5Stack object
    M5.begin();
    /*
      Power chip connected to gpio21, gpio22, I2C device
      Set battery charging voltage and current
      If used battery, please call this function in your project
    */
    M5.Power.begin();
    M5.IMU.Init();
    M5.Speaker.begin(); // これが無いとmuteしても無意味です。
    M5.Speaker.mute();
    M5.Lcd.setBrightness(100);
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextColor(GREEN, BLACK);
    M5.Lcd.setTextSize(2);

    // Add Serial Communication
    Serial.begin(115200);

    /**
     * WiFi AP Mode Issue
     * WiFi-AP modeを使用したとき、コアパニックが発生する場合があります。
     * https://github.com/espressif/arduino-esp32/issues/2025
     * https://github.com/espressif/arduino-esp32/issues/4288
     * WiFi.persistent(false)を追加し、Flashを削除する必要があります。
     */
    WiFi.persistent(false);
    //無線LAN接続APモード
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(ip, ip, subnet);
    WiFi.softAP(ap_ssid, ap_password);
    dnsServer.start(DNS_PORT, "*", ip);

    // Webサーバーのコンテンツ設定
    // favicon.ico, Chart.min.jsは dataフォルダ内に配置
    SPIFFS.begin();
    server.serveStatic("/favicon.ico", SPIFFS, "/favicon.ico");
    server.serveStatic("/Chart.min.js", SPIFFS, "/Chart.min.js");
    server.serveStatic("/graph.html", SPIFFS, "/graph.html");
    server.on("/", handleRoot);
    server.onNotFound(handleNotFound);
    server.begin();

    // WebSocketサーバー開始
    webSocket.begin();
}

void loop() {
    dnsServer.processNextRequest();
    server.handleClient();
    webSocket.loop();

    // 一定の周期でセンシング
    if (sensorElapsed > DELAY) {
        sensorElapsed = 0;
        sensor_loop();
    }
}