#ifndef SENSOR_PIR_H
#define SENSOR_PIR_H

#include <Arduino.h>

// Khởi tạo cảm biến PIR
void setupPIR();

// Đọc trạng thái cảm biến PIR
// true  = phát hiện chuyển động
// false = không có chuyển động
bool isMotionDetected();

#endif