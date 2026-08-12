#ifndef MQTT_H
#define MQTT_H

#include <Arduino.h>

void setupWiFiAndMQTT();
void maintainMQTTConnection();
void sendSensorData();

#endif