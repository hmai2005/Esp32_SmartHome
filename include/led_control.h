#ifndef LED_CONTROL_H
#define LED_CONTROL_H

void setupLED();
void updateLEDButton();

void discardLEDButtonEvent();

bool isLEDOn();
void setLED(bool state);

#endif