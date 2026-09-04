#ifndef BUZZER_CONTROL_H
#define BUZZER_CONTROL_H

void setupBuzzer();

void fireAlarmTask();

bool isAlarmActive();

bool consumeAlarmStopButton();

#endif