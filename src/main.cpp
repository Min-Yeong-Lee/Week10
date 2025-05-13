#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ConfigPortal32.h>
#include <WebServer.h>

// 강제 LilyGo T-Display S3 설정 로드
#define USER_SETUP_LOADED
#include <User_Setups/Setup206_LilyGo_T_Display_S3.h>

#include <TFT_eSPI.h>
#include <qrcode_eSPI.h>

// ConfigPortal32에 필요한 HTML 입력 폼 (meta.relayIP)
String user_config_html = "<p><input type='text' name='meta.relayIP' placeholder='RelayWeb Address'>";

// 설정 파트
char* ssid_pfix = (char*)"MyRelayWeb";  // ConfigPortal AP SSID prefix
char  relayIP[50];                      // relay 제어 IP 저장
#define RELAY       15                  // 릴레이 제어 핀
#define TFT_BL      38                  // TFT 백라이트 핀
#define PUSH_BUTTON 43                  // 물리 버튼 핀

// 전역 객체
WebServer    server(80);
TFT_eSPI     display = TFT_eSPI();
QRcode_eSPI  qrcode(&display);

// 버튼 디바운스 인터럽트 변수
volatile bool        buttonPressed = false;
volatile unsigned long lastPressed   = 0;
IRAM_ATTR void pressed() {
  unsigned long now = millis();
  if (now - lastPressed > 200) {
    lastPressed = now;
    buttonPressed = true;
  }
}

// 간단한 웹 UI
const char* htmlPage =
  "<!DOCTYPE html><html><head><meta charset='utf-8'><title>Relay Control</title></head>"
  "<body><h1>Relay Control</h1>"
  "<button onclick=\"fetch('/turnOn')\">ON</button>"
  "<button onclick=\"fetch('/turnOff')\">OFF</button>"
  "<button onclick=\"fetch('/toggle')\">TOGGLE</button>"
  "</body></html>";

void setup() {
  // 1) 시리얼 & TFT 백라이트 ON
  Serial.begin(115200);
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  delay(100);

  // 2) 설정 로드, 포털 모드 필요 시 QR 표시
  loadConfig();
  if (!cfg.containsKey("config") || strcmp((const char*)cfg["config"], "done") != 0) {
    // AP SSID 생성
    uint8_t mac[6]; WiFi.macAddress(mac);
    char macStr[7]; sprintf(macStr, "%02X%02X%02X", mac[3], mac[4], mac[5]);
    String apSSID = String(ssid_pfix) + "-" + macStr;

    // TFT 및 QR 초기화
    display.init();
    display.setRotation(1);
    display.fillScreen(TFT_WHITE);
    qrcode.init();

    // QR 데이터 (포털 접속용)
    String qrData = "WIFI:S:" + apSSID + ";T:WPA;P:" + apSSID + ";;"; // AP 비밀번호=SSID
    char qrBuf[128]; qrData.toCharArray(qrBuf, sizeof(qrBuf));
    qrcode.create(qrBuf);
    Serial.println("[ConfigPortal] QR code shown: " + qrData);

    // ConfigPortal 실행 (AP 띄우고 설정 대기)
    configDevice();
  }

  // 3) STA 모드로 Wi-Fi 연결
  WiFi.mode(WIFI_STA);
  WiFi.begin((const char*)cfg["ssid"], (const char*)cfg["w_pw"]);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print('.');
  }
  Serial.printf("\nWiFi connected, IP: %s\n", WiFi.localIP().toString().c_str());

  // 4) relayIP 읽기
  if (cfg.containsKey("meta") && cfg["meta"].containsKey("relayIP")) {
    strncpy(relayIP, (const char*)cfg["meta"]["relayIP"], sizeof(relayIP)-1);
    relayIP[sizeof(relayIP)-1] = '\0';
    Serial.printf("Relay IP: %s\n", relayIP);
  }

  // 5) 릴레이 & 버튼 핀 세팅
  pinMode(RELAY, OUTPUT);
  pinMode(PUSH_BUTTON, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PUSH_BUTTON), pressed, FALLING);

  // 6) HTTP 서버 & 웹 UI
  server.on("/",         [](){ server.send(200, "text/html", htmlPage); });
  server.on("/turnOn",   [](){ digitalWrite(RELAY, HIGH);  server.send(200, "text/plain", "Relay ON"); });
  server.on("/turnOff",  [](){ digitalWrite(RELAY, LOW);   server.send(200, "text/plain", "Relay OFF"); });
  server.on("/toggle",   [](){ digitalWrite(RELAY, !digitalRead(RELAY)); server.send(200, "text/plain", "Toggled"); });
  server.onNotFound(     [](){ server.send(404, "text/html", "<h1>404 Not Found</h1>"); });
  server.begin();

  // 7) mDNS (선택)
  if (MDNS.begin("AdvRelayWeb")) {
    Serial.println("mDNS responder started");
  }
}

void loop() {
  // 물리 버튼 눌림 감지 시 릴레이 토글
  if (buttonPressed) {
    buttonPressed = false;
    digitalWrite(RELAY, !digitalRead(RELAY));
    Serial.printf("Physical button toggled → %s\n", digitalRead(RELAY) ? "ON" : "OFF");
  }

  // 웹 요청 처리
  server.handleClient();
}
