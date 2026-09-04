#ifndef FAN_CONTROL_H
#define FAN_CONTROL_H

#include <Arduino.h>


// ============================================================
// FAN INITIALIZATION
// ============================================================

/**
 * Khởi tạo:
 * - PWM điều khiển quạt
 * - chân nút bấm thủ công
 * - ngắt ngoài của nút bấm
 *
 * Mặc định quạt khởi động ở Level 0 = OFF.
 */
void setupFanPWM();


// ============================================================
// FAN MAIN LOOP
// ============================================================

/**
 * Xử lý sự kiện nút bấm thủ công.
 *
 * Hàm này phải được gọi thường xuyên trong loop().
 *
 * ISR chỉ đặt cờ báo có sự kiện nút bấm.
 * Việc:
 * - debounce
 * - đổi level
 * - ghi PWM
 * - bật Manual Override
 *
 * được thực hiện trong hàm này.
 */
void updateFanControl();

void discardFanButtonEvent();


// ============================================================
// AI / GATEWAY CONTROL
// ============================================================

/**
 * Kiểm tra AI/Gateway hiện có quyền điều khiển quạt hay không.
 *
 * Returns:
 *   true  -> AI được phép điều khiển.
 *   false -> đang trong thời gian Manual Override.
 */
bool canAIGovernFan();


/**
 * Đặt cấp quạt từ AI/Gateway.
 *
 * level:
 *   0 = OFF
 *   1 = LOW
 *   2 = MEDIUM
 *   3 = HIGH
 *
 * Returns:
 *   true  -> level hợp lệ và đã được áp dụng,
 *            hoặc quạt đã ở đúng level yêu cầu.
 *
 *   false -> level không hợp lệ
 *            hoặc AI đang bị khóa bởi Manual Override
 *            hoặc PWM không áp dụng được.
 */
bool setFanLevelFromAI(int level);

bool setFanLevelFromManual(int level);


// ============================================================
// FAN STATE
// ============================================================

/**
 * Trả về cấp quạt thực tế hiện tại.
 *
 * Returns:
 *   0 = OFF
 *   1 = LOW
 *   2 = MEDIUM
 *   3 = HIGH
 */
int getFanLevel();


/**
 * Kiểm tra Manual Override.
 *
 * Returns:
 *   true  -> người dùng đang giữ quyền điều khiển thủ công.
 *   false -> AI/Gateway được phép điều khiển.
 *
 * Manual Override tự hết hạn sau khoảng thời gian
 * được cấu hình trong fan_control.cpp.
 */
bool isFanManualOverrideActive();


#endif  // FAN_CONTROL_H