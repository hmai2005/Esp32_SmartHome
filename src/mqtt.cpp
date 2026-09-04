#include "dht11.h"
#include "sensor_flame.h"
#include "sensor_gas.h"
#include "sensor_rain.h"

#include "servo_control.h"
#include "fan_control.h"
#include "led_control.h"
#include "buzzer_control.h"

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// Điền WiFi của bạn tại đây.
// Không nên commit mật khẩu thật lên Git.
const char* ssid = "GIMhomes #4";
const char* password = "likeyourhome";

const char* mqtt_server = "caebe80fc31544bab129c40f7b5d3425.s1.eu.hivemq.cloud";
const int mqtt_port = 8883;
const char* mqtt_username = "SmartHomeApp";
const char* mqtt_password = "12345678";


// ============================================================
// MQTT TOPICS - FAN
// ============================================================
// Python Gateway -> ESP32
//
// {
//   "schema": 1,
//   "command_id": 15,
//   "level": 3,
//   "source": "smart-home-ai-controller",
//   "sent_at_unix_ms": ...
// }
const char* command_topic = "smart-home/fan/command";
const char* manual_command_topic = "smart-home/fan/manual-command";

// ESP32 -> Python Gateway
//
// ACK command:
//
// {
//   "schema": 1,
//   "command_id": 15,
//   "applied_level": 3
// }
//
// Hoặc Manual Override:
//
// {
//   "schema": 1,
//   "command_id": 15,
//   "applied_level": 1,
//   "error": "manual_override"
// }
const char* state_topic = "smart-home/fan/state";
// ESP32 availability:
// {"status":"online"}
// hoặc LWT:
// {"status":"offline"}
const char* availability_topic = "smart-home/fan/availability";
// ============================================================
// MQTT TOPICS - SENSOR
// ============================================================
const char* temperature_topic = "smart-home/sensor/temperature";
const char* humidity_topic ="smart-home/sensor/humidity";
const char* rain_topic =  "smart-home/sensor/rain";
const char* flame_topic = "smart-home/sensor/flame";
const char* person_count_topic = "smart-home/ai/person_count";
const char* safety_status_topic = "smart-home/safety/status";
// ============================================================
// MQTT TOPICS - ACTUATOR STATE
// ============================================================
const char* led_state_topic = "smart-home/led/state";
const char* servo_retracted_topic = "smart-home/servo/retracted";
const char* led_command_topic = "smart-home/led/command";
const char* servo_command_topic = "smart-home/servo/command";
// ============================================================
// AVAILABILITY PAYLOAD
// ============================================================
const char* availability_online_payload = "{\"status\":\"online\"}";
const char* availability_offline_payload = "{\"status\":\"offline\"}";

// ============================================================
// MQTT CLIENT
// ============================================================
WiFiClientSecure espClient;
PubSubClient client(espClient);
// ============================================================
// TIMING
// ============================================================
// DHT11, MQ2, Rain, Servo:
// publish định kỳ mỗi 5 giây.
const unsigned long SENSOR_INTERVAL_MS = 5000;

// Thử reconnect WiFi/MQTT cách nhau 5 giây.
const unsigned long RECONNECT_INTERVAL_MS = 5000;

unsigned long lastSensorMsg = 0;
unsigned long lastReconnectAttempt = 0;
int personCount = 0;

bool connectWiFi()
{
    if (WiFi.status() == WL_CONNECTED)
    {
        return true;
    }
    Serial.print("Connecting to WiFi: ");
    Serial.println(ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    // Không block vô hạn.
    // Chờ tối đa khoảng 10 giây.
    for (int attempt = 0; attempt < 20; attempt++)
    {
        if (WiFi.status() == WL_CONNECTED)
        {
            Serial.println();
            Serial.println("WiFi connected");
            Serial.print("ESP32 IP: ");
            Serial.println(WiFi.localIP());
            return true;
        }
        Serial.print(".");
        delay(500);
    }
    Serial.println();
    Serial.println("WiFi connection failed");
    return false;
}
// MQTT PUBLISH HELPERS
//chuyển đổi số thực float sang dạng văn bản
bool publishFloat(const char* topic,float value,bool retained = true)
{
    char buffer[24];
    snprintf(buffer, sizeof(buffer),"%.2f",value);
    return client.publish( topic,buffer,retained);
}

bool publishBool( const char* topic, bool value,bool retained = true)
{
    return client.publish(topic, value ? "1" : "0", retained);
}

bool publishStateJson(const char* topic, bool state, bool retained = true)
{
    JsonDocument doc;
    doc["state"] = state ? 1 : 0;

    char buffer[32];
    size_t written = serializeJson(doc, buffer, sizeof(buffer));
    return written > 0 && client.publish(topic, buffer, retained);
}

void publishSafetyStatus()
{
    bool gasDetected = isGasDetected();
    bool flameDetected = isFlameDetected();

    bool safe = !gasDetected && !flameDetected && !isAlarmActive();

    Serial.println("========== SAFETY ==========");
    Serial.print("Gas detected   : ");
    Serial.println(gasDetected ? "YES" : "NO");

    Serial.print("Flame detected : ");
    Serial.println(flameDetected ? "YES" : "NO");

    Serial.print("Safe           : ");
    Serial.println(safe ? "TRUE" : "FALSE");

    JsonDocument doc;
    doc["safe"] = safe;

    char buffer[64];
    size_t written = serializeJson(doc, buffer, sizeof(buffer));

    if (written == 0)
    {
        Serial.println("ERROR: Cannot serialize safety status");
        return;
    }

    Serial.print("MQTT safety payload: ");
    Serial.println(buffer);

    bool ok = client.publish(
        safety_status_topic,
        buffer,
        true
    );

    Serial.print("Publish safety: ");
    Serial.println(ok ? "OK" : "FAILED");

    Serial.println("============================");
}

// FAN STATE / ACK : phản hồi
//đóng gói trạng thái hiện tại thành chuỗi JSON
void publishFanState( int appliedLevel, long commandId = -1, const char* errorMessage = nullptr)
{
    JsonDocument doc;
    doc["schema"] = 1; //định danh phiên bản cấu trúc dữ liệu

    // Level THỰC TẾ trên ESP32.
    doc["applied_level"] = appliedLevel; //mức quạt thực tế

    // Nếu đây là phản hồi cho command của Python
    // thì gửi lại command_id.
    if (commandId >= 0)
    {
        doc["command_id"] = commandId;
    }

    if (errorMessage != nullptr)
    {
        doc["error"] = errorMessage;
    }

    char buffer[192];
    size_t written = serializeJson(doc, buffer, sizeof(buffer)); //chuyển thành chuỗi ký tự và ghi vào buffer
    //serializeJson: trả về số byte đã ghi
    if (written == 0)
    {
        Serial.println("ERROR: Cannot serialize fan state");
        return;
    }
    bool ok = client.publish(state_topic, buffer, false);
    Serial.print( "Fan state -> MQTT: ");
    Serial.println(buffer);
    if (!ok)
    {
        Serial.println(
            "WARNING: Fan state publish failed"
        );
    }
}
// APPLY FAN COMMAND
// Hàm phải:
// - nhận 0..3
// - cập nhật PWM
// - cập nhật biến current fan level
// - trả true nếu thành công
bool applyAIFanLevel(int level)
{
    if (level < 0 || level > 3)
    {
        return false;
    }
    return setFanLevelFromAI(level);
}
// ============================================================
// MQTT COMMAND CALLBACK: tin nhắn mqtt gửi từ gatewway đến esp32
// ============================================================
void callback( char* topic, byte* payload,unsigned int length)
{
    if (strcmp(topic, person_count_topic) == 0)
    {
        JsonDocument doc;
        DeserializationError jsonError = deserializeJson(doc, payload, length);

        if (jsonError || !doc["count"].is<int>())
        {
            Serial.println("Invalid person count JSON");
            return;
        }

        int receivedCount = doc["count"].as<int>();
        if (receivedCount < 0)
        {
            Serial.println("Invalid person count: negative value");
            return;
        }

        personCount = receivedCount;
        Serial.print("Person count received: ");
        Serial.println(personCount);
        return;
    }

    if (strcmp(topic, manual_command_topic) == 0)
    {
        JsonDocument doc;
        DeserializationError jsonError = deserializeJson(doc, payload, length);
        if (jsonError || !doc["level"].is<int>())
        {
            Serial.println("Invalid manual fan command JSON");
            return;
        }

        int requestedLevel = doc["level"].as<int>();
        if (!setFanLevelFromManual(requestedLevel))
        {
            publishFanState(getFanLevel(), -1, "invalid_level");
            return;
        }
        publishFanState(getFanLevel());
        return;
    }

    if (strcmp(topic, led_command_topic) == 0)
    {
        JsonDocument doc;
        DeserializationError jsonError = deserializeJson(doc, payload, length);
        JsonVariant state = doc["state"];
        if (!state.is<bool>())
        {
            state = doc["on"];
        }

        if (jsonError || !state.is<bool>())
        {
            Serial.println("Invalid LED command JSON");
            return;
        }

        setLED(state.as<bool>());
        Serial.print("LED command received: ");
        Serial.println(isLEDOn() ? "ON" : "OFF");
        return;
    }

    if (strcmp(topic, servo_command_topic) == 0)
    {
        JsonDocument doc;
        DeserializationError jsonError = deserializeJson(doc, payload, length);
        JsonVariant state = doc["retracted"];
        if (!state.is<bool>())
        {
            state = doc["state"];
        }

        if (jsonError || !state.is<bool>())
        {
            Serial.println("Invalid servo command JSON");
            return;
        }

        setServoRetracted(state.as<bool>());
        return;
    }

    // Chỉ quan tâm fan command.
    if (strcmp(topic, command_topic) != 0) 
    //So sánh topic nhận được với topic điều khiển quạt (command_topic).
    {
        return;
    }
    Serial.print( "MQTT command received: ");
    for ( unsigned int i = 0; i < length;i++)
    {
        Serial.print( static_cast<char>( payload[i] ));
        //Ép kiểu byte sang ký tự (char) để in nội dung JSON theo dạng văn bản đọc được.
    }
    Serial.println();

    JsonDocument doc; //chứa DL sau khi giải mã
    DeserializationError jsonError = deserializeJson(doc, payload, length);

    if (jsonError)
    {
        Serial.print( "Invalid command JSON: ");
        Serial.println(jsonError.c_str());
        return;
    }
    //Kiểm tra phiên bản Schema
    int schema = doc["schema"] | 0;
    if (schema != 1) //không hỗ trợ
    {
        Serial.print("Unsupported command schema: ");
        Serial.println(schema);
        return;
    }
    // COMMAND ID
    if (!doc["command_id"].is<long>())
    {
        Serial.println( "Missing/invalid command_id" );
        return;
    }
    long commandId = doc["command_id"].as<long>();
    // LEVEL
    if (!doc["level"].is<int>() )
    {
        Serial.println( "Missing/invalid level");
        publishFanState( getFanLevel(),commandId, "invalid_level");
        return;
    }
    int requestedLevel = doc["level"].as<int>();
    if (requestedLevel < 0 || requestedLevel > 3)
    {
        Serial.print("Invalid requested level: ");
        Serial.println(requestedLevel);
        publishFanState(getFanLevel(),commandId, "invalid_level");
        return;
    }
    Serial.print( "AI requested level = " );
    Serial.print(requestedLevel);
    Serial.print(", command_id = ");
    Serial.println( commandId );

    // Kiểm tra chế độ đè thủ công: MANUAL OVERRIDE
    if (!canAIGovernFan())
    {
        int actualLevel = getFanLevel();
        Serial.println("AI command blocked: MANUAL mode");
        //Phản hồi về MQTT rằng lệnh bị từ chối do chế độ thủ công
        publishFanState(actualLevel,commandId,"manual_override");
        return;
    }
    // ========================================================
    // APPLY COMMAND: Thực thi lệnh điều khiển
    // ========================================================
    bool applied = applyAIFanLevel(requestedLevel );
    if (!applied)
    {
        Serial.println( "Failed to apply fan level");
        publishFanState(getFanLevel(), commandId,"apply_failed");
        return;
    }
    // VERIFY ACTUAL LEVEL
    int actualLevel = getFanLevel(); //Đọc lại trạng thái quạt thực tế

    if (actualLevel != requestedLevel)
    {
        Serial.print("Fan level mismatch. requested=");
        Serial.print(requestedLevel);
        Serial.print(", actual=");
        Serial.println( actualLevel);
        publishFanState(actualLevel, commandId,"level_mismatch");
        return;
    }
    //XÁC NHẬN THÀNH CÔNG ACK
    publishFanState(actualLevel,commandId);
}

// PERIODIC SENSOR DATA
void sendSensorData()
{
    if (!client.connected())
    {
        return;
    }

    unsigned long now = millis();
    if (now - lastSensorMsg < SENSOR_INTERVAL_MS)
    {
        return;
    }
    lastSensorMsg = now;
    // DHT11
    float temperature = getTemperature();
    float humidity = getHumidity();

    if (!isnan(temperature))
    {
        publishFloat(temperature_topic,temperature, true);
    }
    else
    {
        Serial.println( "DHT11 temperature read failed");
    }
    if (!isnan(humidity))
    {
        publishFloat(humidity_topic,humidity,true);
    }
    else
    {
        Serial.println("DHT11 humidity read failed");
    }
    // RAIN / FLAME: 1 = detected, 0 = clear.
    bool rain = isRaining();
    publishBool(rain_topic, rain, true);
    bool flame = isFlameDetected();
    publishStateJson(flame_topic, flame, true);

    // SERVO 
    bool retracted = isClothesRetracted();
    publishBool(servo_retracted_topic, retracted, true);

    bool ledOn = isLEDOn();
    publishBool( led_state_topic, ledOn, true);    
    publishSafetyStatus();

    Serial.println(
        "========== SENSOR MQTT =========="
    );
    if (!isnan(temperature))
    {
        Serial.print("Temperature: ");
        Serial.print( temperature );
        Serial.println( " C");
    }
    if (!isnan(humidity))
    {
        Serial.print("Humidity: " );
        Serial.print( humidity );
        Serial.println( " %");
    }
    Serial.print("Rain: ");
    Serial.println(rain);

    Serial.print("Flame: ");
    Serial.println(flame);

    Serial.print("Fan level: ");
    Serial.println(getFanLevel());

    Serial.print("Servo retracted: ");
    Serial.println( retracted);

    // Trạng thái LED
    Serial.print("LED: ");
    Serial.println(ledOn ? "ON" : "OFF");
    Serial.println("=================================");
}

// MQTT CLIENT ID: Broker yêu cầu mỗi thiết bị kết nối vào phải có một Client ID hoàn toàn duy nhất
String buildMqttClientId()
{
    String mac = WiFi.macAddress();
    // AA:BB:CC:DD:EE:FF
    // ->
    // AABBCCDDEEFF
    mac.replace( ":","");
    return ( "ESP32-SmartHome-" + mac );
}

// MQTT CONNECT
bool connectMQTT()
{
    if (client.connected())
    {
        return true;
    }
    if (WiFi.status() != WL_CONNECTED)
    {
        return false;
    }
    Serial.print("Connecting MQTT...");

    String clientId = buildMqttClientId();

    // LAST WILL
    // Nếu ESP32 mất điện / WiFi bất ngờ:
    // Broker giữ:{"status":"offline"}

    bool connected = client.connect( clientId.c_str(), mqtt_username, mqtt_password,
        availability_topic, 1, true, availability_offline_payload);
    if (!connected)
    {
        Serial.print( "MQTT failed, rc=");
        Serial.println(client.state());
        return false;
    }
    Serial.println("connected"
    );

    // ONLINE STATE
    client.publish(availability_topic,availability_online_payload, true);
    // SUBSCRIBE FAN COMMAND
    bool subscribed = client.subscribe( command_topic,1);
    Serial.print( "Subscribe ");
    Serial.print(command_topic);
    Serial.print( ": ");
    Serial.println( subscribed ? "OK" : "FAILED");

    bool manualSubscribed = client.subscribe(manual_command_topic, 1);
    Serial.print("Subscribe ");
    Serial.print(manual_command_topic);
    Serial.print(": ");
    Serial.println(manualSubscribed ? "OK" : "FAILED");

    bool personCountSubscribed = client.subscribe(person_count_topic, 1);
    Serial.print("Subscribe ");
    Serial.print(person_count_topic);
    Serial.print(": ");
    Serial.println(personCountSubscribed ? "OK" : "FAILED");

    bool ledSubscribed = client.subscribe(led_command_topic, 1);
    Serial.print("Subscribe ");
    Serial.print(led_command_topic);
    Serial.print(": ");
    Serial.println(ledSubscribed ? "OK" : "FAILED");

    bool servoSubscribed = client.subscribe(servo_command_topic, 1);
    Serial.print("Subscribe ");
    Serial.print(servo_command_topic);
    Serial.print(": ");
    Serial.println(servoSubscribed ? "OK" : "FAILED");

    // ========================================================
    // PUBLISH CURRENT FAN SNAPSHOT: chủ động gửi trạng thái của quạt lên mqtt
    // ========================================================
    //
    // Không có command_id vì đây không phải ACK.
    //
    // Python actuator vẫn cập nhật applied_level
    // nhưng không coi đây là ACK cho command mới.
    publishFanState(getFanLevel());
    // Force sensor refresh ngay sau reconnect.
    lastSensorMsg = millis() - SENSOR_INTERVAL_MS;
    return true;
}
// SETUP WIFI + MQTT
void setupWiFiAndMQTT()
{
    connectWiFi();
    // HiveMQ Cloud requires MQTT over TLS on port 8883.
    espClient.setInsecure();
    client.setServer( mqtt_server, mqtt_port);
    client.setCallback(callback);

    // PubSubClient mặc định buffer nhỏ.
    // Command JSON Python có nhiều field,nên đặt 512 byte.
    client.setBufferSize(512);

    // Đồng bộ với keepalive phía Python.
    client.setKeepAlive(60); //thời gian duy trì kết nối
    if (WiFi.status() == WL_CONNECTED)
    {
        connectMQTT();
    }
}
// MAIN MQTT MAINTENANCE
void maintainMQTTConnection()
{
    unsigned long now = millis();
    // WIFI RECONNECT
    if ( WiFi.status() != WL_CONNECTED)
    {
        if (now - lastReconnectAttempt >= RECONNECT_INTERVAL_MS)
        {
            lastReconnectAttempt = now;
            connectWiFi();
        }
        return;
    }
    // MQTT RECONNECT
    if (!client.connected())
    {
        if (now - lastReconnectAttempt >= RECONNECT_INTERVAL_MS)
        {
            lastReconnectAttempt = now;
            connectMQTT();
        }
        return;
    }
    // MQTT NETWORK LOOP
    client.loop(); //duy trì luồng DL
    // PERIODIC SENSOR DATA
    sendSensorData();
}