#include <Arduino.h>
#include "sensor_rain.h"
#include "servo_control.h"

#define RAIN_ANALOG_PIN 35 
#define RAIN_POWER_PIN  27

const int RAIN_THRESHOLD = 3500; 

void setupRainSensor() {
  pinMode(RAIN_POWER_PIN, OUTPUT);
  digitalWrite(RAIN_POWER_PIN, LOW); // Mặc định TẮT nguồn
  Serial.println("Khoi tao cam bien MUA thanh cong!");
}

// Hàm cấp nguồn tạm thời và đọc giá trị Analog
int getRainAnalogValue() {
  digitalWrite(RAIN_POWER_PIN, HIGH);
  delay(10); // Chờ nguồn ổn định
  int analogVal = analogRead(RAIN_ANALOG_PIN);
  digitalWrite(RAIN_POWER_PIN, LOW); // Tắt nguồn chống ăn mòn
  return analogVal;
}

// Trả về true nếu TRỜI MƯA
bool isRaining() {
  return (getRainAnalogValue() < RAIN_THRESHOLD);
}

void readRainSensor() {
  int analogVal = getRainAnalogValue();

  Serial.print(F("Gia tri mua (Analog): "));
  Serial.print(analogVal);

  if (analogVal < RAIN_THRESHOLD) {
    Serial.println(F(" -> Trang thai: TRỜI ĐANG MƯA!"));
  } else {
    Serial.println(F(" -> Trang thai: Tạnh ráo"));
  }
}