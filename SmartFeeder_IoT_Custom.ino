#include <WiFi.h>
#include <Servo.h>

#include "wifi_portal.h"
#include "wifi_store.h"
#include "feed_control.h"
#include "mqtt_client.h"

int motor_pin1 = D2;
int motor_pin2 = D3;
int motor_button = D7;

const int servo_pin = D6;
Servo servo;

const char* AP_SSID = "SmartFeeder_Setup";
const char* AP_PASS = "12345678";

const char* MQTT_HOST = "d1229804bb2a42cd87dedd808119a65b.s1.eu.hivemq.cloud";
const uint16_t MQTT_PORT = 8883;

const char* MQTT_USER = "Oowni";
const char* MQTT_PASS = "Inwoo0203!@";

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

bool g_isAutoAttempt = false;

String g_deviceId = "";

uint8_t g_feedMethodCode = FEED_METHOD_FEED_BUTTON;

String makeDeviceId() {
  return "SF-" + String((uint32_t)ESP.getEfuseMac(), HEX);
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

void beginStaConnect(const String& ssid, const String& pass) {
  mqttPublishOfflineNow();

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

WifiPortalBindings makeWifiPortalBindings() {
  WifiPortalBindings b;
  b.connState = &g_connState;
  b.errorMsg = &g_errorMsg;
  b.connectedSsid = &g_connectedSsid;
  b.deviceId = &g_deviceId;
  b.targetSsid = &g_targetSsid;
  b.targetPass = &g_targetPass;
  b.connectRequested = &g_connectRequested;
  b.isAutoAttempt = &g_isAutoAttempt;
  b.apOffDone = &g_apOffDone;
  return b;
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

  mqttInit(MQTT_HOST, MQTT_PORT, MQTT_USER, MQTT_PASS, g_deviceId);

  String ssid, pass;
  bool hasSaved = wifiStoreLoad(ssid, pass);

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
    wifiPortalStart(AP_SSID, AP_PASS, makeWifiPortalBindings());
  }
}

void loop() {
  wifiPortalHandleClient();

  if (g_connectRequested && !g_connecting) {
    g_connectRequested = false;
    beginStaConnect(g_targetSsid, g_targetPass);
  }

  if (g_connecting) {
    if (WiFi.status() == WL_CONNECTED) {
      g_connectedSsid = WiFi.SSID();
      g_bssidStr = WiFi.BSSIDstr();

      wifiStoreSave(g_targetSsid, g_targetPass);

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

        if (!wifiPortalIsStarted()) {
          Serial.println("Auto connect failed. Starting AP setup mode.");
          wifiPortalStart(AP_SSID, AP_PASS, makeWifiPortalBindings());
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

  mqttTick(g_connState);

  if (mqttConsumeFeedNow()) {
    feedButtonRunNow();
  }

  feedMethodTick(g_feedMethodCode);
}
