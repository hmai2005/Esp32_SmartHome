#include <Arduino.h>
#include <ESP32Servo.h>
#include "servo_control.h"
#include "sensor_rain.h"

#define SERVO_PIN 22  // Chân điều khiển Servo 
#define BUTTON_PIN 13 // Button điều khiển servo

Servo myServo;

bool currentServoState = false;       // false: 0 độ (Mở dây phơi), true: 180 độ (Thu dây phơi)
static volatile unsigned long lastDebounceTime = 0;
volatile bool manualTriggered = false; 
static bool manualOverride = false;
static unsigned long overrideStartTime = 0;
static unsigned long dryStartTime = 0;

// --- HÀM XỬ LÝ NGẮT NGOÀI (ISR) ---
void IRAM_ATTR handleButtonInterrupt() {
  unsigned long currentTime = millis();
  if (currentTime - lastDebounceTime > 250) {
    manualTriggered = true; 
    lastDebounceTime = currentTime;
  }
}

void setupServo() {
  ESP32PWM::allocateTimer(0);
  myServo.setPeriodHertz(50);
  myServo.attach(SERVO_PIN, 500, 2400);

  myServo.write(0); // Mặc định mở dây phơi khi khởi động

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), handleButtonInterrupt, FALLING);

  Serial.println("Khoi tao Servo thanh cong!");
}

bool isClothesRetracted() {
  return currentServoState; 
}

void setServoRetracted(bool retracted) {
  currentServoState = retracted;
  myServo.write(retracted ? 180 : 0);
  manualOverride = true;
  overrideStartTime = millis();
  dryStartTime = 0;
  Serial.printf("MQTT -> Servo %s (%d do)\n", retracted ? "THU" : "MO", retracted ? 180 : 0);
}

void controlServoByRain() {
  // Biến quản lý đếm thời gian 10s sau khi tạnh mưa
  const unsigned long DRY_DELAY_MS = 10000; // Thời gian chờ: 10 giây (10000 ms)

  // 1. CHẾ ĐỘ THỦ CÔNG (ƯU TIÊN NÚT BẤM)
  if (manualTriggered) {
    manualTriggered = false;
    currentServoState = !currentServoState;
    manualOverride = true;
    overrideStartTime = millis();
    dryStartTime = 0; // Reset bộ đếm tạnh mưa
    
    int targetAngle = currentServoState ? 180 : 0;
    myServo.write(targetAngle);
    Serial.printf("Nut bam thu cong -> Quay Servo %d do\n", targetAngle);
  }

  if (manualOverride) {
    if (millis() - overrideStartTime > 10000) {
      manualOverride = false;
      Serial.println(F("Het thoi gian uu tien thu cong -> Tra lai quyen cho Cam bien Mua"));
    }
    return; // Đang ưu tiên nút bấm thì bỏ qua logic cảm biến tự động
  }

  // 2. CHẾ ĐỘ TỰ ĐỘNG THEO CẢM BIẾN MƯA
  bool raining = isRaining(); 

  // --- TRƯỜNG HỢP 1: TRỜI MƯA ---
  if (raining) {
    dryStartTime = 0; // Hễ có mưa là reset bộ đếm thời gian tạnh về 0

    if (!currentServoState) { // Nếu dây phơi đang mở -> Thu dây phơi ngay lập tức
      myServo.write(180);
      currentServoState = true;
      Serial.println(F("Phat hien MUA -> Thu day phoi ngay lap tuc (180 do)"));
    }
  } 
  // --- TRƯỜNG HỢP 2: TRỜI TẠNH ---
  else {
    if (currentServoState) { // Nếu dây phơi đang thu -> Cần chờ đủ 10 giây tạnh liên tục
      
      // Bắt đầu chốt mốc thời gian ngay khi phát hiện tạnh mưa
      if (dryStartTime == 0) {
        dryStartTime = millis(); 
        Serial.println(F("Troi da tanh -> Bat dau dem 10 giay de mo lai day phoi..."));
      }

      // Kiểm tra nếu thời gian tạnh mưa đã đủ 10 giây
      if (millis() - dryStartTime >= DRY_DELAY_MS) {
        myServo.write(0);
        currentServoState = false;
        dryStartTime = 0; // Reset bộ đếm
        Serial.println(F("Da tanh mua liendu 10s -> Mo lai day phoi (0 do)"));
      }
    }
  }
}

void discardServoButtonEvent() {
  manualTriggered = false;
}