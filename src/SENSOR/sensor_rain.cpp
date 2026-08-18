#include <Arduino.h>
#include "sensor_rain.h"
#include "servo_control.h"

#define RAIN_ANALOG_PIN 35

const int RAIN_THRESHOLD = 2800; 

void setupRainSensor() {
  Serial.println("Khoi tao cam bien MUA thanh cong!");
}

// Hàm cấp nguồn tạm thời và đọc giá trị Analog
int getRainAnalogValue() {
  int analogVal = analogRead(RAIN_ANALOG_PIN);
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
