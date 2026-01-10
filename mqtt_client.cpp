#include "mqtt_client.h"

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

static WiFiClientSecure g_secureClient;
static PubSubClient g_mqtt(g_secureClient);

static String g_deviceId = "";
static String g_username = "";
static String g_password = "";

static unsigned long g_lastTryMs = 0;
static const unsigned long MQTT_RETRY_MS = 3000;

static volatile bool g_feedNowRequested = false;

static String statusTopic() {
  return String("feeder/") + g_deviceId + "/status";
}

static String lastTopicSegment(const char* topic) {
  String t(topic);
  int idx = t.lastIndexOf('/');
  if (idx < 0) return t;
  return t.substring(idx + 1);
}

static void onMqttMessage(char* topic, byte* payload, unsigned int length) {
  String p;
  p.reserve(length);
  for (unsigned int i = 0; i < length; i++) p += (char)payload[i];

  const String action = lastTopicSegment(topic);

  Serial.print("mqtt rx action=");
  Serial.print(action);
  Serial.print(" payload=");
  Serial.println(p);

  if (action == "feed_button") {
    g_feedNowRequested = true;
  }
}

void mqttInit(
  const char* host,
  uint16_t port,
  const char* username,
  const char* password,
  const String& deviceId
) {
  g_deviceId = deviceId;
  g_username = username ? String(username) : String("");
  g_password = password ? String(password) : String("");

  g_secureClient.setInsecure();

  g_mqtt.setServer(host, port);
  g_mqtt.setCallback(onMqttMessage);
}

static void ensureConnected() {
  if (WiFi.status() != WL_CONNECTED) return;
  if (g_mqtt.connected()) return;

  const unsigned long now = millis();
  if (now - g_lastTryMs < MQTT_RETRY_MS) return;
  g_lastTryMs = now;

  const String clientId = String("sf-") + g_deviceId;

  const String willTopic = statusTopic();
  const char* willPayload = "offline";
  const int willQos = 1;
  const bool willRetain = true;

  bool ok = false;
  if (g_username.length() > 0) {
    ok = g_mqtt.connect(
      clientId.c_str(),
      g_username.c_str(),
      g_password.c_str(),
      willTopic.c_str(),
      willQos,
      willRetain,
      willPayload
    );
  } else {
    ok = g_mqtt.connect(
      clientId.c_str(),
      willTopic.c_str(),
      willQos,
      willRetain,
      willPayload
    );
  }

  if (!ok) {
    Serial.println("mqtt connect failed");
    return;
  }

  const String st = statusTopic();
  g_mqtt.publish(st.c_str(), "online", true);

  const String sub = String("feeder/") + g_deviceId + "/#";
  g_mqtt.subscribe(sub.c_str());
  Serial.println("mqtt connected");
}

void mqttTick(int connState) {
  if (connState != 2) return;

  ensureConnected();

  if (g_mqtt.connected()) {
    g_mqtt.loop();
  }
}

bool mqttConsumeFeedNow() {
  if (g_feedNowRequested) {
    g_feedNowRequested = false;
    return true;
  }
  return false;
}

void mqttPublishOfflineNow() {
  if (!g_mqtt.connected()) return;

  const String st = statusTopic();
  g_mqtt.publish(st.c_str(), "offline", true);
  g_mqtt.disconnect();
}
