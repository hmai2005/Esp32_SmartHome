#include <Arduino.h>
#include "sensor_gas.h"

// Khai báo chân kết nối trên ESP32
#define GAS_ANALOG_PIN  32  // Chân AO (Đọc giá trị nồng độ gas 0-4095)

const int GAS_THRESHOLD = 1800; 

void setupGasSensor() {
  pinMode(GAS_ANALOG_PIN, INPUT);
  Serial.println("Khoi tao cam bien GAS thanh cong!");
}

// Trả về true nếu phát hiện rò rỉ Gas/Khói vượt ngưỡng
bool isGasDetected() {
  return (analogRead(GAS_ANALOG_PIN) > GAS_THRESHOLD);
}

// Trả về giá trị nồng độ Gas thô (0 - 4095) để gửi lên MQTT
int getGasAnalogValue() {
  return analogRead(GAS_ANALOG_PIN);
}

void readGasSensor() {
  int analogVal = getGasAnalogValue();
  Serial.print(F("Nong do Gas (Analog): "));
  Serial.print(analogVal);

  if (isGasDetected()) {
    Serial.println(F(" -> CẢNH BÁO: PHÁT HIỆN RÒ RỈ GAS / KHÓI!"));
  } else {
    Serial.println(F(" -> Trang thai: An toan"));
  }
}