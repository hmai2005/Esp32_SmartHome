#include "dht11.h"
#include "sensor_flame.h"
#include "sensor_gas.h"
#include "sensor_rain.h"
#include "servo_control.h"
#include "fan_control.h"
#include "buzzer_control.h"
#include "led_control.h"
#include "mqtt.h"

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

void setup() {
Serial.begin(9600);
Serial.println("ESP32 START");
//khởi tạo cảm biến, servo, quạt
setupDHT();
setupRainSensor();
setupGasSensor();
setupFlameSensor();
setupServo();
setupFanPWM();
setupBuzzer();
setupLED();


//Khởi tạo Mạng & MQTT
setupWiFiAndMQTT();
}

void loop() {
Serial.println("ESP32 RUNNING");
// 1. Duy trì mạng & xử lý MQTT
maintainMQTTConnection();
//DHT11
readDHT();
//GAS
readGasSensor();

//báo cháy
fireAlarmTask();
// Mot lan nhan nut vua tat coi (neu dang bao dong), vua dieu khien thiet bi.
// Khong xoa su kien ngat Servo/Fan/LED.
consumeAlarmStopButton();
updateFanControl();
updateLEDButton();
controlServoByRain();

// 3. Gửi dữ liệu định kỳ
sendSensorData();
delay(500);
}
