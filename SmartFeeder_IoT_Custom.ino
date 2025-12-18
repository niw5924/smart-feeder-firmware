#include <WiFi.h>
#include <WebServer.h>
#include <Servo.h>
#include <Preferences.h>

const int motor_pin1 = D2;
const int motor_pin2 = D3;
const int motor_button = D7;

const int servo_pin = D6;
Servo servo;

WebServer server(80);
Preferences prefs;

const char* AP_SSID = "SmartFeeder_Setup";
const char* AP_PASS = "12345678";

String g_targetSsid = "";
String g_targetPass = "";

bool g_connectRequested = false;
bool g_connecting = false;

unsigned long g_connectStartMs = 0;
const unsigned long CONNECT_TIMEOUT_MS = 30000;

int g_connState = 0;

String g_connectedSsid = "";
String g_bssidStr = "";
String g_errorMsg = "";

unsigned long g_successAtMs = 0;
bool g_apOffDone = false;

bool g_serverStarted = false;
bool g_isAutoAttempt = false;

String g_deviceId = "";

String makeDeviceId() {
  return "SF-" + String((uint32_t)ESP.getEfuseMac(), HEX);
}

void saveWifiCreds(const String& ssid, const String& pass) {
  prefs.begin("wifi", false);
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  prefs.end();
}

bool loadWifiCreds(String& ssid, String& pass) {
  prefs.begin("wifi", true);
  ssid = prefs.getString("ssid", "");
  pass = prefs.getString("pass", "");
  prefs.end();
  return ssid.length() > 0;
}

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

String pageHtml(const String& bannerText, bool isError) {
  String options = buildWifiOptionsHtml();

  String h;
  h += "<!doctype html><html><head><meta charset='utf-8'/>";
  h += "<meta name='viewport' content='width=device-width, initial-scale=1'/>";
  h += "<title>SmartFeeder WiFi</title>";
  h += "<style>";
  h += "body{font-family:sans-serif;padding:18px;}";
  h += ".banner{padding:12px 12px;border-radius:10px;margin:0 0 14px 0;font-size:14px;line-height:1.4;}";
  h += ".ok{background:#eef7ff;border:1px solid #cfe6ff;color:#0b3b66;}";
  h += ".err{background:#fff1f1;border:1px solid #ffd0d0;color:#7a1111;}";
  h += "select,input{width:100%;box-sizing:border-box;padding:10px;margin:8px 0;font-size:16px;}";
  h += "button{width:100%;box-sizing:border-box;padding:12px 14px;margin-top:10px;font-size:16px;}";
  h += "a{color:#1a73e8;text-decoration:none;}";
  h += "</style></head><body>";
  h += "<h2>Wi-Fi 설정</h2>";

  if (bannerText.length() > 0) {
    h += "<div class='banner ";
    h += (isError ? "err" : "ok");
    h += "'>";
    h += bannerText;
    h += "</div>";
  }

  h += "<form method='POST' action='/save'>";
  h += "<label>Wi-Fi 선택</label><br/>";
  h += "<select name='ssid'>";
  h += options;
  h += "</select>";
  h += "<label>비밀번호</label><br/>";
  h += "<input name='pass' type='password'/>";
  h += "<button type='submit'>연결</button>";
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
  h += ".row{display:flex;align-items:center;gap:10px;}";
  h += ".spinner{width:22px;height:22px;border:3px solid #ddd;border-top-color:#1a73e8;border-radius:50%;animation:spin 1s linear infinite;}";
  h += "@keyframes spin{to{transform:rotate(360deg);}}";
  h += ".muted{color:#666;font-size:13px;line-height:1.45;}";
  h += "</style></head><body>";
  h += "<h2>연결 중</h2>";
  h += "<div class='box'>";
  h += "<div class='row'><div class='spinner'></div><div><b>Wi-Fi에 접속하고 있어요</b></div></div>";
  h += "<p class='muted'>잠시만 기다려 주세요. 연결이 완료되면 안내 화면으로 이동합니다.</p>";
  h += "</div>";
  h += "<script>";
  h += "async function tick(){";
  h += "  try{";
  h += "    const r=await fetch('/status',{cache:'no-store'});";
  h += "    const j=await r.json();";
  h += "    if(j.done===1){ location.href='/done'; }";
  h += "    if(j.fail===1){ location.href='/fail'; }";
  h += "  }catch(e){}";
  h += "}";
  h += "setInterval(tick, 800); tick();";
  h += "</script>";
  h += "</body></html>";
  return h;
}

String doneHtml() {
  String h;
  h += "<!doctype html><html><head><meta charset='utf-8'/>";
  h += "<meta name='viewport' content='width=device-width, initial-scale=1'/>";
  h += "<title>Done</title>";
  h += "<style>";
  h += "body{font-family:sans-serif;padding:18px;}";
  h += ".box{padding:14px;border:1px solid #ddd;border-radius:10px;}";
  h += ".ok{background:#eef7ff;border:1px solid #cfe6ff;border-radius:10px;padding:12px;margin:0 0 14px 0;color:#0b3b66;}";
  h += ".muted{color:#666;font-size:13px;line-height:1.55;}";
  h += ".mono{font-family:ui-monospace,SFMono-Regular,Menlo,Monaco,Consolas,monospace;font-size:13px;word-break:break-all;}";
  h += "</style></head><body>";
  h += "<h2>연결 완료</h2>";
  h += "<div class='ok'><b>Wi-Fi 연결이 완료됐어요.</b><br/>앱 화면으로 이동합니다.</div>";
  h += "<div class='box'>";
  h += "<div class='muted'>연결된 Wi-Fi</div>";
  h += "<div class='mono'>";
  h += (g_connectedSsid.length() > 0 ? g_connectedSsid : "-");
  h += "</div>";
  h += "</div>";
  h += "<script>";
  h += "try{";
  h += "  if(window.WifiSetup && WifiSetup.postMessage){";
  h += "    WifiSetup.postMessage(JSON.stringify({type:'wifi_done', deviceId:'";
  h += g_deviceId;
  h += "'}));";
  h += "  }";
  h += "}catch(e){}";
  h += "</script>";
  h += "</body></html>";
  return h;
}

String failHtml() {
  String msg = g_errorMsg;
  if (msg.length() == 0) msg = "연결에 실패했어요. 다시 선택하고 비밀번호를 입력해 주세요.";
  return pageHtml(msg, true);
}

void beginStaConnect(const String& ssid, const String& pass) {
  g_connecting = true;
  g_connectStartMs = millis();
  g_connState = 1;

  g_connectedSsid = "";
  g_bssidStr = "";
  g_errorMsg = "";

  Serial.println("=== WIFI CONNECT START ===");
  Serial.print("Target SSID: ");
  Serial.println(ssid);
  Serial.print("Mode: ");
  Serial.println(g_isAutoAttempt ? "AUTO(STORED)" : "MANUAL(WEB)");

  if (g_isAutoAttempt) {
    WiFi.mode(WIFI_MODE_STA);
  } else {
    WiFi.mode(WIFI_MODE_APSTA);
  }

  WiFi.disconnect(false, true);
  WiFi.scanDelete();
  delay(200);

  if (!g_isAutoAttempt) {
    WiFi.softAP(AP_SSID, AP_PASS);
  }

  WiFi.begin(ssid.c_str(), pass.c_str());
}

void restoreAp() {
  WiFi.disconnect(false, true);
  WiFi.scanDelete();
  delay(200);

  WiFi.mode(WIFI_MODE_APSTA);
  WiFi.softAP(AP_SSID, AP_PASS);

  Serial.println("=== AP RESTORED ===");
  Serial.print("AP IP: http://");
  Serial.println(WiFi.softAPIP());
}

void applyApOffStaOnly() {
  if (g_apOffDone) return;

  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_MODE_STA);

  Serial.println("=== AP OFF / STA ONLY ===");
  Serial.print("Now open: http://");
  Serial.println(WiFi.localIP());

  g_apOffDone = true;
}

void startApAndWeb() {
  WiFi.mode(WIFI_MODE_APSTA);
  WiFi.disconnect(false, true);
  WiFi.scanDelete();
  delay(200);

  WiFi.softAP(AP_SSID, AP_PASS);

  IPAddress ip = WiFi.softAPIP();

  server.on("/", []() {
    String banner = "";
    bool isError = false;

    if (g_connState == 3) {
      banner = g_errorMsg.length() > 0 ? g_errorMsg : "연결에 실패했어요. 다시 시도해 주세요.";
      isError = true;
    } else if (g_connState == 2) {
      banner = "이미 Wi-Fi에 연결되어 있어요. 필요하면 다시 설정할 수 있어요.";
      isError = false;
    }

    server.send(200, "text/html", pageHtml(banner, isError));
  });

  server.on("/fail", []() {
    server.send(200, "text/html", failHtml());
  });

  server.on("/done", []() {
    server.send(200, "text/html", doneHtml());
  });

  server.on("/status", []() {
    int done = (g_connState == 2) ? 1 : 0;
    int fail = (g_connState == 3) ? 1 : 0;

    String json = "{";
    json += "\"done\":";
    json += String(done);
    json += ",";
    json += "\"fail\":";
    json += String(fail);
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
      g_connState = 3;
      g_errorMsg = "Wi-Fi를 선택해 주세요.";
      server.send(200, "text/html", failHtml());
      return;
    }

    Serial.println("=== WIFI INPUT ===");
    Serial.print("SSID: ");
    Serial.println(ssid);
    Serial.print("PASS: ");
    Serial.println(pass);

    g_targetSsid = ssid;
    g_targetPass = pass;

    g_isAutoAttempt = false;
    g_connectRequested = true;
    g_apOffDone = false;

    server.send(200, "text/html", connectingHtml());
  });

  server.begin();
  g_serverStarted = true;

  Serial.println("=== AP SETUP MODE ON ===");
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

  g_deviceId = makeDeviceId();
  g_deviceId.toUpperCase();
  Serial.print("DEVICE ID: ");
  Serial.println(g_deviceId);

  String ssid, pass;
  bool hasSaved = loadWifiCreds(ssid, pass);

  Serial.println("=== BOOT ===");
  Serial.print("Saved SSID: ");
  Serial.println(hasSaved ? ssid : "-");
  Serial.print("Saved PASS LEN: ");
  Serial.println(hasSaved ? String(pass.length()) : String(0));

  if (hasSaved) {
    Serial.println("Saved Wi-Fi found. Trying STA auto connect.");
    g_targetSsid = ssid;
    g_targetPass = pass;
    g_apOffDone = true;
    g_isAutoAttempt = true;
    beginStaConnect(g_targetSsid, g_targetPass);
  } else {
    Serial.println("No saved Wi-Fi. Starting AP setup mode.");
    startApAndWeb();
  }
}

void loop() {
  if (g_serverStarted) {
    server.handleClient();
  }

  if (g_connectRequested && !g_connecting) {
    g_connectRequested = false;
    beginStaConnect(g_targetSsid, g_targetPass);
  }

  if (g_connecting) {
    if (WiFi.status() == WL_CONNECTED) {
      g_connectedSsid = WiFi.SSID();
      g_bssidStr = WiFi.BSSIDstr();

      saveWifiCreds(g_targetSsid, g_targetPass);

      Serial.println("=== WIFI CONNECTED ===");
      Serial.print("STA IP: ");
      Serial.println(WiFi.localIP());
      Serial.print("CONNECTED SSID: ");
      Serial.println(g_connectedSsid);
      Serial.print("BSSID: ");
      Serial.println(g_bssidStr);

      if (g_isAutoAttempt) {
        Serial.println("=== AUTO CONNECT SUCCESS ===");
      } else {
        Serial.println("=== MANUAL CONNECT SUCCESS ===");
      }

      g_isAutoAttempt = false;

      g_connecting = false;
      g_connState = 2;
      g_successAtMs = millis();
      g_errorMsg = "";
    } else {
      if (millis() - g_connectStartMs > CONNECT_TIMEOUT_MS) {
        Serial.println("=== WIFI CONNECT TIMEOUT ===");
        Serial.print("Status: ");
        Serial.println(wifiStatusText(WiFi.status()));

        g_connecting = false;
        g_connState = 3;
        g_errorMsg = "연결에 실패했어요. Wi-Fi/비밀번호를 확인하고 다시 시도해 주세요.";

        if (!g_serverStarted) {
          Serial.println("Auto connect failed. Starting AP setup mode.");
          startApAndWeb();
        } else {
          Serial.println("Connect failed. Restoring AP.");
          restoreAp();
        }

        g_isAutoAttempt = false;
      }
    }
  }

  if (g_connState == 2 && !g_apOffDone) {
    if (millis() - g_successAtMs > 8000) {
      applyApOffStaOnly();
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
