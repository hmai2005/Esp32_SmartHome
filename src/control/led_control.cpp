#include <Arduino.h>
#include "led_control.h"

#define LED_PIN     26
#define LED_BUTTON  25

bool ledState = false;

// Biến ngắt và chống dội (dùng static để tránh đụng độ tên biến)
static volatile unsigned long lastDebounceTime = 0;
static volatile bool buttonPressed = false;

// --- HÀM XỬ LÝ NGẮT NGOÀI (ISR) ---
void IRAM_ATTR handleLEDButtonInterrupt() {
  unsigned long currentTime = millis();
  // Chống dội nút 250ms
  if (currentTime - lastDebounceTime > 250) {
    buttonPressed = true;
    lastDebounceTime = currentTime;
  }
}

void setupLED() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  ledState = false;

  pinMode(LED_BUTTON, INPUT_PULLUP);
  // Đăng ký ngắt ngoài khi nhấn nút (FALLING: HIGH -> LOW)
  attachInterrupt(digitalPinToInterrupt(LED_BUTTON), handleLEDButtonInterrupt, FALLING);

  Serial.println("Khoi tao LED & Button Ngat ngoai thanh cong!");
}

void updateLEDButton() {
  // Xử lý sự kiện khi có tín hiệu từ ngắt nút bấm
  if (buttonPressed) {
    buttonPressed = false; // Xóa cờ ngắt
    ledState = !ledState;  // Đảo trạng thái Bật/Tắt

    digitalWrite(LED_PIN, ledState ? HIGH : LOW);

    if (ledState) {
      Serial.println(F("Nut bam (GPIO25) -> LED BAT"));
    } else {
      Serial.println(F("Nut bam (GPIO25) -> LED TAT"));
    }
  }
}

bool isLEDOn() {
  return ledState;
}

void setLED(bool state) {
  ledState = state;
  digitalWrite(LED_PIN, ledState ? HIGH : LOW);
}