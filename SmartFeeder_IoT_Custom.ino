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

String g_targetSsid = "";
String g_targetPass = "";
bool g_connectRequested = false;
bool g_connecting = false;
unsigned long g_connectStartMs = 0;

String wifiStatusText(wl_status_t st) {
  switch (st) {
    case WL_IDLE_STATUS:
      return "IDLE";
    case WL_NO_SSID_AVAIL:
      return "NO_SSID";
    case WL_SCAN_COMPLETED:
      return "SCAN_DONE";
    case WL_CONNECTED:
      return "CONNECTED";
    case WL_CONNECT_FAILED:
      return "FAILED";
    case WL_CONNECTION_LOST:
      return "LOST";
    case WL_DISCONNECTED:
      return "DISCONNECTED";
    default:
      return "UNKNOWN";
  }
}

String buildWifiOptionsHtml() {
  WiFi.scanDelete();
  delay(50);

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

String connectingHtml() {
  String h;
  h += "<!doctype html><html><head><meta charset='utf-8'/>";
  h += "<meta name='viewport' content='width=device-width, initial-scale=1'/>";
  h += "<title>Connecting...</title>";
  h += "<style>";
  h += "body{font-family:sans-serif;padding:18px;}";
  h += ".box{padding:14px;border:1px solid #ddd;border-radius:10px;}";
  h += "</style></head><body>";
  h += "<h2>연결 중...</h2>";
  h += "<div class='box'>";
  h += "<p>선택한 Wi-Fi로 접속을 시도하고 있어요.</p>";
  h += "<p>연결되면 AP 모드가 꺼지고, 기기는 공유기 네트워크로 이동합니다.</p>";
  h += "<p id='st'>상태 확인 중...</p>";
  h += "</div>";
  h += "<script>";
  h += "async function tick(){";
  h += "  try{";
  h += "    const r=await fetch('/status',{cache:'no-store'});";
  h += "    const j=await r.json();";
  h += "    document.getElementById('st').innerText='status: '+j.status+' | ip: '+(j.ip||'-')+' | ap: '+j.ap;";
  h += "  }catch(e){";
  h += "    document.getElementById('st').innerText='상태 확인 실패(연결 전환 중일 수 있어요)';";
  h += "  }";
  h += "}";
  h += "setInterval(tick, 1000); tick();";
  h += "</script>";
  h += "</body></html>";
  return h;
}

void beginStaConnect(const String& ssid, const String& pass) {
  g_connecting = true;
  g_connectStartMs = millis();

  Serial.println("=== WIFI CONNECT START ===");
  Serial.print("Target SSID: ");
  Serial.println(ssid);

  WiFi.mode(WIFI_MODE_APSTA);
  WiFi.disconnect(true);
  WiFi.scanDelete();
  delay(200);

  WiFi.begin(ssid.c_str(), pass.c_str());
}

void startApAndWeb() {
  WiFi.mode(WIFI_MODE_APSTA);
  WiFi.disconnect(true);
  WiFi.scanDelete();
  delay(200);

  WiFi.softAP(AP_SSID, AP_PASS);

  IPAddress ip = WiFi.softAPIP();

  server.on("/", []() {
    server.send(200, "text/html", pageHtml());
  });

  server.on("/status", []() {
    String ipStr = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : "";
    String apStr = (WiFi.getMode() == WIFI_MODE_AP || WiFi.getMode() == WIFI_MODE_APSTA) ? "on" : "off";

    String json = "{";
    json += "\"status\":\"";
    json += wifiStatusText(WiFi.status());
    json += "\",";
    json += "\"ip\":\"";
    json += ipStr;
    json += "\",";
    json += "\"ap\":\"";
    json += apStr;
    json += "\"";
    json += "}";

    server.send(200, "application/json", json);
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

    if (ssid.length() == 0) {
      server.send(400, "text/plain", "SSID is empty");
      return;
    }

    Serial.println("=== WIFI INPUT ===");
    Serial.print("SSID: ");
    Serial.println(ssid);
    Serial.print("PASS: ");
    Serial.println(pass);

    g_targetSsid = ssid;
    g_targetPass = pass;
    g_connectRequested = true;

    server.send(200, "text/html", connectingHtml());
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

  if (g_connectRequested && !g_connecting) {
    g_connectRequested = false;
    beginStaConnect(g_targetSsid, g_targetPass);
  }

  if (g_connecting) {
    if (WiFi.status() == WL_CONNECTED) {
      IPAddress staIp = WiFi.localIP();

      Serial.println("=== WIFI CONNECTED ===");
      Serial.print("STA IP: ");
      Serial.println(staIp);
      Serial.print("CONNECTED SSID: ");
      Serial.println(WiFi.SSID());
      Serial.print("BSSID: ");
      Serial.println(WiFi.BSSIDstr());

      WiFi.softAPdisconnect(true);
      WiFi.mode(WIFI_MODE_STA);

      Serial.println("=== AP OFF / STA ONLY ===");
      Serial.print("Now open: http://");
      Serial.println(staIp);

      g_connecting = false;
    } else {
      if (millis() - g_connectStartMs > 60000) {
        Serial.println("=== WIFI CONNECT TIMEOUT ===");
        Serial.print("Status: ");
        Serial.println(wifiStatusText(WiFi.status()));

        g_connecting = false;

        WiFi.disconnect(true);
        WiFi.scanDelete();
        delay(200);

        WiFi.mode(WIFI_MODE_APSTA);
        WiFi.softAP(AP_SSID, AP_PASS);

        Serial.println("=== AP RESTORED ===");
        Serial.print("AP IP: http://");
        Serial.println(WiFi.softAPIP());
      }
    }
  }

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
