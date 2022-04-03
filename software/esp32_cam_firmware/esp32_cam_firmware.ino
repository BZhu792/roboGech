/**
 * @brief: ESP32 CAM firmware to control image upload processing.
 *         Extended upon simple project from https://how2electronics.com/esp32-cam-based-object-detection-identification-with-opencv/ in a FreeRTOS setting
 *         
 * @setup: Follow instructions from https://how2electronics.com/esp32-cam-based-object-detection-identification-with-opencv/
 *         You will need to download the ESP-IDF with version >= 2.0, as well as several libraries.
 * 
 * @author: roboGech
 * @date: 4/2/2022
 */
 
/////////////////////////////////////////////////////////////////////////////////////////////////////
// Global Variables and Imports
/////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * FreeRTOS
 */
#if CONFIG_FREERTOS_UNICORE
  static const BaseType_t app_cpu = 0;
#else
  static const BaseType_t app_cpu = 1;
#endif

/**
 * Camera and Server
 */
#include <WebServer.h>
#include <WiFi.h>
#include <esp32cam.h>

// Set to local network/hotspot
const char* WIFI_SSID = "CPhone";
const char* WIFI_PASS = "fy7271cs701vqg";
 
WebServer server(80);
 
static auto loRes = esp32cam::Resolution::find(320, 240);
static auto midRes = esp32cam::Resolution::find(350, 530);
static auto hiRes = esp32cam::Resolution::find(800, 600);

void serveJpg(void);
void handleJpgLo(void);
void handleJpgHi(void);
void handleJpgMid(void);

/////////////////////////////////////////////////////////////////////////////////////////////////////
// Task Functions
/////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * @brief: Handles camera setup and image capture and transmits up to the server
 * @param[in]: N/A -- need void pointer for FreeRTOS implementation
 */
void handle_cam_and_server(void * no_params) {
  Serial.println();
  {
    using namespace esp32cam;
    Config cfg;
    cfg.setPins(pins::AiThinker);
    cfg.setResolution(hiRes);
    cfg.setBufferCount(2);
    cfg.setJpeg(80);
 
    bool ok = Camera.begin(cfg);
    Serial.println(ok ? "CAMERA OK" : "CAMERA FAIL");
  }
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    vTaskDelay(pdMS_TO_TICKS(500));
  }
  Serial.print("http://");
  Serial.println(WiFi.localIP());
  Serial.println("  /cam-lo.jpg");
  Serial.println("  /cam-hi.jpg");
  Serial.println("  /cam-mid.jpg");
 
  server.on("/cam-lo.jpg", handleJpgLo);
  server.on("/cam-hi.jpg", handleJpgHi);
  server.on("/cam-mid.jpg", handleJpgMid);
 
  server.begin();
  
  while (true) {
    server.handleClient();
  }
}

/**
 * @brief: Entry point to FreeRTOS framework
 */
void setup() {
  Serial.begin(115200);

  // Create tasks
  xTaskCreatePinnedToCore(
    handle_cam_and_server,
    "Handle Camera and Server",
    8192,
    NULL,
    5,// Highest Priority Task
    NULL,
    app_cpu);

  vTaskDelete(NULL); // Delete setup() and loop() task
}

void loop() {
  // Not needed in FreeRTOS Implementation
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
// Helper Functions
/////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * @brief: Captures images and sends them to the server
 * @reference: https://how2electronics.com/esp32-cam-based-object-detection-identification-with-opencv/
 */
void serveJpg(void)
{
  auto frame = esp32cam::capture();
  if (frame == nullptr) {
    Serial.println("CAPTURE FAIL");
    server.send(503, "", "");
    return;
  }
  Serial.printf("CAPTURE OK %dx%d %db\n", frame->getWidth(), frame->getHeight(),
                static_cast<int>(frame->size()));
 
  server.setContentLength(frame->size());
  server.send(200, "image/jpeg");
  WiFiClient client = server.client();
  frame->writeTo(client);
}

/**
 * @brief: Tries to set the camera image resolution to low
 * @reference: https://how2electronics.com/esp32-cam-based-object-detection-identification-with-opencv/
 */
void handleJpgLo(void)
{
  if (!esp32cam::Camera.changeResolution(loRes)) {
    Serial.println("SET-LO-RES FAIL");
  }
  serveJpg();
}

/**
 * @brief: Tries to set the camera image resolution to high
 * @reference: https://how2electronics.com/esp32-cam-based-object-detection-identification-with-opencv/
 */
void handleJpgHi(void)
{
  if (!esp32cam::Camera.changeResolution(hiRes)) {
    Serial.println("SET-HI-RES FAIL");
  }
  serveJpg();
}

/**
 * @brief: Tries to set the camera image resolution to medium
 * @reference: https://how2electronics.com/esp32-cam-based-object-detection-identification-with-opencv/
 */
void handleJpgMid(void)
{
  if (!esp32cam::Camera.changeResolution(midRes)) {
    Serial.println("SET-MID-RES FAIL");
  }
  serveJpg();
}
