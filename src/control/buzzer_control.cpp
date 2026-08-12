#include <Arduino.h>

#include "buzzer_control.h"
#include "sensor_flame.h"
#include "sensor_gas.h"

//======================
// Chân kết nối
//======================

#define BUZZER_PIN         19

#define SERVO_BUTTON_PIN   13
#define FAN_BUTTON_PIN     14

//======================

bool alarmActive = false;

//======================

void setupBuzzer()
{
    pinMode(BUZZER_PIN, OUTPUT);

    pinMode(SERVO_BUTTON_PIN, INPUT_PULLUP);
    pinMode(FAN_BUTTON_PIN, INPUT_PULLUP);

    digitalWrite(BUZZER_PIN, LOW);
}

//======================

void fireAlarmTask()
{
    bool flame = isFlameDetected();

    bool gas = isGasDetected();

    //-------------------------------------------------
    // Nếu phát hiện cháy thì kích hoạt báo động
    //-------------------------------------------------

    if(flame || gas)
    {
        alarmActive = true;
    }

    //-------------------------------------------------
    // Nếu đang báo động
    //-------------------------------------------------

    if(alarmActive)
    {
        // Còi hú liên tục
        digitalWrite(BUZZER_PIN,HIGH);

        // Nếu nhấn bất kỳ nút nào
        if(digitalRead(SERVO_BUTTON_PIN)==LOW ||
           digitalRead(FAN_BUTTON_PIN)==LOW)
        {
            alarmActive = false;

            digitalWrite(BUZZER_PIN,LOW);

            Serial.println("Tat bao dong!");
        }
    }
    else
    {
        digitalWrite(BUZZER_PIN,LOW);
    }
}