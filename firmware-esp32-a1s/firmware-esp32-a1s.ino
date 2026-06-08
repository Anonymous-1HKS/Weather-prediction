#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>

// Thay đổi thông tin mạng của bạn tại đây
const char* ssid = "VGU_Student_Guest";
const char* password = "";
const char* server_ip = "172.16.134.105"; // IP máy tính đang chạy Node.js server

// Cấu hình các chân chân IO sạch đã thống nhất
#define PIN_TRANSISTOR_TCA  5   // Kích nguồn mạch mở rộng
#define PIN_TRANSISTOR_CAM  18  // Kích nguồn nuôi ESP32-CAM
#define PIN_TRIGGER_CAM     23  // Chân lệnh nháy báo ESP32-CAM chụp ảnh
#define PIN_RAIN_SENSOR     19  // Đọc Relay cảm biến mưa rơi

#define TCA_ADDRESS 0x70

// Hàm chuyển kênh trên mạch mở rộng TCA9548A
void tcaSelect(uint8_t bus) {
  if (bus > 7) return;
  Wire.beginTransmission(TCA_ADDRESS);
  Wire.write(1 << bus);
  Wire.endTransmission();
}

void setup() {
  Serial.begin(115200);

  pinMode(PIN_TRANSISTOR_TCA, OUTPUT);
  pinMode(PIN_TRANSISTOR_CAM, OUTPUT);
  pinMode(PIN_TRIGGER_CAM, OUTPUT);
  pinMode(PIN_RAIN_SENSOR, INPUT_PULLUP); // Trở kéo lên nội bộ cho Relay mưa

  // Trạng thái ban đầu: Tắt hết các thiết bị ngoại vi để tiết kiệm điện
  digitalWrite(PIN_TRANSISTOR_TCA, LOW);
  digitalWrite(PIN_TRANSISTOR_CAM, LOW);
  digitalWrite(PIN_TRIGGER_CAM, LOW);

  // Khởi động I2C trên 2 chân IO21 và IO22
  Wire.begin(21, 22);

  // Kết nối Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi đã kết nối thành công!");
}

unsigned long lastSensorCheck = 0;
unsigned long lastCameraTrigger = 0;

void loop() {
  // [NHÁNH 1]: Đọc cảm biến mưa liên tục 24/24 (Không bị ngắt nguồn)
  // LOW = Tiếp điểm đóng (Mưa) | HIGH = Tiếp điểm mở (Tạnh)
  String rainStatus = (digitalRead(PIN_RAIN_SENSOR) == LOW) ? "Mua" : "Tanh";

  // [NHÁNH 2]: Định kỳ đọc cảm biến I2C qua mạch mở rộng (Ví dụ: mỗi 10 giây)
  if (millis() - lastSensorCheck > 10000) {
    lastSensorCheck = millis();

    digitalWrite(PIN_TRANSISTOR_TCA, HIGH); // Bật nguồn cho cụm cảm biến
    delay(200); // Chờ mạch ổn định điện áp

    // Thực hiện chọn kênh và đọc dữ liệu (Mẫu giả lập cấu trúc đọc thực tế của bạn)
    tcaSelect(0); // Kênh 0: LCD 1602
    // Code gửi dữ liệu ra LCD của bạn...

    tcaSelect(1); // Kênh 1: SHT30
    float temp = 28.5; // Thay bằng lệnh đọc thực tế: sht30.readTemperature()
    float hum = 70.2;  // Thay bằng lệnh đọc thực tế: sht30.readHumidity()

    tcaSelect(2); // Kênh 2: MPU6050
    float ax = 0.0, ay = 0.0;

    digitalWrite(PIN_TRANSISTOR_TCA, LOW); // Đọc xong thì tắt nguồn ngay để tiết kiệm điện

    // Đóng gói chuỗi JSON gửi lên endpoint /api/sensors của Server
    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      String url = "http://" + String(server_ip) + ":3000/api/sensors";
      http.begin(url);
      http.addHeader("Content-Type", "application/json");

      String jsonPayload = "{\"device_id\":\"esp32-a1s\",\"temperature\":" + String(temp) +
                           ",\"humidity\":" + String(hum) + ",\"rain_status\":\"" + rainStatus +
                           "\",\"tilt_x\":" + String(ax) + ",\"tilt_y\":" + String(ay) + "}";

      int httpResponseCode = http.POST(jsonPayload);
      http.end();
    }
  }

  // [NHÁNH 3]: Định kỳ gọi ESP32-CAM dậy chụp hình (Ví dụ: mỗi 5 phút = 300000ms)
  // Ở đây test nhanh để mỗi 1 phút (60000ms) cho bạn dễ quan sát
  if (millis() - lastCameraTrigger > 60000) {
    lastCameraTrigger = millis();

    Serial.println("Kích hoạt chu kỳ chụp ảnh...");
    digitalWrite(PIN_TRANSISTOR_CAM, HIGH); // Bật nguồn cấp cho ESP32-CAM khởi động
    delay(3000); // Chờ 3 giây để ESP32-CAM nạp hệ điều hành và tự kết nối Wi-Fi

    // Nháy xung lệnh gửi sang chân Trigger của ESP32-CAM
    digitalWrite(PIN_TRIGGER_CAM, HIGH);
    delay(500);
    digitalWrite(PIN_TRIGGER_CAM, LOW);

    delay(7000); // Chờ 7 giây để ESP32-CAM bắt hình và upload lên Server xong xuôi
    digitalWrite(PIN_TRANSISTOR_CAM, LOW); // Tắt điện hoàn toàn ESP32-CAM, bắt nó ngủ tiếp
    Serial.println("Đã ngắt nguồn ESP32-CAM.");
  }

  // Bạn có thể chèn luồng xử lý thu âm Audio PCM của bạn tiếp tục chạy song song tại đây...
}