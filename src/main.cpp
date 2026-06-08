#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <driver/i2s.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <PCF8575.h>

// ======================================================
// CẤU HÌNH HỆ THỐNG
// ======================================================
const char *ssid = "VGU_Student_Guest";
const char *password = "";
const char *serverUrl = "http://172.16.131.249:3000/upload";

// Bus I2C 1: Mạch 16-bit (Chân 21, 22)
#define I2C1_SDA 21
#define I2C1_SCL 22
PCF8575 ioExpander(0x20, I2C1_SDA, I2C1_SCL);

// Bus I2C 2: LCD (Chân 19, 18)
#define I2C2_SDA 19
#define I2C2_SCL 18
LiquidCrystal_I2C lcd(0x27, 16, 2);

// I2S Mic
#define I2S_BCK 27
#define I2S_WS 25
#define I2S_DIN 35
#define SAMPLE_RATE 16000
#define BUFFER_SIZE 4096
static uint8_t audioBuffer[BUFFER_SIZE];

// Biến lưu trạng thái cũ để chống nháy
int last3PinStatus = -1;

// ======================================================
// WIFI CONNECT
// ======================================================
void connectWiFi()
{
  lcd.setCursor(0, 0);
  lcd.print("WiFi: Connecting");
  WiFi.begin(ssid, password);
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 20)
  {
    delay(500);
    Serial.print(".");
    retry++;
  }
  lcd.setCursor(0, 0);
  if (WiFi.status() == WL_CONNECTED)
  {
    lcd.print("WiFi: Connected "); // Thêm khoảng trắng để xóa chữ cũ
    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP());
  }
  else
  {
    lcd.print("WiFi: FAILED    ");
  }
  delay(2000);
}

// ======================================================
// SETUP
// ======================================================
void setup()
{
  Serial.begin(115200);
  delay(1000);

  // 1. Khởi tạo LCD trên chân 19, 18
  Wire.begin(I2C2_SDA, I2C2_SCL);
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.print("System Booting");

  // 2. Khởi tạo Mạch 16-bit trên chân 21, 22
  if (ioExpander.begin())
  {
    Serial.println("IO Expander OK!");
    ioExpander.pinMode(P0, INPUT);
  }

  // 3. WiFi
  connectWiFi();

  // 4. Cấu hình I2S
  i2s_config_t i2s_config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
      .sample_rate = SAMPLE_RATE,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 2,
      .dma_buf_len = 128,
      .use_apll = false};
  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_pin_config_t pin_config = {
      .bck_io_num = I2S_BCK,
      .ws_io_num = I2S_WS,
      .data_out_num = -1,
      .data_in_num = I2S_DIN};
  i2s_set_pin(I2S_NUM_0, &pin_config);

  lcd.clear();
  lcd.print("SYSTEM READY");
  delay(1500);
}

// ======================================================
// RECORD + UPLOAD
// ======================================================
void recordAndUpload()
{
  if (WiFi.status() != WL_CONNECTED)
    return;

  // Đọc trạng thái từ IO Expander
  uint8_t signal3Pin = ioExpander.digitalRead(P0);

  // Cập nhật LCD dòng 1 (Chỉ in khi cần, không dùng lcd.clear)
  lcd.setCursor(0, 0);
  lcd.print("Recording...    ");

  // Cập nhật LCD dòng 2 (Chỉ in khi trạng thái thay đổi để chống nháy)
  if (signal3Pin != last3PinStatus)
  {
    lcd.setCursor(0, 1);
    lcd.print("3Pin Status: ");
    lcd.print(signal3Pin == HIGH ? "1" : "0");
    lcd.print(" ");
    last3PinStatus = signal3Pin;
  }

  size_t bytesRead = 0;
  i2s_read(I2S_NUM_0, audioBuffer, BUFFER_SIZE, &bytesRead, pdMS_TO_TICKS(2000));

  // Thông báo Upload
  lcd.setCursor(0, 0);
  lcd.print("Uploading...    ");

  HTTPClient http;
  http.begin(serverUrl);
  http.setTimeout(10000);
  http.addHeader("Content-Type", "application/octet-stream");
  http.addHeader("x-device-id", "ESP32-A1S-Tuan");
  http.addHeader("x-sensor-status", String(signal3Pin));

  int httpCode = http.POST(audioBuffer, bytesRead);

  lcd.setCursor(0, 0);
  if (httpCode == 200)
  {
    lcd.print("SENT SUCCESS    ");
  }
  else
  {
    lcd.print("HTTP ERR: ");
    lcd.print(httpCode);
    lcd.print("  ");
  }
  http.end();

  // Đếm ngược (Không dùng clear để giữ thông báo SUCCESS)
  for (int i = 5; i > 0; i--)
  {
    lcd.setCursor(12, 1);
    lcd.print(i);
    lcd.print("s ");
    delay(1000);
  }
}

void loop()
{
  recordAndUpload();
}