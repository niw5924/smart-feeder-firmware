#pragma once
#include <Arduino.h>
#include <Servo.h>

enum FeedMethod : uint8_t {
  FEED_METHOD_FEED_BUTTON = 0,
  FEED_METHOD_ULTRASONIC = 1,
  FEED_METHOD_INTERVAL_TIMER = 2,
};

void feedHandle(
  uint8_t methodCode,
  Servo& servo,
  int motorPin1,
  int motorPin2,
  int feedButtonPin
);
