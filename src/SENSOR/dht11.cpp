#include <Arduino.h>
#include <DHT.h>       // Bắt buộc phải thêm thư viện gốc của cảm biến
#include "dht11.h"

#define DHTPIN 21
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);  // Khởi tạo đối tượng

float getTemperature() {
  return dht.readTemperature();
}

float getHumidity() {
  return dht.readHumidity();
}

void setupDHT() {
  Serial.println("DHT11 test!");
  dht.begin();
}

void readDHT() {
  float h = dht.readHumidity(); 
  float t = dht.readTemperature();  

  if (isnan(h) || isnan(t)) { 
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }
  Serial.print(F("Độ ẩm: "));
  Serial.print(h);
  Serial.print(F("% Nhiệt độ: "));
  Serial.print(t);
  Serial.print(F(" độ C "));
  Serial.println();
}

