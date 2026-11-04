#include <Arduino.h>
#include <esp_arduino_version.h>
#include "motor_control.h"
#include "sensor_rain.h"

// =============================================================
// SMART HOME - L298N + 2 CONG TAC HANH TRINH + NUT D13 + CAM BIEN MUA
// L298N: IN1=D22, IN2=D23, ENA=D4 (PWM 70%).
// QUAN TRONG: KHOI DONG ENA PWM = 0, IN1=IN2=LOW; MOTOR KHONG TU QUAY.
// Chi chay neu: nhan D13 / lenh setMotorRetracted() / MUA MOI PHAT SINH.
// Sau mot su kien mua, neu kho lien tuc 10 s thi TU MO (1 LAN).
// Khong tu mo sau 10 s ke tu luc vua cap nguon khi khong mua.
// =============================================================

// ------------------ CHAN GPIO ------------------
static constexpr uint8_t MOTOR_IN1_PIN   = 22; // L298N IN1 / INA
static constexpr uint8_t MOTOR_IN2_PIN   = 23; // L298N IN2 / INB
static constexpr uint8_t MOTOR_ENA_PIN   = 4;  // L298N ENA: PWM (thao jumper ENA)
static constexpr uint8_t LIMIT_CLOSE_PIN = 16; // THU HET: D16 = LOW
static constexpr uint8_t LIMIT_OPEN_PIN  = 27; // MO HET : D27 = LOW
static constexpr uint8_t BUTTON_PIN      = 13; // Nhan noi D13 voi GND

// ------------------ PWM 70% ------------------
// PWM 8 bit: 179/255 = 70.2%, gan 70% nhat.
static constexpr uint8_t  MOTOR_PWM_DUTY = 150;
static constexpr uint32_t MOTOR_PWM_FREQ = 1000;  // 1 kHz
static constexpr uint8_t  MOTOR_PWM_BITS = 8;

// Arduino-ESP32 Core 2.x: dam bao kenh PWM 4 KHONG bi quat/coi/den dung chung.
// Core 3.x: ledcAttach tu chon kenh con trong.
static constexpr uint8_t MOTOR_PWM_CH_ENA = 4;

// true: IN2=HIGH, IN1=LOW la THU; IN1=HIGH, IN2=LOW la MO.
// Neu motor quay nguoc thuc te, doi thanh false.
static constexpr bool RETRACT_ON_IN2 = true;

static constexpr uint32_t BUTTON_DEBOUNCE_MS = 40;
static constexpr uint32_t REVERSE_PAUSE_MS   = 300;
static constexpr uint32_t MANUAL_HOLD_MS     = 10000;
static constexpr uint32_t DRY_DELAY_MS       = 10000;
static constexpr uint32_t RAIN_DEBOUNCE_MS   = 500;
static constexpr uint32_t MAX_RUN_MS         = 15000;

// ------------------ TRANG THAI ------------------
enum class Motion { STOPPED, WAITING, OPENING, CLOSING };
static Motion motion = Motion::STOPPED;
static bool targetRetracted = true;
static bool hasMovementCommand = false;
static bool motorReady = false;
static bool motorFault = false;
static const char* motorFaultReason = "Khong co";

static bool manualOverride = false;
static bool manualTimerRunning = false;
static bool stoppedByUser = false;
static uint32_t manualFinishedAt = 0;
static uint32_t waitStartedAt = 0;
static uint32_t runStartedAt = 0;

// Chi tu dong xu ly khi da co SU KIEN MUA SAU KHI KHOI DONG.
// Khoi dong khong xem trang thai KHO la mot lenh MO.
static bool rainInitialized = false;
static bool rainRaw = false;
static bool rainStable = false;
static uint32_t rainChangedAt = 0;
static bool rainCycleActive = false;
static bool dryTimerRunning = false;
static uint32_t dryStartedAt = 0;
static bool autoCommandValid = false;
static bool autoTargetRetracted = false;

static volatile uint32_t buttonPressCounter = 0;
static uint32_t buttonHandledCounter = 0;
static bool buttonSamplerReady = false;

// ------------------ DOC NUT KHONG BLOCKING ------------------
static void motorButtonTask(void* arg) {
    (void)arg;
    bool raw = digitalRead(BUTTON_PIN);
    bool stable = raw;
    uint32_t changedAt = millis();
    for (;;) {
        const uint32_t now = millis();
        const bool sample = digitalRead(BUTTON_PIN);
        if (sample != raw) {
            raw = sample;
            changedAt = now;
        }
        if (raw != stable && uint32_t(now - changedAt) >= BUTTON_DEBOUNCE_MS) {
            stable = raw;
            if (stable == LOW) ++buttonPressCounter;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static bool readButtonPress() {
    if (!buttonSamplerReady) return false;
    const uint32_t count = buttonPressCounter;
    if (count == buttonHandledCounter) return false;
    buttonHandledCounter = count;
    return true;
}

// Cong tac COM -> GND, NO -> GPIO (INPUT_PULLUP): nhan = LOW.
static bool closeLimitPressed() { return digitalRead(LIMIT_CLOSE_PIN) == LOW; }
static bool openLimitPressed()  { return digitalRead(LIMIT_OPEN_PIN) == LOW; }
static bool targetLimitPressed() {
    return targetRetracted ? closeLimitPressed() : openLimitPressed();
}

// ------------------ XUAT PWM LEN ENA, IN1/IN2 LA GPIO THUONG ------------------
static void pwmWriteEnable(uint8_t duty) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcWrite(MOTOR_ENA_PIN, duty);
#else
    ledcWrite(MOTOR_PWM_CH_ENA, duty);
#endif
}

static void stopOutputs() {
    // Ngat ENA truoc, sau do moi ha hai chan dieu khien chieu quay.
    if (motorReady) pwmWriteEnable(0);
    else digitalWrite(MOTOR_ENA_PIN, LOW);
    digitalWrite(MOTOR_IN1_PIN, LOW);
    digitalWrite(MOTOR_IN2_PIN, LOW);
}

static void driveMotor(bool retract) {
    if (!motorReady || motorFault) return;
    // ENA=0 khi thay doi chieu quay, tranh dao chieu khi dang cap dien.
    pwmWriteEnable(0);
    const bool in2High = (retract == RETRACT_ON_IN2);
    digitalWrite(MOTOR_IN1_PIN, in2High ? LOW : HIGH);
    digitalWrite(MOTOR_IN2_PIN, in2High ? HIGH : LOW);
    pwmWriteEnable(MOTOR_PWM_DUTY);
}

static void latchFault(const char* reason) {
    stopOutputs();
    motion = Motion::STOPPED;
    motorFault = true;
    motorFaultReason = reason;
    manualTimerRunning = false;
    Serial.printf("[MOTOR][LOI] %s | D16=%d D27=%d\n",
                  reason, digitalRead(LIMIT_CLOSE_PIN), digitalRead(LIMIT_OPEN_PIN));
}

static void finishMovement(uint32_t now) {
    stopOutputs();
    motion = Motion::STOPPED;
    if (manualOverride) {
        manualFinishedAt = now;
        manualTimerRunning = true;
    }
    Serial.printf("[MOTOR DUNG] Da cham cong tac %s. IN1=0 IN2=0 ENA=0\n",
                  targetRetracted ? "D16 - THU HET" : "D27 - MO HET");
}

// retract=true -> THU, false -> MO; manual=true cho lenh nguoi dung.
static void requestMovement(bool retract, bool manual) {
    if (!motorReady || motorFault) {
        Serial.println("[MOTOR] Tu choi: chua san sang hoac dang khoa loi.");
        return;
    }
    if (closeLimitPressed() && openLimitPressed()) {
        latchFault("D16 va D27 cung LOW");
        return;
    }

    if (manual) {
        manualOverride = true;
        manualTimerRunning = false;
        stoppedByUser = false;
        // Neu co mua trong thoi gian thu cong, cho tu dong kiem tra lai
        // sau khi thao tac thu cong ket thuc.
        autoCommandValid = false;
    }

    if (motion != Motion::STOPPED && targetRetracted == retract) return;

    targetRetracted = retract;
    hasMovementCommand = true;
    if (targetLimitPressed()) {
        finishMovement(millis());
        return;
    }

    stopOutputs();
    motion = Motion::WAITING;
    waitStartedAt = millis();
    Serial.println(retract ? "[LENH] THU GIAN PHOI" : "[LENH] MO GIAN PHOI");
}

static void serviceMovement(uint32_t now) {
    if (!motorReady || motorFault) return;
    if (closeLimitPressed() && openLimitPressed()) {
        latchFault("Hai cong tac cung LOW khi dang van hanh");
        return;
    }
    if (motion == Motion::STOPPED) return;
    if (targetLimitPressed()) {
        finishMovement(now);
        return;
    }

    if (motion == Motion::WAITING) {
        if (uint32_t(now - waitStartedAt) < REVERSE_PAUSE_MS) return;
        runStartedAt = now;
        driveMotor(targetRetracted);
        motion = targetRetracted ? Motion::CLOSING : Motion::OPENING;
        Serial.printf("[MOTOR CHAY %s] Duty=%u/255 (~70%%), doi %s=0 de DUNG\n",
                      targetRetracted ? "THU" : "MO", MOTOR_PWM_DUTY,
                      targetRetracted ? "D16" : "D27");
        return;
    }

    if (uint32_t(now - runStartedAt) >= MAX_RUN_MS)
        latchFault("Timeout 15s: khong cham cong tac dich");
}

static void printStatus(bool force = false) {
    static int old16 = -1, old27 = -1;
    static uint32_t oldTime = 0;
    int d16 = digitalRead(LIMIT_CLOSE_PIN);
    int d27 = digitalRead(LIMIT_OPEN_PIN);
    uint32_t now = millis();
    if (!force && d16 == old16 && d27 == old27 && uint32_t(now - oldTime) < 2000)
        return;
    old16 = d16;
    old27 = d27;
    oldTime = now;
    const char* state = motorFault ? "FAULT" :
        (motion == Motion::STOPPED ? "DUNG" :
         (motion == Motion::WAITING ? "CHO" :
          (motion == Motion::CLOSING ? "THU" : "MO")));
    Serial.printf("[STATUS] D13=%d D16=%d D27=%d | MOTOR=%s | MUA=%s | chu ky mua=%d\n",
                  digitalRead(BUTTON_PIN), d16, d27, state,
                  rainInitialized ? (rainStable ? "CO" : "KHONG") : "CHO DOC",
                  (int)rainCycleActive);
}

// Quan trong: khong phat lenh nao khi doc cam bien lan dau.
// Chi xu ly khi trang thai mua THAY DOI on dinh sau 500ms.
static void updateRainState(uint32_t now) {
    const bool sample = isRaining();
    if (!rainInitialized) {
        rainInitialized = true;
        rainRaw = sample;
        rainStable = sample;
        rainChangedAt = now;
        Serial.printf("[MUA] Khoi tao: %s. KHONG ra lenh motor luc cap nguon.\n",
                      rainStable ? "DANG MUA" : "DANG KHO");
        return;
    }
    if (sample != rainRaw) {
        rainRaw = sample;
        rainChangedAt = now;
    }
    if (rainRaw == rainStable || uint32_t(now - rainChangedAt) < RAIN_DEBOUNCE_MS)
        return;

    rainStable = rainRaw;
    autoCommandValid = false;
    if (rainStable) {
        // Chi mot su kien KHO -> MUA moi cho phep thu tu dong.
        rainCycleActive = true;
        dryTimerRunning = false;
        // Mua moi la su kien an toan: bo khoa DUNG thu cong cu.
        stoppedByUser = false;
        manualOverride = false;
        manualTimerRunning = false;
        Serial.println("[MUA] KHO -> MUA: LENH TU DONG THU (sau 500ms on dinh)");
    } else {
        // Chi dem kho neu truoc do DA CO SU KIEN MUA.
        if (rainCycleActive) {
            dryTimerRunning = true;
            dryStartedAt = now;
            Serial.println("[MUA] MUA -> KHO: dem 10 giay kho de MO");
        } else {
            Serial.println("[MUA] MUA -> KHO: khong co chu ky mua, MOTOR DUNG");
        }
    }
}

static void requestAutomatic(bool retract) {
    if (autoCommandValid && autoTargetRetracted == retract) return;
    autoCommandValid = true;
    autoTargetRetracted = retract;
    requestMovement(retract, false);
}

// ------------------ CAC HAM SU DUNG BOI MAIN / APP ------------------
void setupMotor() {
    // Chi dat GPIO LOW, KHONG kich motor khi khoi dong.
    motorReady = false;
    motion = Motion::STOPPED;
    motorFault = false;
    motorFaultReason = "Khong co";
    manualOverride = false;
    manualTimerRunning = false;
    stoppedByUser = false;
    rainInitialized = false;
    rainCycleActive = false;
    dryTimerRunning = false;
    autoCommandValid = false;

    pinMode(MOTOR_IN1_PIN, OUTPUT);
    pinMode(MOTOR_IN2_PIN, OUTPUT);
    pinMode(MOTOR_ENA_PIN, OUTPUT);
    digitalWrite(MOTOR_IN1_PIN, LOW);
    digitalWrite(MOTOR_IN2_PIN, LOW);
    digitalWrite(MOTOR_ENA_PIN, LOW);
    pinMode(LIMIT_CLOSE_PIN, INPUT_PULLUP);
    pinMode(LIMIT_OPEN_PIN, INPUT_PULLUP);
    pinMode(BUTTON_PIN, INPUT_PULLUP);

#if ESP_ARDUINO_VERSION_MAJOR >= 3
    bool okENA = ledcAttach(MOTOR_ENA_PIN, MOTOR_PWM_FREQ, MOTOR_PWM_BITS);
    if (!okENA) {
        pinMode(MOTOR_ENA_PIN, OUTPUT);
        digitalWrite(MOTOR_ENA_PIN, LOW);
        motorFault = true;
        motorFaultReason = "Loi khoi tao PWM";
        Serial.println("[MOTOR] LOI: ledcAttach that bai");
        return;
    }
#else
    double freqENA = ledcSetup(MOTOR_PWM_CH_ENA, MOTOR_PWM_FREQ, MOTOR_PWM_BITS);
    if (freqENA <= 0) {
        motorFault = true;
        motorFaultReason = "Loi khoi tao PWM";
        Serial.println("[MOTOR] LOI: ledcSetup that bai");
        return;
    }
    ledcWrite(MOTOR_PWM_CH_ENA, 0);
    ledcAttachPin(MOTOR_ENA_PIN, MOTOR_PWM_CH_ENA);
#endif
    motorReady = true;
    stopOutputs(); // BAT BUOC: ENA duty=0 va IN1/IN2=LOW sau khi khoi tao

    BaseType_t created = xTaskCreate(motorButtonTask, "motorBtn", 2048,
                                     nullptr, 1, nullptr);
    buttonSamplerReady = (created == pdPASS);
    buttonHandledCounter = buttonPressCounter;

    Serial.printf("[MOTOR INIT] STOP | PWM=%u/255 (~70%%) | IN1=D22 IN2=D23 ENA=D4 | D16=THU D27=MO D13=NUT\n",
                  MOTOR_PWM_DUTY);
    Serial.printf("[MOTOR INIT] Task nut: %s. Cho nut hoac SU KIEN MUA; KHONG tu MO khi kho.\n",
                  buttonSamplerReady ? "OK" : "LOI");

    if (closeLimitPressed() && openLimitPressed())
        latchFault("D16 va D27 cung LOW luc khoi tao");
    printStatus(true);
}

bool isClothesRetracted() {
    return !motorFault && closeLimitPressed() && !openLimitPressed();
}

const char* getClothesState() {
    if (motorFault) return "FAULT";
    if (closeLimitPressed() && openLimitPressed()) return "FAULT";
    if (motion == Motion::WAITING) return targetRetracted ? "WAIT_CLOSE" : "WAIT_OPEN";
    if (motion == Motion::OPENING) return "OPENING";
    if (motion == Motion::CLOSING) return "CLOSING";
    if (closeLimitPressed()) return "CLOSED";
    if (openLimitPressed()) return "OPEN";
    return "UNKNOWN";
}

void setMotorRetracted(bool retracted) {
    // Lenh truc tiep tu app/MQTT duoc xem nhu lenh thu cong.
    requestMovement(retracted, true);
}

void stopClothesMotor() {
    stopOutputs();
    motion = Motion::STOPPED;
    stoppedByUser = true;
    manualOverride = true;
    manualTimerRunning = false;
    autoCommandValid = false;
    Serial.println("[MOTOR] DUNG theo lenh nguoi dung. Chi chay lai khi co lenh moi/mua moi.");
}

bool hasMotorFault() { return motorFault; }

bool clearMotorFault() {
    if (!motorReady || (closeLimitPressed() && openLimitPressed())) return false;
    motorFault = false;
    motorFaultReason = "Da xoa";
    stopClothesMotor(); // KHONG tu chay lai khi xoa loi
    Serial.println("[MOTOR] Da xoa loi, cho lenh moi.");
    return true;
}

void discardMotorButtonEvent() {
    // Ham tuong thich voi module bao dong dung chung D13.
    // Module bao dong co the nuot lan nhan nut motor; can tach nut neu xung dot.
    if (!motorFault && buttonHandledCounter != buttonPressCounter) {
        Serial.println("[MOTOR] Nut D13 da duoc module bao dong xu ly (discard).");
        buttonHandledCounter = buttonPressCounter;
    }
}

void controlMotorByRain() {
    const uint32_t now = millis();
    if (!motorReady) return;

    // An toan: phat hien cham dich/timeout trong moi vong loop.
    serviceMovement(now);
    // Cap nhat trang thai mua KHONG PHAT LENH o lan doc dau.
    updateRainState(now);
    printStatus();

    const bool pressed = readButtonPress();
    if (pressed && motorFault) {
        Serial.println("[D13] Dang FAULT, thu xoa loi; KHONG CHAY ngay.");
        if (!clearMotorFault())
            Serial.println("[D13] Khong xoa duoc loi: kiem tra D16/D27.");
        return;
    }
    if (motorFault) return;

    if (pressed) {
        const int d16 = digitalRead(LIMIT_CLOSE_PIN);
        const int d27 = digitalRead(LIMIT_OPEN_PIN);
        Serial.printf("[NUT D13] D16=%d D27=%d\n", d16, d27);
        if (motion != Motion::STOPPED) {
            stopClothesMotor();
            return;
        }
        if (d16 == HIGH && d27 == LOW) {
            Serial.println("[D13] MO HET -> THU");
            requestMovement(true, true);
        } else if (d16 == LOW && d27 == HIGH) {
            Serial.println("[D13] THU HET -> MO");
            requestMovement(false, true);
        } else if (d16 == HIGH && d27 == HIGH) {
            // Giua hanh trinh: neu dung lai, nhan de doi chieu lan truoc.
            const bool retract = hasMovementCommand ? !targetRetracted : true;
            requestMovement(retract, true);
        } else {
            latchFault("D16 va D27 cung LOW");
        }
        return; // KHONG cho logic mua ghi de lenh D13 trong cung vong loop
    }

    if (stoppedByUser) return;

    if (manualOverride) {
        if (!manualTimerRunning) return;
        if (uint32_t(now - manualFinishedAt) < MANUAL_HOLD_MS) return;
        manualOverride = false;
        manualTimerRunning = false;
        autoCommandValid = false;
        Serial.println("[MOTOR] Ket thuc uu tien thu cong, tra quyen cho cam bien mua.");
    }

    // TU DONG: KHONG xu ly neu chua co chu ky mua ke tu khi cap nguon.
    if (!rainCycleActive) return;

    if (rainStable) {
        // Co SU KIEN KHO -> MUA: chi gui 1 lenh THU, khong lap lai.
        requestAutomatic(true);
    } else if (dryTimerRunning && uint32_t(now - dryStartedAt) >= DRY_DELAY_MS) {
        // Da tung co mua, kho lien tuc 10 giay: gui 1 lenh MO.
        requestAutomatic(false);
        rainCycleActive = false;    // Xoa chu ky: khong tu mo lap lai.
        dryTimerRunning = false;
    }
}
