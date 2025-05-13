#include <Arduino.h>
#include <WiFi.h>
#include <ConfigPortal32.h>
#include <TFT_eSPI.h>
#include <qrcode_eSPI.h>
#include <HTTPClient.h>

char*        ssid_pfix        = (char*)"MyRelayWeb";
String       user_config_html = "";

// ————————————————
//   핀 정의 (Lab4)
// ————————————————
#define PUSH_BUTTON 43     // 버튼 핀
#define TFT_BL      38     // TFT 백라이트 핀

// ————————————————
//   객체 선언
// ————————————————
TFT_eSPI     display = TFT_eSPI();
QRcode_eSPI  qrcode(&display);

volatile bool        pressedFlag = false;
volatile unsigned long lastPress  = 0;
String               relayIP;
String               toggleURL;

IRAM_ATTR void buttonISR() {
  unsigned long now = millis();
  if (now - lastPress > 200) {
    lastPress   = now;
    pressedFlag = true;
  }
}

void setup() {
  Serial.begin(115200);

  Serial.println("Toggle URL: " + toggleURL);

  // 백라이트 ON
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  // TFT + QR 초기화
  display.begin();
  display.setRotation(1);
  display.fillScreen(TFT_WHITE);
  qrcode.init();

  // ConfigPortal 설정
  loadConfig();
  if (!cfg.containsKey("config") || strcmp((const char*)cfg["config"], "done")) {
    // AP 모드
    WiFi.mode(WIFI_AP);
    delay(200);

    // AP SSID 계산
    uint8_t mac[6];
    WiFi.softAPmacAddress(mac);
    char macStr[7];
    sprintf(macStr, "%02X%02X%02X", mac[3], mac[4], mac[5]);
    String apSSID = String(ssid_pfix) + "-" + macStr;
    String apPW   = apSSID;
    String qrData = "WIFI:S:" + apSSID + ";T:WPA;P:" + apPW + ";;";

    // QR 생성
    char buf[128];
    qrData.toCharArray(buf, sizeof(buf));
    display.fillScreen(TFT_WHITE);
    qrcode.create(buf);
    Serial.println("Show QR: " + qrData);

    // 설정 포털 실행
    configDevice();
  }

  // 3) AP 모드 띄우고 SSID/PW QR 생성
  WiFi.mode(WIFI_AP);
  delay(200);
  uint8_t mac[6];
  WiFi.softAPmacAddress(mac);
  char macStr[7];
  sprintf(macStr, "%02X%02X%02X", mac[3], mac[4], mac[5]);
  String apSSID = String(ssid_pfix) + "-" + macStr;
  String qrData = "WIFI:S:" + apSSID + ";T:WPA;P:" + apSSID + ";;";

  char buf[128];
  qrData.toCharArray(buf, sizeof(buf));
  display.fillScreen(TFT_WHITE);
  qrcode.create(buf);
  Serial.println("Show QR: " + qrData);

  // relayIP → toggleURL
  relayIP   = String(cfg["meta"]["relayIP"].as<const char*>());
  toggleURL = "http://" + relayIP + "/toggle";
  Serial.println("Toggle URL: " + toggleURL);

  // 버튼 인터럽트 설정
  pinMode(PUSH_BUTTON, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PUSH_BUTTON), buttonISR, FALLING);
}

void loop() {
  if (pressedFlag) {
    pressedFlag = false;
    Serial.println("Button pressed → calling toggle");

    HTTPClient http;
    if (http.begin(toggleURL)) {
      int code = http.GET();
      Serial.printf("GET /toggle → %d\n", code);
      http.end();
    } else {
      Serial.println("HTTPClient begin failed");
    }
  }
}
