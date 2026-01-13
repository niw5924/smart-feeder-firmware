#include "feed_control.h"
#include <Servo.h>

#include "mqtt_client.h"

extern int motor_pin1;
extern int motor_pin2;
extern int motor_button;
extern Servo servo;

static void runFeedButtonSequence() {
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

static void runFeedSequenceWithEvent(const char* startedEvent, const char* finishedEvent) {
  mqttPublishActivityEvent(startedEvent, false);
  mqttPublishActivityState("feeding", true);

  runFeedButtonSequence();

  mqttPublishActivityState("idle", true);
  mqttPublishActivityEvent(finishedEvent, false);
}

void feedButtonRunNow() {
  Serial.println("feed_button remote");
  runFeedSequenceWithEvent("feeding_started_remote", "feeding_finished_remote");
}

static void feedButtonTick() {
  if (digitalRead(motor_button) == LOW) {
    Serial.println("feed_button local");
    runFeedSequenceWithEvent("feeding_started_local", "feeding_finished_local");
  }
}

static void ultrasonicTick() {
  Serial.println("ultrasonic tick");
}

static void intervalTimerTick() {
  Serial.println("interval_timer tick");
}

void feedMethodTick(uint8_t methodCode) {
  switch (methodCode) {
    case FEED_METHOD_FEED_BUTTON:
      feedButtonTick();
      break;

    case FEED_METHOD_ULTRASONIC:
      ultrasonicTick();
      break;

    case FEED_METHOD_INTERVAL_TIMER:
      intervalTimerTick();
      break;

    default:
      Serial.println("unknown feed method");
      break;
  }
}
