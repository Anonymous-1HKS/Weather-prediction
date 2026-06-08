#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>

const char* ssid = "VGU_Student_Guest";
const char* password = "";
const char* server_ip = "172.16.134.105";

// Cấu hình chân cứng của dòng mạch ESP32-CAM AI-Thinker phổ thông
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

#define PIN_INPUT_TRIGGER   12 // Chân nhận lệnh từ IO23 của ESP32-A1S truyền sang

void setup() {
  Serial.begin(115200);
  pinMode(PIN_INPUT_TRIGGER, INPUT);

  // Cấu hình thông số Camera
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  // Thiết lập chất lượng ảnh nhằm tối ưu dung lượng truyền tải
  config.frame_size = FRAMESIZE_VGA; // Độ phân giải 640x480 vừa đủ nét
  config.jpeg_quality = 12;
  config.fb_count = 1;

  // Khởi tạo cụm Camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Khởi tạo Camera thất bại, mã lỗi: 0x%x", err);
    return;
  }

  // Kết nối mạng Wi-Fi sẵn
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
    Serial.print(".");
  }
  Serial.println("\nCAM đã kết nối mạng, đang đợi lệnh từ A1S...");
}

void loop() {
  // Đợi cho đến khi chân lệnh từ ESP32-A1S chuyển sang mức HIGH
  if (digitalRead(PIN_INPUT_TRIGGER) == HIGH) {
    Serial.println("Nhận được xung lệnh! Đang tiến hành chụp...");

    // Xả bỏ 1 khung hình cũ đầu tiên để lấy độ sáng thực tế cân bằng hơn
    camera_fb_t * fb = esp_camera_fb_get();
    esp_camera_fb_return(fb);

    // Bắt hình ảnh thực tế
    fb = esp_camera_fb_get();
    if(!fb) {
      Serial.println("Chụp ảnh thất bại!");
      return;
    }

    // Tiến hành truyền dữ liệu mảng byte ảnh lên server Node.js
    if(WiFi.status() == WL_CONNECTED){
      HTTPClient http;
      String url = "http://" + String(server_ip) + ":3000/upload-image";
      http.begin(url);
      http.addHeader("Content-Type", "image/jpeg");
      http.addHeader("x-device-id", "esp32-cam-station");

      int httpResponseCode = http.POST(fb->buf, fb->len);

      if (httpResponseCode > 0) {
        Serial.printf("Tải ảnh lên thành công, phản hồi từ server: %d\n", httpResponseCode);
      } else {
        Serial.printf("Lỗi khi tải ảnh: %s\n", http.errorToString(httpResponseCode).c_str());
      }
      http.end();
    }

    // Giải phóng bộ nhớ của ảnh sau khi gửi xong
    esp_camera_fb_return(fb);

    // Chờ cho chân lệnh hạ xuống LOW để tránh lặp lại hành động chụp liên tục
    while(digitalRead(PIN_INPUT_TRIGGER) == HIGH) {
      delay(10);
    }
  }
}