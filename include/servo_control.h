#ifndef SERVO_CONTROL_H
#define SERVO_CONTROL_H

void setupServo();
void controlServoByRain();
void discardServoButtonEvent();
bool isClothesRetracted();
void setServoRetracted(bool retracted);
#endif