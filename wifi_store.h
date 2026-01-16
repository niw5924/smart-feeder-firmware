#pragma once
#include <Arduino.h>

bool wifiStoreLoad(String& ssid, String& pass);
void wifiStoreSave(const String& ssid, const String& pass);
bool wifiStoreClear();
