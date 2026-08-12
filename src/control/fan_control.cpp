#include <Arduino.h>
#include "fan_control.h"

#define FAN_MOSFET_PIN 18
#define FAN_BUTTON_PIN 14

// Tần số PWM
#define PWM_FREQ 5000
// Channel PWM
#define PWM_CHANNEL 1
// Độ phân giải 8 bit
// Giá trị duty từ 0 -> 255
#define PWM_RESOLUTION 8

// ============================================================
// FAN LEVEL
// ============================================================
// Level 0 = OFF     ->   0 / 255 =   0%
// Level 1 = LOW     ->  77 / 255 ≈  30%
// Level 2 = MEDIUM  -> 153 / 255 ≈  60%
// Level 3 = HIGH    -> 255 / 255 = 100%
// ============================================================

static const uint8_t SPEED_LEVELS[4] = {
    0,
    77,
    153,
    255
};
#define TOTAL_LEVELS 4

// BUTTON CONFIG
// Thời gian chống dội nút
#define BUTTON_DEBOUNCE_MS 250

// Khi người dùng bấm nút:
//      Manual Override = ON
//
// Trong 15 giây:
//      AI / MQTT không được thay đổi quạt.
//
// Sau 15 giây:
//      quyền điều khiển trả lại AI Gateway.

#define MANUAL_OVERRIDE_MS 15000

//FAN STATE
// 0 = OFF,1 = LOW,  2 = MEDIUM, 3 = HIGH
static uint8_t currentLevel = 0; // Cấp quạt hiện tại


//kiểm tra có đang ở chế độ thủ công không?
static bool fanManualOverride = false;


// Thời điểm bắt đầu Manual Override
static unsigned long fanOverrideStartTime = 0;
// Thời điểm xử lý nút gần nhất
static unsigned long lastFanButtonTime = 0;

//INTERRUPT STATE
static volatile bool fanButtonEvent = false;


static void writeFanPWM(uint8_t duty)
{
    ledcWrite(
        PWM_CHANNEL,
        duty
    );
}

// SET FAN LEVEL INTERNAL
static bool setFanLevelInternal(uint8_t level)
{
    // --------------------------------------------------------
    // Kiểm tra level
    // --------------------------------------------------------

    if (level >= TOTAL_LEVELS)
    {
        Serial.print("[FAN] Invalid level: ");
        Serial.println(level);

        return false;
    }
    uint8_t duty = SPEED_LEVELS[level];
    writeFanPWM(duty);
    currentLevel = level; //cập nhật level

    Serial.print("[FAN] Level = ");
    Serial.print(currentLevel);

    Serial.print(" | PWM = ");
    Serial.print(duty);
    Serial.print("/255");

    switch (currentLevel)
    {
        case 0:
            Serial.println(" | OFF");
            break;

        case 1:
            Serial.println(" | LOW");
            break;

        case 2:
            Serial.println(" | MEDIUM");
            break;

        case 3:
            Serial.println(" | HIGH");
            break;
    }
    return true;
}
//ngắt ngoài
void IRAM_ATTR handleFanButtonInterrupt()
{
    fanButtonEvent = true; //nút nhấn được nhấn
}



// kiểm tra xem còn nhấn nút không
static void updateManualOverride()
{
    if (!fanManualOverride)
    {
        return;
    }


    unsigned long now = millis();


    // Chưa đủ 15 giây
    if (
        now - fanOverrideStartTime
        < MANUAL_OVERRIDE_MS
    )
    {
        return;
    }
    //hết chế độ thủ công
    fanManualOverride = false;
    Serial.println(
        "[FAN] Manual Override expired" //THÔNG BÁO HẾT CHẾ ĐỘ THỦ CÔNG
    );
    Serial.println(
        "[FAN] Control returned to AI Gateway" //TRỞ LẠI ĐIỀU KHIỂN BẰNG AI
    );
}

// SETUP FAN PWM
void setupFanPWM()
{
    ledcSetup(
        PWM_CHANNEL,
        PWM_FREQ,
        PWM_RESOLUTION
    );
    ledcAttachPin(
        FAN_MOSFET_PIN,
        PWM_CHANNEL
    );
    //TRẠNG THÁI BAN ĐẦU
    currentLevel = 0;
    fanManualOverride = false;
    fanOverrideStartTime = 0;
    lastFanButtonTime = 0;
    fanButtonEvent = false;

    // quạt OFF
    writeFanPWM(
        SPEED_LEVELS[0]
    );
    // BUTTON 
    pinMode(
        FAN_BUTTON_PIN,
        INPUT_PULLUP
    );
    //NGẮT NGOÀI
    attachInterrupt(
        digitalPinToInterrupt(
            FAN_BUTTON_PIN
        ),
        handleFanButtonInterrupt,
        FALLING
    );
    Serial.println();
    Serial.println(
        "[FAN] Initial Level: 0 (OFF)"
    );
}

//UPDATE FAN CONTROL

// Hàm này PHẢI được gọi liên tục trong loop().
// Luồng:
// Button
//      ↓
// Interrupt
//      ↓
// fanButtonEvent = true
//      ↓
// updateFanControl()
//      ↓
// Debounce
//      ↓
// Tăng level
//
// 0 -> 1 -> 2 -> 3 -> 0
//
//      ↓
// Manual Override = ON
//      ↓
// AI bị khóa trong 15 giây


void updateFanControl()
{
    // CHECK BUTTON
    updateManualOverride();
    if (!fanButtonEvent)
    {
        return;
    }
    fanButtonEvent = false;
    unsigned long now = millis();

    if (now - lastFanButtonTime < BUTTON_DEBOUNCE_MS)
    {
        return;
    }
    lastFanButtonTime = now;

    uint8_t nextLevel = (currentLevel + 1)% TOTAL_LEVELS;

    bool success =
        setFanLevelInternal(nextLevel);

    if (!success)
    {
        return;
    }

    //ĐIỀU KHIỂN THỦ CÔNG
    fanManualOverride = true;

    // Mỗi lần bấm:
    // reset lại timer 15 giây
    fanOverrideStartTime = now;
    Serial.print("[FAN] MANUAL Override ON");
    Serial.print(" | Level = ");
    Serial.print(currentLevel);
    Serial.print(" | Timeout = " );
    Serial.print(MANUAL_OVERRIDE_MS / 1000 );
    Serial.println(" s");
}

// 14. CHECK MANUAL OVERRIDE
bool isFanManualOverrideActive()
{
    // Cập nhật timeout trước
    updateManualOverride();
    return fanManualOverride;
}

//  CAN AI CONTROL FAN?
bool canAIGovernFan()
{
    return !isFanManualOverrideActive();
}

// SET FAN LEVEL FROM AI / MQTT
// mqtt.cpp sẽ gọi:
//      setFanLevelFromAI(level);
// level:
//      0 = OFF
//      1 = LOW
//      2 = MEDIUM
//      3 = HIGH
bool setFanLevelFromAI(int level)
{
    if (level < 0 || level >= TOTAL_LEVELS)
    {
        Serial.print( "[FAN] AI requested invalid level: ");
        Serial.println( level );
        return false;
    }

    // CHẾ ĐỘ THỦ CÔNG ĐƯỢC ƯU TIÊN
    if (!canAIGovernFan())
    {
        Serial.print("[FAN] AI command blocked by MANUAL mode");
        Serial.print( " | Requested = ");
        Serial.print(level );
        Serial.print(" | Actual = ");
        Serial.println(currentLevel);
        return false;
    }
    // Nếu Python gửi lại command định kỳ,
    // mà quạt đã ở đúng level: không cần ghi PWM lại.
    // Nhưng vẫn return true
    // để MQTT có thể gửi ACK.
    if (currentLevel == (uint8_t) level )
    {
        return true;
    }
    bool success =  setFanLevelInternal((uint8_t)level);
    if (!success)
    {
        return false;
    }
    Serial.print( "[FAN] AI applied Level " );
    Serial.println(level);
    return true;
}
// Trả về cấp quạt thực tế:
// mqtt.cpp có thể dùng hàm này để gửi ACK về Raspberry Pi.
int getFanLevel()
{
    return (int)currentLevel;
}