#include <Arduino.h>
#include "led_control.h"

#define LED_PIN     26
#define LED_BUTTON  25

bool ledState = false;

// Biến chống dội nút
unsigned long lastButtonTime = 0;

// Trạng thái nút trước đó
bool lastButtonState = HIGH;


void setupLED()
{
    // GPIO26 điều khiển transistor
    pinMode(LED_PIN, OUTPUT);

    // Mặc định tắt đèn
    digitalWrite(LED_PIN, LOW);
    ledState = false;

    // GPIO25 làm nút nhấn
    // Dùng điện trở kéo lên nội bộ
    pinMode(LED_BUTTON, INPUT_PULLUP);

    Serial.println("Khoi tao LED & Button thanh cong!");
}


void updateLEDButton()
{
    bool buttonState = digitalRead(LED_BUTTON);

    // Phát hiện cạnh nhấn:
    // HIGH -> LOW
    if (lastButtonState == HIGH && buttonState == LOW)
    {
        // Chống dội nút 250 ms
        if (millis() - lastButtonTime > 250)
        {
            ledState = !ledState;

            digitalWrite(LED_PIN, ledState ? HIGH : LOW);

            if (ledState)
            {
                Serial.println("LED -> BAT");
            }
            else
            {
                Serial.println("LED -> TAT");
            }

            lastButtonTime = millis();
        }
    }

    lastButtonState = buttonState;
}


bool isLEDOn()
{
    return ledState;
}


void setLED(bool state)
{
    ledState = state;

    digitalWrite(
        LED_PIN,
        ledState ? HIGH : LOW
    );
}