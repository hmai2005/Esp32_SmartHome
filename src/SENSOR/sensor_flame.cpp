#include <Arduino.h>
#include "sensor_flame.h"

// Khai báo chân kết nối trên ESP32
#define FLAME_ANALOG_PIN  34  // Chân AO (đọc độ mạnh của bức xạ lửa)

const int FLAME_THRESHOLD = 500; 

//Hàm trả về true nếu CÓ LỬA, false nếu AN TOÀN
bool isFlameDetected() {
  int analogVal = analogRead(FLAME_ANALOG_PIN);
  
  // Phát hiện lửa khi giá trị Analog nhỏ hơn ngưỡng FLAME_THRESHOLD
  return (analogVal < FLAME_THRESHOLD);
}

void setupFlameSensor() {
  // Chân Analog Read trên ESP32 không bắt buộc pinMode(), nhưng khai báo INPUT cho rõ ràng
  pinMode(FLAME_ANALOG_PIN, INPUT_PULLUP); 
  
  Serial.println("Khoi tao cam bien LUA thanh cong!");
}

void readFlameSensor() {
  int analogVal = analogRead(FLAME_ANALOG_PIN);

  Serial.print(F("Gia tri Lua (Analog): "));
  Serial.print(analogVal);

  // Chỉ so sánh trực tiếp giá trị Analog với ngưỡng FLAME_THRESHOLD
  if (analogVal < FLAME_THRESHOLD) {
    Serial.println(F(" CẢNH BÁO KHẨN CẤP: PHÁT HIỆN LỬA!"));
  } else {
    Serial.println(F("Trang thai: An toan (Khong co lua)"));
  }
}