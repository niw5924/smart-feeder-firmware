#include <WiFi.h>
#include <WebServer.h>
#include <Servo.h>

const int motor_pin1 = D2;
const int motor_pin2 = D3;
const int motor_button = D7;

const int servo_pin = D6;
Servo servo;

WebServer server(80);

const char* AP_SSID = "SmartFeeder_Setup";
const char* AP_PASS = "12345678";

void startApAndWeb() {
  WiFi.mode(WIFI_MODE_AP);
  WiFi.softAP(AP_SSID, AP_PASS);

  IPAddress ip = WiFi.softAPIP();

  server.on("/", []() {
    server.send(200, "text/plain", "Hello! SmartFeeder Web is running.");
  });

  server.begin();

  Serial.print("AP SSID: ");
  Serial.println(AP_SSID);
  Serial.print("Open: http://");
  Serial.println(ip);
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

  startApAndWeb();
}

void loop() {
  server.handleClient();

  if (digitalRead(motor_button) == LOW) {
    Serial.println("pressed");

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
}
