#pragma once
#include <Arduino.h>

enum FeedMethod : uint8_t {
  FEED_METHOD_FEED_BUTTON = 0,
  FEED_METHOD_ULTRASONIC = 1,
  FEED_METHOD_INTERVAL_TIMER = 2,
};

void feedMethodTick(uint8_t methodCode);
void feedButtonRunNow();
