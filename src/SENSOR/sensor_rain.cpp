#include <Arduino.h>
#include "sensor_rain.h"

#define RAIN_ANALOG_PIN 35

const int RAIN_THRESHOLD = 2800;

void setupRainSensor() {
  Serial.println("Khoi tao cam bien MUA thanh cong!");
}

// Đọc giá trị analog của cảm biến mưa.
// Hàm này không điều khiển cấp/ngắt nguồn cảm biến.
int getRainAnalogValue() {
  int analogVal = analogRead(RAIN_ANALOG_PIN);
  return analogVal;
}

// Trả về true nếu giá trị analog thấp hơn ngưỡng.
bool isRaining() {
  return (getRainAnalogValue() < RAIN_THRESHOLD);
}

// In giá trị cảm biến và trạng thái ra Serial.
void readRainSensor() {
  int analogVal = getRainAnalogValue();

  Serial.print(F("Gia tri mua (Analog): "));
  Serial.print(analogVal);

  if (analogVal < RAIN_THRESHOLD) {
    Serial.println(F(" -> Trang thai: TROI DANG MUA!"));
  } else {
    Serial.println(F(" -> Trang thai: Tanh rao"));
  }
}