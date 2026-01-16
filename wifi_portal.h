#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

struct WifiPortalBindings {
  int* connState;
  String* errorMsg;
  String* connectedSsid;
  String* deviceId;

  String* targetSsid;
  String* targetPass;

  bool* connectRequested;
  bool* isAutoAttempt;
  bool* apOffDone;
};

void wifiPortalStart(const char* apSsid, const char* apPass, const WifiPortalBindings& b);
void wifiPortalStop(bool apOff = false);
void wifiPortalRestart(const char* apSsid, const char* apPass, const WifiPortalBindings& b, bool apOffBeforeRestart = true);

bool wifiPortalIsStarted();
void wifiPortalHandleClient();
