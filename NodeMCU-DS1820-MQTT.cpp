#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// ================= การตั้งค่า WiFi และ MQTT =================
const char* ssid = "ohoh_TRSR_2.4GHz";
const char* password = "patty2net";

// หมายเหตุ: หากใช้ชื่อ "mosquitto" แล้วบอร์ดหาไม่เจอ (DNS Resolving error) 
// แนะนำให้เปลี่ยนเป็น IP Address ของเครื่องเซิร์ฟเวอร์ เช่น "192.168.1.100"
const char* mqtt_server = "mosquitto"; 
const int mqtt_port = 1883;

// ================= การตั้งค่า เซ็นเซอร์ DS1820 =================
// กำหนดขาที่ต่อสาย Data จากเซ็นเซอร์ DS1820 (ขา D2 ของ NodeMCU คือ GPIO4)
const int ONE_WIRE_BUS = 4; 

// สร้างออบเจ็กต์สำหรับสื่อสารผ่าน 1-Wire
OneWire oneWire(ONE_WIRE_BUS);
// ส่งอ้างอิงของ OneWire ไปยัง DallasTemperature
DallasTemperature sensors(&oneWire);

// ================= ตัวแปรสำหรับใช้งาน =================
WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastMsg = 0; // ตัวแปรสำหรับจับเวลา

// ฟังก์ชันเชื่อมต่อ WiFi
void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("กำลังเชื่อมต่อ WiFi ไปที่: ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  // รอจนกว่าจะเชื่อมต่อสำเร็จ
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("เชื่อมต่อ WiFi สำเร็จแล้ว");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

// ฟังก์ชันเชื่อมต่อ MQTT Broker (กรณีหลุด)
void reconnect() {
  // วนลูปจนกว่าจะเชื่อมต่อ MQTT สำเร็จ
  while (!client.connected()) {
    Serial.print("กำลังพยายามเชื่อมต่อ MQTT Broker...");
    
    // สร้าง Client ID แบบสุ่ม
    String clientId = "NodeMCUClient-";
    clientId += String(random(0xffff), HEX);
    
    // พยายามเชื่อมต่อ
    if (client.connect(clientId.c_str())) {
      Serial.println(" เชื่อมต่อ MQTT สำเร็จ");
    } else {
      Serial.print(" ล้มเหลว, สถานะ(rc)=");
      Serial.print(client.state());
      Serial.println(" จะลองใหม่ในอีก 5 วินาที");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  
  // เริ่มต้นการทำงานของเซ็นเซอร์
  sensors.begin();
  
  // เริ่มต้นการเชื่อมต่อ WiFi และตั้งค่า MQTT
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
}

void loop() {
  // ตรวจสอบการเชื่อมต่อ MQTT ตลอดเวลา ถ้าหลุดให้ต่อใหม่
  if (!client.connected()) {
    reconnect();
  }
  
  // ฟังก์ชันนี้จำเป็นต้องเรียกใช้เรื่อยๆ เพื่อรักษาการเชื่อมต่อและรับข้อมูลที่ subscribe ไว้
  client.loop();

  // จับเวลาด้วย millis() เพื่อให้ทำงานทุกๆ 60 วินาที (60000 มิลลิวินาที)
  unsigned long now = millis();
  if (now - lastMsg > 60000) {
    lastMsg = now;

    // สั่งให้เซ็นเซอร์อ่านค่าอุณหภูมิ
    sensors.requestTemperatures(); 
    
    // ดึงค่าอุณหภูมิเป็นองศาเซลเซียส
    float tempC = sensors.getTempCByIndex(0);
    
    // ตรวจสอบว่าอ่านค่าได้ถูกต้องหรือไม่ (ถ้าไม่ได้ต่อสายจะอ่านได้ -127.00)
    if (tempC != DEVICE_DISCONNECTED_C) {
      // แปลงค่า Float เป็นข้อความ (String/Char Array) เพื่อส่งผ่าน MQTT
      char tempString[8];
      dtostrf(tempC, 1, 2, tempString); // รูปแบบ: ตัวแปร, ความยาวรวม, ทศนิยม 2 ตำแหน่ง, ตัวแปรปลายทาง

      Serial.print("อุณหภูมิที่อ่านได้: ");
      Serial.print(tempString);
      Serial.println(" °C");

      // ส่งข้อมูล (Publish) ไปยัง Topic: test/temp
      client.publish("test/temp", tempString);
      Serial.println("ส่งข้อมูลผ่าน MQTT เรียบร้อยแล้ว");
    } else {
      Serial.println("เกิดข้อผิดพลาด: ไม่สามารถอ่านค่าจากเซ็นเซอร์ DS1820 ได้ (กรุณาเช็คสายสัญญาณ)");
    }
  }
}