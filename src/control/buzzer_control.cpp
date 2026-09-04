#include <Arduino.h>

#include "buzzer_control.h"
#include "sensor_flame.h"
#include "sensor_gas.h"

#define BUZZER_PIN         19
#define SERVO_BUTTON_PIN   13
#define FAN_BUTTON_PIN     14
#define LED_BUTTON  25

//======================
// Biến quản lý trạng thái
//======================
bool alarmActive = false; // Trạng thái chốt báo động
bool alarmMuted  = false; // Cờ khóa tạm thời nếu bấm nút khi gas/lửa vẫn còn
static bool alarmStopButtonPressed = false;

void setupBuzzer()
{
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(SERVO_BUTTON_PIN, INPUT_PULLUP);
    pinMode(FAN_BUTTON_PIN, INPUT_PULLUP);
    pinMode(LED_BUTTON, INPUT_PULLUP);

    digitalWrite(BUZZER_PIN, LOW);
}

void fireAlarmTask()
{
    bool flame = isFlameDetected();
    bool gas = isGasDetected();
    bool sensorTriggered = (flame || gas);

    // 1. Nếu phát hiện gas/lửa VÀ chưa bị tắt tiếng -> Chốt trạng thái báo động
    if (sensorTriggered && !alarmMuted)
    {
        alarmActive = true;
    }

    // 2. Reset cờ tắt tiếng khi môi trường đã hoàn toàn sạch gas/lửa
    if (!sensorTriggered)
    {
        alarmMuted = false;
    }

    // 3. Kiểm tra nút nhấn để tắt còi thủ công
    bool buttonPressed = (digitalRead(SERVO_BUTTON_PIN) == LOW || digitalRead(LED_BUTTON) == LOW ||
                          digitalRead(FAN_BUTTON_PIN) == LOW);

    if (buttonPressed && alarmActive)
    {
        alarmActive = false; // Hủy trạng thái báo động
        alarmMuted  = true;  // Tránh việc cảm biến đang nhận gas/lửa kích hoạt còi lại ngay lập tức
        alarmStopButtonPressed = true;
        digitalWrite(BUZZER_PIN, LOW);
        Serial.println(F("Da nhan nut -> Tat bao dong thu cong!"));
    }

    // 4. Còi kêu liên tục cho đến khi alarmActive bị xóa bởi nút nhấn
    if (alarmActive)
    {
        digitalWrite(BUZZER_PIN, HIGH);
    }
    else
    {
        digitalWrite(BUZZER_PIN, LOW);
    }
}

bool isAlarmActive()
{
    return alarmActive;
}

bool consumeAlarmStopButton()
{
    bool pressed = alarmStopButtonPressed;
    alarmStopButtonPressed = false;
    return pressed;
}