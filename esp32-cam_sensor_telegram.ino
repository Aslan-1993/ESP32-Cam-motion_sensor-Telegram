#include <Arduino.h>                      // Provides core Arduino functions and macros
#include "esp_camera.h"                   // ESP32-CAM library for camera control
#include <WiFi.h>                         // Wi-Fi connectivity library
#include <WiFiClientSecure.h>            // Secure client for HTTPS requests
#include <UniversalTelegramBot.h>        // Telegram Bot API wrapper library
#include <ArduinoJson.h>                 // JSON parsing support used by Telegram library
#include "esp_http_server.h"             // Enables HTTP server for streaming MJPEG

// ==== Wi-Fi Configuration ====
const char* ssid = "**********";          // Wi-Fi SSID (network name)
const char* password = "*********";       // Wi-Fi password

// ==== Telegram Bot Configuration ====
String BOTtoken = "**************************";   // Telegram bot token
String CHAT_ID = "***********";                  // Telegram chat ID for authorized access

#define _STREAM_CONTENT_TYPE "multipart/x-mixed-replace; boundary=frame"  // MIME type for MJPEG stream

// ==== AI-Thinker ESP32-CAM GPIO Mappings ====
#define PWDN_GPIO_NUM     32             // Camera power-down pin
#define RESET_GPIO_NUM    -1             // Camera reset pin (not used)
#define XCLK_GPIO_NUM      0             // External clock input to camera
#define SIOD_GPIO_NUM     26             // Camera I2C data (SDA)
#define SIOC_GPIO_NUM     27             // Camera I2C clock (SCL)
#define Y9_GPIO_NUM       35             // Camera data pin Y9
#define Y8_GPIO_NUM       34             // Camera data pin Y8
#define Y7_GPIO_NUM       39             // Camera data pin Y7
#define Y6_GPIO_NUM       36             // Camera data pin Y6
#define Y5_GPIO_NUM       21             // Camera data pin Y5
#define Y4_GPIO_NUM       19             // Camera data pin Y4
#define Y3_GPIO_NUM       18             // Camera data pin Y3
#define Y2_GPIO_NUM        5             // Camera data pin Y2
#define VSYNC_GPIO_NUM    25             // Vertical sync signal
#define HREF_GPIO_NUM     23             // Horizontal reference signal
#define PCLK_GPIO_NUM     22             // Pixel clock signal

// ==== Telegram Bot Variables ====
WiFiClientSecure clientTCP;                    // Secure TCP connection object
UniversalTelegramBot bot(BOTtoken, clientTCP); // Initializes Telegram bot with token and TCP client
bool sendPhoto = false;                        // Flag to indicate photo capture request


#define FLASH_LED_PIN 4                        // On-board flash LED pin (GPIO4)
bool flashState = LOW;                         // Flash LED ON/OFF state
unsigned long lastTimeBotRan = 0;              // Timestamp for last Telegram polling
int botRequestDelay = 1000;                    // Polling interval for Telegram in milliseconds

#define PIR_SENSOR_PIN 13                      // GPIO pin connected to PIR motion sensor
unsigned long lastMovementTime = 0;            // Last time motion was detected
const unsigned long movementCooldown = 30000;  // Cooldown period to prevent repeated alerts

httpd_handle_t stream_httpd = NULL;            // HTTP server handle for streaming

// ==== MJPEG Stream Handler ====
static esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t *fb = NULL;                      // Frame buffer for camera images
  esp_err_t res = ESP_OK;                      // Result code

  res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE); // Set MIME type for MJPEG
  if (res != ESP_OK) return res;               // If failed, return error

  while (true) {                                // Continuous streaming loop
    fb = esp_camera_fb_get();                  // Capture a frame from the camera
    if (!fb) {                                 // If frame is null
      Serial.println("Camera capture failed"); // Log failure
      return ESP_FAIL;                         // Return failure
    }

    res = httpd_resp_send_chunk(req, "--frame\r\n", strlen("--frame\r\n"));                             // Send boundary
    res |= httpd_resp_send_chunk(req, "Content-Type: image/jpeg\r\n\r\n", strlen("Content-Type: image/jpeg\r\n\r\n")); // Send content-type header
    res |= httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);                                  // Send JPEG image data
    res |= httpd_resp_send_chunk(req, "\r\n", 2);                                                        // End frame
    esp_camera_fb_return(fb);                        // Return frame buffer to the driver

    if (res != ESP_OK) break;                        // Exit if sending failed
    delay(100);                                      // Delay for frame rate control
  }
  return res;                                        // Return result
}

// ==== Start the HTTP Server ====
void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();  // Use default HTTP server config
  config.server_port = 80;                         // Set port to 80

  httpd_uri_t stream_uri = {                       // Define stream endpoint
    .uri       = "/",                              // URI path
    .method    = HTTP_GET,                         // HTTP method
    .handler   = stream_handler,                   // Handler function
    .user_ctx  = NULL                              // Context (not used)
  };

  if (httpd_start(&stream_httpd, &config) == ESP_OK) { // Start server
    httpd_register_uri_handler(stream_httpd, &stream_uri); // Register the stream URI
    Serial.println("Camera stream available at /");        // Print confirmation
  } else {
    Serial.println("Failed to start web server!");         // Print error
  }
}

// ==== Send Photo to Telegram ====
String sendPhotoTelegram() {
  camera_fb_t *fb = esp_camera_fb_get();        // Capture a frame
  if (!fb) {                                    // If capture fails
    Serial.println("Camera capture failed");    // Log failure
    return "Camera capture failed";             // Return error message
  }

  const char* server = "api.telegram.org";      // Telegram API host

  // Construct multipart message headers
  String head = "--boundary\r\nContent-Disposition: form-data; name=\"chat_id\"\r\n\r\n" + CHAT_ID +
                "\r\n--boundary\r\nContent-Disposition: form-data; name=\"photo\"; filename=\"esp32.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
  String tail = "\r\n--boundary--\r\n";         // Multipart footer

  clientTCP.connect(server, 443);               // Connect to Telegram API over HTTPS
  clientTCP.println("POST /bot" + BOTtoken + "/sendPhoto HTTP/1.1");        // Send POST request
  clientTCP.println("Host: " + String(server));                            // Set host header
  clientTCP.println("Content-Type: multipart/form-data; boundary=boundary"); // Set content type
  clientTCP.println("Content-Length: " + String(fb->len + head.length() + tail.length())); // Content length
  clientTCP.println();                          // End of headers
  clientTCP.print(head);                        // Send header part
  clientTCP.write(fb->buf, fb->len);            // Send image buffer
  clientTCP.print(tail);                        // Send tail part

  esp_camera_fb_return(fb);                     // Release frame buffer
  delay(1000);                                  // Wait after sending
  return "Photo sent";                          // Return success message
}

// ==== Process Telegram Messages ====
void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {                 // Loop through new messages
    String text = bot.messages[i].text;                      // Extract message text
    String chat_id = bot.messages[i].chat_id;                // Extract sender chat ID

    if (chat_id != CHAT_ID) {                                // Reject unauthorized users
      bot.sendMessage(chat_id, "Unauthorized", "");
      continue;
    }

    if (text == "/start") {                                  // Handle /start command
      String msg = "Welcome!\n";                             // Greeting message
      msg += "/photo - take a photo\n";                      // Instruction: photo
      msg += "/flash - toggle flash\n";                      // Instruction: flash
      msg += "/IP - get IP\n";                               // Instruction: IP
      bot.sendMessage(chat_id, msg, "");                     // Send response
    }

    if (text == "/photo") {                                  // Handle /photo command
      sendPhoto = true;                                      // Set flag to capture photo
    }

    if (text == "/flash") {                                  // Handle /flash command
      flashState = !flashState;                              // Toggle flash state
      digitalWrite(FLASH_LED_PIN, flashState);               // Apply new state to pin
      bot.sendMessage(chat_id, flashState ? "Flash ON" : "Flash OFF", ""); // Confirm to user
    }

    if (text == "/IP") {                                     // Handle /IP command
      bot.sendMessage(chat_id, "IP: http://" + WiFi.localIP().toString(), ""); // Send IP
    }
  }
}

// ==== Arduino Setup Function ====
void setup() {
  Serial.begin(115200);                        // Start serial monitor
  Serial.println();                            // Print newline

  // Configure camera settings
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;        // PWM channel for XCLK
  config.ledc_timer = LEDC_TIMER_0;            // PWM timer
  config.pin_d0 = Y2_GPIO_NUM;                 // Data pin D0
  config.pin_d1 = Y3_GPIO_NUM;                 // Data pin D1
  config.pin_d2 = Y4_GPIO_NUM;                 // Data pin D2
  config.pin_d3 = Y5_GPIO_NUM;                 // Data pin D3
  config.pin_d4 = Y6_GPIO_NUM;                 // Data pin D4
  config.pin_d5 = Y7_GPIO_NUM;                 // Data pin D5
  config.pin_d6 = Y8_GPIO_NUM;                 // Data pin D6
  config.pin_d7 = Y9_GPIO_NUM;                 // Data pin D7
  config.pin_xclk = XCLK_GPIO_NUM;             // Clock signal
  config.pin_pclk = PCLK_GPIO_NUM;             // Pixel clock
  config.pin_vsync = VSYNC_GPIO_NUM;           // VSYNC signal
  config.pin_href = HREF_GPIO_NUM;             // HREF signal
  config.pin_sccb_sda = SIOD_GPIO_NUM;         // SCCB data
  config.pin_sccb_scl = SIOC_GPIO_NUM;         // SCCB clock
  config.pin_pwdn = PWDN_GPIO_NUM;             // Power-down control
  config.pin_reset = RESET_GPIO_NUM;           // Reset pin
  config.xclk_freq_hz = 20000000;              // XCLK frequency
  config.pixel_format = PIXFORMAT_JPEG;        // JPEG output

  // Adjust image settings based on PSRAM
  if (psramFound()) {
    config.frame_size = FRAMESIZE_VGA;         // Set resolution to VGA
    config.jpeg_quality = 10;                  // High quality JPEG
    config.fb_count = 2;                       // Double buffer for performance
  } else {
    config.frame_size = FRAMESIZE_CIF;         // Lower resolution
    config.jpeg_quality = 12;                  // Lower quality
    config.fb_count = 1;                       // Single frame buffer
  }

  esp_err_t err = esp_camera_init(&config);    // Initialize camera
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x", err); // Log failure
    return;
  }

  pinMode(FLASH_LED_PIN, OUTPUT);              // Set flash LED pin as output
  digitalWrite(FLASH_LED_PIN, LOW);            // Turn off flash initially

  WiFi.begin(ssid, password);                  // Connect to Wi-Fi
  WiFi.setSleep(false);                        // Disable sleep for stability

  while (WiFi.status() != WL_CONNECTED) {      // Wait for connection
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");          // Confirm connection

  clientTCP.setCACert(TELEGRAM_CERTIFICATE_ROOT); // Load Telegram CA certificate

  startCameraServer();                         // Start HTTP streaming server

  Serial.println("Camera ready at http://" + WiFi.localIP().toString()); // Print IP
}

// ==== Arduino Main Loop ====
void loop() {
  if (sendPhoto) {                             // If photo requested
    sendPhotoTelegram();                       // Capture and send it
    sendPhoto = false;                         // Reset flag
  }

  if (millis() - lastTimeBotRan > botRequestDelay) { // Polling interval check
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1); // Get new messages
    while (numNewMessages) {                    // While there are messages
      handleNewMessages(numNewMessages);        // Process them
      numNewMessages = bot.getUpdates(bot.last_message_received + 1); // Check again
    }
    lastTimeBotRan = millis();                 // Update polling timestamp
  }

  int pirState = digitalRead(PIR_SENSOR_PIN);  // Read PIR sensor state
  if (pirState == HIGH && (millis() - lastMovementTime) > movementCooldown) {
    bot.sendMessage(CHAT_ID, "Motion detected!", ""); // Send motion alert
    lastMovementTime = millis();                       // Update motion timestamp
  }
}
