#include <WiFi.h>
#include <WebServer.h>
#include <Servo.h>

const int motor_pin1 = D2;
const int motor_pin2 = D3;
const int motor_button = D7;

const int servo_pin = D6;
Servo servo;

WebServer server(80);

const char* AP_SSID = "SmartFeeder_Setup";
const char* AP_PASS = "12345678";

String buildWifiOptionsHtml() {
  int n = WiFi.scanNetworks(false, false);
  String opt = "";

  if (n <= 0) {
    opt += "<option value=''>스캔된 Wi-Fi 없음</option>";
    return opt;
  }

  for (int i = 0; i < n; i++) {
    String ssid = WiFi.SSID(i);
    ssid.trim();
    if (ssid.length() == 0) continue;

    opt += "<option value='";
    opt += ssid;
    opt += "'>";
    opt += ssid;
    opt += "</option>";
  }

  if (opt.length() == 0) {
    opt += "<option value=''>스캔된 Wi-Fi 없음</option>";
  }

  return opt;
}

String pageHtml() {
  String options = buildWifiOptionsHtml();

  String h;
  h += "<!doctype html><html><head><meta charset='utf-8'/>";
  h += "<meta name='viewport' content='width=device-width, initial-scale=1'/>";
  h += "<title>SmartFeeder WiFi</title>";
  h += "<style>";
  h += "select,input{width:100%;box-sizing:border-box;padding:10px;margin:8px 0;font-size:16px;}";
  h += "button{width:100%;box-sizing:border-box;padding:12px 14px;margin-top:10px;font-size:16px;}";
  h += "body{font-family:sans-serif;padding:18px;}";
  h += "</style></head><body>";
  h += "<h2>Wi-Fi 설정</h2>";
  h += "<form method='POST' action='/save'>";
  h += "<label>Wi-Fi 선택</label><br/>";
  h += "<select name='ssid'>";
  h += options;
  h += "</select>";
  h += "<label>비밀번호</label><br/>";
  h += "<input name='pass' type='password'/>";
  h += "<button type='submit'>확인</button>";
  h += "</form>";
  h += "<p style='margin-top:14px;'><a href='/'>다시 스캔</a></p>";
  h += "</body></html>";
  return h;
}

void startApAndWeb() {
  WiFi.mode(WIFI_MODE_APSTA);
  WiFi.softAP(AP_SSID, AP_PASS);

  IPAddress ip = WiFi.softAPIP();

  server.on("/", []() {
    server.send(200, "text/html", pageHtml());
  });

  server.on("/save", []() {
    if (server.method() != HTTP_POST) {
      server.send(405, "text/plain", "Method Not Allowed");
      return;
    }

    String ssid = server.arg("ssid");
    String pass = server.arg("pass");
    ssid.trim();
    pass.trim();

    Serial.println("=== WIFI INPUT ===");
    Serial.print("SSID: ");
    Serial.println(ssid);
    Serial.print("PASS: ");
    Serial.println(pass);

    server.sendHeader("Location", "/");
    server.send(303);
  });

  server.begin();

  Serial.print("AP SSID: ");
  Serial.println(AP_SSID);
  Serial.print("Open: http://");
  Serial.println(ip);
}

void setup() {
  pinMode(motor_pin1, OUTPUT);
  pinMode(motor_pin2, OUTPUT);
  pinMode(motor_button, INPUT);

  digitalWrite(motor_pin1, LOW);
  digitalWrite(motor_pin2, LOW);

  servo.attach(servo_pin);
  servo.write(180);

  Serial.begin(115200);
  delay(800);

  startApAndWeb();
}

void loop() {
  server.handleClient();

  if (digitalRead(motor_button) == LOW) {
    Serial.println("pressed");

    servo.write(50);
    delay(300);

    digitalWrite(motor_pin1, HIGH);
    digitalWrite(motor_pin2, HIGH);
    delay(1000);

    digitalWrite(motor_pin1, LOW);
    digitalWrite(motor_pin2, LOW);
    delay(300);

    servo.write(180);
    delay(500);

    delay(300);
  }
}
