#include "dht11.h"
#include "sensor_flame.h"
#include "sensor_gas.h"
#include "sensor_rain.h"
#include "motor_control.h"  // Đổi từ servo_control.h
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

    // Khởi tạo cảm biến, motor, quạt
    setupDHT();
    setupRainSensor();
    setupGasSensor();
    setupFlameSensor();
    setupMotor();  // Đổi từ setupServo()
    setupFanPWM();
    setupBuzzer();
    setupLED();

    // Khởi tạo mạng & MQTT
    setupWiFiAndMQTT();
}

void loop() {
    Serial.println("ESP32 RUNNING");

    // 1. Duy trì mạng & xử lý MQTT
    maintainMQTTConnection();

    // DHT11
    readDHT();

    // GAS
    readGasSensor();

    // Báo cháy
    fireAlarmTask();

    // Một lần nhấn nút vừa tắt còi (nếu đang báo động),
    // vừa điều khiển thiết bị.
    consumeAlarmStopButton();

    updateFanControl();
    updateLEDButton();
    controlMotorByRain();  // Đổi từ controlServoByRain()

    // 3. Gửi dữ liệu định kỳ
    sendSensorData();

    delay(500);
}