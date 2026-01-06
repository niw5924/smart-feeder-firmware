#include "feed_control.h"

static void handleFeedButton(
  Servo& servo,
  int motorPin1,
  int motorPin2,
  int feedButtonPin
) {
  if (digitalRead(feedButtonPin) == LOW) {
    Serial.println("pressed");

    servo.write(50);
    delay(300);

    digitalWrite(motorPin1, HIGH);
    digitalWrite(motorPin2, HIGH);
    delay(1000);

    digitalWrite(motorPin1, LOW);
    digitalWrite(motorPin2, LOW);
    delay(300);

    servo.write(180);
    delay(500);

    delay(300);
  }
}

static void handleUltrasonic(Servo& servo, int motorPin1, int motorPin2) {
  (void)servo;
  (void)motorPin1;
  (void)motorPin2;
}

static void handleIntervalTimer(Servo& servo, int motorPin1, int motorPin2) {
  (void)servo;
  (void)motorPin1;
  (void)motorPin2;
}

void feedHandle(
  uint8_t methodCode,
  Servo& servo,
  int motorPin1,
  int motorPin2,
  int feedButtonPin
) {
  switch (methodCode) {
    case FEED_METHOD_FEED_BUTTON:
      handleFeedButton(servo, motorPin1, motorPin2, feedButtonPin);
      break;

    case FEED_METHOD_ULTRASONIC:
      handleUltrasonic(servo, motorPin1, motorPin2);
      break;

    case FEED_METHOD_INTERVAL_TIMER:
      handleIntervalTimer(servo, motorPin1, motorPin2);
      break;

    default:
      Serial.println("unknown feed method");
      break;
  }
}
