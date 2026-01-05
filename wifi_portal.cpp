#include "wifi_portal.h"

static WebServer server(80);
static bool started = false;

static WifiPortalBindings B;
static const char* AP_SSID = nullptr;
static const char* AP_PASS = nullptr;

static String buildWifiOptionsHtml() {
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

static String pageHtml(const String& bannerText, bool isError) {
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

static String connectingHtml() {
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

static String doneHtml() {
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
  if (B.connectedSsid && B.connectedSsid->length() > 0) h += *B.connectedSsid;
  else h += "-";
  h += "</div>";
  h += "</div>";
  h += "<script>";
  h += "try{";
  h += "  if(window.WifiSetup && WifiSetup.postMessage){";
  h += "    WifiSetup.postMessage(JSON.stringify({type:'wifi_done', deviceId:'";
  if (B.deviceId) h += *B.deviceId;
  h += "'}));";
  h += "  }";
  h += "}catch(e){}";
  h += "</script>";
  h += "</body></html>";
  return h;
}

static String failHtml() {
  String msg = (B.errorMsg ? *B.errorMsg : String(""));
  if (msg.length() == 0) msg = "연결에 실패했어요. 다시 선택하고 비밀번호를 입력해 주세요.";
  return pageHtml(msg, true);
}

bool wifiPortalIsStarted() {
  return started;
}

void wifiPortalHandleClient() {
  if (!started) return;
  server.handleClient();
}

void wifiPortalStart(const char* apSsid, const char* apPass, const WifiPortalBindings& b) {
  if (started) return;

  AP_SSID = apSsid;
  AP_PASS = apPass;
  B = b;

  WiFi.mode(WIFI_MODE_APSTA);
  WiFi.disconnect(false, true);
  WiFi.scanDelete();
  delay(200);

  WiFi.softAP(AP_SSID, AP_PASS);
  IPAddress ip = WiFi.softAPIP();

  server.on("/", []() {
    String banner = "";
    bool isError = false;

    if (B.connState && *B.connState == 3) {
      if (B.errorMsg && B.errorMsg->length() > 0) banner = *B.errorMsg;
      else banner = "연결에 실패했어요. 다시 시도해 주세요.";
      isError = true;
    } else if (B.connState && *B.connState == 2) {
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
    int done = (B.connState && *B.connState == 2) ? 1 : 0;
    int fail = (B.connState && *B.connState == 3) ? 1 : 0;

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
      if (B.connState) *B.connState = 3;
      if (B.errorMsg) *B.errorMsg = "Wi-Fi를 선택해 주세요.";
      server.send(200, "text/html", failHtml());
      return;
    }

    Serial.println("=== WIFI INPUT ===");
    Serial.print("SSID: ");
    Serial.println(ssid);
    Serial.print("PASS: ");
    Serial.println(pass);

    if (B.targetSsid) *B.targetSsid = ssid;
    if (B.targetPass) *B.targetPass = pass;

    if (B.isAutoAttempt) *B.isAutoAttempt = false;
    if (B.connectRequested) *B.connectRequested = true;
    if (B.apOffDone) *B.apOffDone = false;

    server.send(200, "text/html", connectingHtml());
  });

  server.begin();
  started = true;

  Serial.println("=== AP SETUP MODE ON ===");
  Serial.print("AP SSID: ");
  Serial.println(AP_SSID);
  Serial.print("Open: http://");
  Serial.println(ip);
}
