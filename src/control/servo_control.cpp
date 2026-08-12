#include <Arduino.h>
#include <ESP32Servo.h>
#include "servo_control.h"
#include "sensor_rain.h"

#define SERVO_PIN 22 // Chân điều khiển Servo 
#define BUTTON_PIN 13 //button điều khiển servo

Servo myServo;

// Biến trạng thái Servo & Biến đếm thời gian chống dội (Debounce)
volatile bool currentServoState = false;  // false: 0 độ, true: 180 độ
volatile unsigned long lastDebounceTime = 0;
volatile bool manualOverride = false;      // Đang trong chế độ ép điều khiển thủ công
unsigned long overrideStartTime = 0;       // Thời điểm bắt đầu nhấn nút

// --- HÀM XỬ LÝ NGẮT NGOÀI ---
void IRAM_ATTR handleButtonInterrupt() {
  unsigned long currentTime = millis();
  // Chống dội nút bấm (Debounce 250ms)
  if (currentTime - lastDebounceTime > 250) {
    currentServoState = !currentServoState; // Đảo trạng thái Servo
    manualOverride = true;                  // Kích hoạt chế độ thủ công
    overrideStartTime = millis(); // Cập nhật ngay thời điểm bấm
    lastDebounceTime = currentTime;
  }
}

void setupServo() {
  // Cho phép cấp xung cho Servo trên ESP32
  ESP32PWM::allocateTimer(0);
  myServo.setPeriodHertz(50);    // Tần số chuẩn cho Servo 50Hz
  myServo.attach(SERVO_PIN, 500, 2400); // Gắn chân với dải xung chuẩn (500us - 2400us)

  // Góc mặc định khi khởi động: 0 độ (Tạnh ráo)
  myServo.write(0);

  // 2. Cấu hình chân nút bấm ngắt ngoài (Dùng điện trở kéo lên nội bộ PULLUP)
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  // Đăng ký ngắt ngoài: kích hoạt khi nhấn nút (FALLING: từ HIGH xuống LOW)
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), handleButtonInterrupt, FALLING);

  Serial.println("Khoi tao Servo thanh cong!");
}

// Trả về true nếu đã thu dây phơi (giai đoạn Servo quay 180 độ)
bool isClothesRetracted() {
  return currentServoState; 
}

void controlServoByRain() {
  // --- CHẾ ĐỘ THỦ CÔNG (ƯU TIÊN NÚT BẤM) ---
  // Nếu người dùng vừa nhấn nút thủ công
  if (manualOverride) {
    // Điều khiển Servo theo trạng thái nút vừa bấm
    int targetAngle = currentServoState ? 180 : 0;
    // Chỉ ghi lệnh nếu Servo chưa ở đúng vị trí
    if (myServo.read() != targetAngle) {
      myServo.write(targetAngle);
      Serial.printf("Nut bam thu cong -> Quay Servo %d do\n", targetAngle);
    }

    // Duy trì chế độ thủ công trong 10 giây, sau 10s sẽ tự động trả lại cho Cảm biến mưa
    if (millis() - overrideStartTime > 10000) {
      manualOverride = false;
      overrideStartTime = 0;
      Serial.println(F("Het thoi gian uu tien thu cong -> Tra lai quyen cho Cam bien Mua"));
    }
    return; 
  }

  // --- CHẾ ĐỘ TỰ ĐỘNG THEO CẢM BIẾN MƯA ---

  // CHỈ QUAY SERVO VÀ IN SERIAL KHI TRẠNG THÁI THAY ĐỔI
  if (isRaining && !currentServoState) {
    myServo.write(180);
    currentServoState = true;
    Serial.println(F("Phat hien mua -> Quay Servo 180 do (Dong mai che)"));
  } 
  else if (!isRaining && currentServoState) {
    myServo.write(0);
    currentServoState = false;
    Serial.println(F("Troi tanh -> Quay Servo 0 do (Mo mai che)"));
    }
  }
