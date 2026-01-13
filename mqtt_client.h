#pragma once
#include <Arduino.h>

void mqttInit(
  const char* host,
  uint16_t port,
  const char* username,
  const char* password,
  const String& deviceId
);

void mqttTick(int connState);
bool mqttConsumeFeedNow();
void mqttPublishOfflineNow();

void mqttPublishActivityState(const char* state, bool retain = true);
void mqttPublishActivityEvent(const char* eventName, bool retain = false);
