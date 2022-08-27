#include <ArduinoJson.h>
#include <esp_camera.h>
#include <Stepper.h>
#include <WiFi.h>
#include <WebSocketsClient.h>

// Camera pins
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

const char* ssid = "Hangangdong";
const char* password = "34567890";
char url[] = "/";
// char host[] = "192.168.86.243";
// char host[] = "192.168.86.248";
char host[] = "192.168.0.4";
int port = 3000;

WebSocketsClient webSocket;
bool connected = false;

StaticJsonDocument<200> doc;

// LED
#define LEDPIN 4
int ledStatus = LOW;

Stepper stepper(1024, 12, 15, 13, 14);
int speed = 25;
int step;

void configCamera() {
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
  config.frame_size = FRAMESIZE_HVGA;
  config.pixel_format = PIXFORMAT_JPEG;  // for streaming
  //config.pixel_format = PIXFORMAT_RGB565; // for face detection/recognition
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
  //                      for larger pre-allocated frame buffer.
  if (config.pixel_format == PIXFORMAT_JPEG) {
    if (psramFound()) {
      config.jpeg_quality = 10;
      config.fb_count = 2;
      config.grab_mode = CAMERA_GRAB_LATEST;
    } else {
      // Limit the frame size when PSRAM is not available
      config.frame_size = FRAMESIZE_SVGA;
      config.fb_location = CAMERA_FB_IN_DRAM;
    }
  } else {
    // Best option for face detection/recognition
    config.frame_size = FRAMESIZE_240X240;
  }

  // Init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }
}

void liveCam() {
  // Capture a frame
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Frame buffer could not be acquired");
    return;
  }
  // Replace this with your own function
  webSocket.sendBIN(fb->buf, fb->len);

  // Return the frame buffer back to be reused
  esp_camera_fb_return(fb);
}

void printJsonMsg(uint8_t* data) {
  auto ret = deserializeJson(doc, data);
  const char* msgType = doc["msgType"];
  const char* deviceType = doc["deviceType"];
  const char* status = doc["status"];
  // const char* deviceID = doc["deviceID"];

  // Serial.print(msgType); Serial.print("\t");
  // Serial.print(deviceID); Serial.print("\t");
  // Serial.println(ledStatus);
  // Serial.println(String(ledStatus).compareTo("On"));
  if (!String(msgType).compareTo("command")) {
    if (!String(deviceType).compareTo("led")) {
      ledStatus = !String(status).compareTo("on");
      digitalWrite(LEDPIN, ledStatus);
    }
    else if (!String(deviceType).compareTo("stepper")) {
      step = ((!String(status).compareTo("up")) * 2 - 1) * 64;
      stepper.step(step);
    }
  }
}

void webSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      connected = false;
      Serial.printf("[WSc] Disconnected!\n");
      break;
    case WStype_CONNECTED:
      connected = true;
      Serial.printf("[WSc] Connected to url: %s\n", payload);

      // send message to server when Connected
      webSocket.sendTXT("Connected");
      break;
    case WStype_TEXT:
      Serial.printf("[WSc] get text: %s\n", payload);
      if (length > 0) {
        printJsonMsg(payload);
      }

      // send message to server
      // webSocket.sendTXT("message here");
      break;
    case WStype_BIN:
      Serial.printf("[WSc] get binary length: %u\n", length);
      // hexdump(payload, length);

      // send data to server
      // webSocket.sendBIN(payload, length);
      break;
    case WStype_ERROR:
    case WStype_FRAGMENT_TEXT_START:
    case WStype_FRAGMENT_BIN_START:
    case WStype_FRAGMENT:
    case WStype_FRAGMENT_FIN:
      break;
  }
}

void setup() {
  pinMode(LEDPIN, OUTPUT);
  digitalWrite(LEDPIN, ledStatus);

  stepper.setSpeed(speed);

  // Serial.begin(921600);
  Serial.begin(115200);
  Serial.setDebugOutput(true);

  // Connect WiFi
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  delay(1000);

  for (uint8_t t = 4; t > 0; t--) {
    Serial.printf("[SETUP] BOOT WAIT %d...\n", t);
    Serial.flush();
    delay(1000);
  }

  // Connect
  webSocket.begin(host, port, url);
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);

  configCamera();
}

void loop() {
  webSocket.loop();
  if (connected) {
    liveCam();
  }
}