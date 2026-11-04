#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

#include <Arduino.h>

// Khởi tạo driver, nút nhấn và công tắc hành trình.
void setupMotor();

// Gọi liên tục trong loop(), kể cả khi mất mạng.
void controlMotorByRain();

// Điều khiển từ MQTT/app: true = thu; false = mở.
void setMotorRetracted(bool retracted);

// Chỉ true khi công tắc THU HẾT đang được nhấn.
bool isClothesRetracted();

// Trạng thái chi tiết dùng cho Serial/MQTT/app.
const char* getClothesState();

// Dừng và giữ chế độ thủ công đến khi có lệnh mới.
void stopClothesMotor();

// Kiểm tra/xóa lỗi sau khi đã kiểm tra phần cơ khí.
bool hasMotorFault();
bool clearMotorFault();

// Bỏ qua lần nhấn nút hiện tại, ví dụ nút dùng tắt còi.
void discardMotorButtonEvent();

#endif