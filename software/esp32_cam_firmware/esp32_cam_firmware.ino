/**
 * @brief: ESP32 CAM firmware to control the self-sorting trashbin project.
 *         Handles image processing and server connection/upload, and controls stepper motor and RGB status LED.
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
#include <ArduinoWebsockets.h>
#include <WiFi.h>
#include <esp32cam.h>

using namespace websockets;

WebsocketsClient client;

// Set to local network/hotspot
const char* WIFI_SSID = "CPhone";
const char* WIFI_PASS = "fy7271cs701vqg";
const char* SERVER_HOST = "127.0.0.1"; //Enter server adress
const uint16_t SERVER_PORT = 8080; // Enter server port
 
WebServer server(80);
 
static auto loRes = esp32cam::Resolution::find(320, 240);
static auto midRes = esp32cam::Resolution::find(350, 530);
static auto hiRes = esp32cam::Resolution::find(800, 600);

void serveJpg(void);
void handleJpgLo(void);
void handleJpgHi(void);
void handleJpgMid(void);

/**
 * Stepper Motor
 */
typedef enum {
  PLASTIC_BIN = 0,
  PAPER_BIN   = 1,
  CAN_BIN     = 2,
  DEFAULT_BIN = 3,
  
} bin_t;

#define STEPPER_IN1 12
#define STEPPER_IN2 13
#define STEPPER_IN3 15 // GPIO pins 15 and 14 are swapped on the ESP32 CAM board
#define STEPPER_IN4 14
#define NUMBER_OF_STEPS_PER_REV 2048
#define SCALE_FACTOR 1.03

#include <Stepper.h>
Stepper stepper = Stepper(NUMBER_OF_STEPS_PER_REV, STEPPER_IN1, STEPPER_IN2, STEPPER_IN3, STEPPER_IN4);

static bin_t current_bin = DEFAULT_BIN;
static volatile bin_t next_bin    = CAN_BIN;

/**
 * RGB LED
 */
typedef enum {
  LED_IDLE    = 0,
  LED_PENDING = 1,
  LED_APPROVE = 2,
  LED_TIMEOUT = 3,
  
} led_status_t;

#define RGB_RED   2
#define RGB_GREEN 16
#define LED_TIMEOUT_MS 5000

static volatile led_status_t led_status = LED_IDLE;
static bool set_timer = true;
hw_timer_t * timer = NULL;

void rgb_write(int red, int green, int blue);
void onTimer(void);


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
 * @brief: Receives bin movement commands and relays to the stepper motor
 * @param[in]: N/A -- need void pointer for FreeRTOS implementation
 */
void control_stepper(void * no_params) {

  // Set speed, 10 rpm is about the limit
  stepper.setSpeed(10);

  while (true) {
    if (next_bin != current_bin) {      
      // Calculate how many bins over the next bin is
      int moves = (current_bin <= next_bin) ? (next_bin - current_bin) : (next_bin + 4 - current_bin);
      
      // Move the stepper motor
      int steps = (int) SCALE_FACTOR * (moves * NUMBER_OF_STEPS_PER_REV) / 4;
      stepper.step(steps);

      // Update the current bin
      current_bin = next_bin;
    }
  }
}


/**
 * @brief: Receives led status commands and relays to the RGB LED. Acts as a state machine based on LED status.
 * @param[in]: N/A -- need void pointer for FreeRTOS implementation
 */
void control_rgb_led(void * no_params) {
  pinMode(RGB_RED, OUTPUT);
  pinMode(RGB_GREEN, OUTPUT);
  
  /* 1 tick take 1/(80MHZ/80) = 1us so we set divider 80 and count up */
  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, pdMS_TO_TICKS(LED_TIMEOUT_MS), true);

  while (true) {
    switch (led_status) {
      case LED_IDLE:
      {
        // Display nothing (black)
        rgb_write(0, 0);

        // Set timer flag to true in preparation for pending state
        set_timer = true;
        break;
      }
      
      case LED_PENDING:
      {
        // If first time into this state (before exiting), set the detection timeout timer
        if (set_timer)
        {
          timerAlarmEnable(timer);
          set_timer = false;
        }

        // Display yellow
        rgb_write(255, 255);
        break;
      }

      case LED_TIMEOUT:
      {
        // Will only enter this state if the timer went off
        // Display red
        rgb_write(255, 0);

        // Delay task for 1 second
        vTaskDelay(pdMS_TO_TICKS(1000));

        // Move to idle
        led_status = LED_IDLE;
        break;
      }

      case LED_APPROVE:
      {
        // Disable timeout timer
        timerAlarmDisable(timer);

        // Display green
        rgb_write(0, 255);

        // Delay task for 1 second
        vTaskDelay(pdMS_TO_TICKS(1000));

        // Move to idle
        led_status = LED_IDLE;
        break;
      }

      default:
      {
        // Should never get here
        break;
      }
    } // end switch
  } // end while
}

void listen_for_action(void * no_params) {
    Serial.println("listen for action");
    bool connected = client.connect(SERVER_HOST, SERVER_PORT, "/");
    if(connected) {
        Serial.println("Connected to server port!");
        client.send("Hello Server");
    } else {
        Serial.println("Not Connected to server port!");
    }
    client.onMessage([&](WebsocketsMessage message){
        Serial.print("Got Message: ");
        Serial.println(message.data());
    });
    while (true) {
        if(client.available()) {
            client.poll();
        }
        delay(500);
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
    2048,
    NULL,
    5,// Highest Priority Task
    NULL,
    app_cpu);

  xTaskCreatePinnedToCore(
    control_stepper,
    "Control Stepper",
    2048,
    NULL,
    3,
    NULL,
    app_cpu);

  xTaskCreatePinnedToCore(
    control_rgb_led,
    "Control RGB LED",
    2048,
    NULL,
    2,
    NULL,
    app_cpu);
    
  xTaskCreatePinnedToCore(
    listen_for_action,
    "Listen for Action",
    2048,
    NULL,
    1,
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

/**
 * @brief: Dispatches write request to the RGB LED
 * @reference: https://create.arduino.cc/projecthub/muhammad-aqib/arduino-rgb-led-tutorial-fc003e
 * @param[in]: red - the red LED value
 * @param[in]: green - the green LED value
 */
void rgb_write(int red, int green) {
  analogWrite(RGB_RED, red);
  analogWrite(RGB_GREEN, green);
}

/**
 * @brief: ISR for detection timeout, transitions LED status into LED_TIMEOUT
 */
void IRAM_ATTR onTimer(void){
  led_status = LED_TIMEOUT;
}
