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
#define NUMBER_OF_STEPS_PER_REV 512
#define DELAY_MS_PER_STEP 5

static bin_t current_bin = DEFAULT_BIN;
static volatile bin_t next_bin    = CAN_BIN;

void stepper_write(int in1, int in2, int in3, int in4);
void stepper_onestep(void);

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
#define RGB_GREEN 4
#define RGB_BLUE  16
#define LED_TIMEOUT_MS 5000

static volatile led_status_t led_status = LED_IDLE;

void rgb_write(int red, int green, int blue);
void onTimer(void);


/////////////////////////////////////////////////////////////////////////////////////////////////////
// Task Functions
/////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * @brief: Receives bin movement commands and relays to the stepper motor
 * @param[in]: N/A -- need void pointer for FreeRTOS implementation
 */
void control_stepper(void * no_params) {
  pinMode(STEPPER_IN1, OUTPUT);
  pinMode(STEPPER_IN2, OUTPUT);
  pinMode(STEPPER_IN3, OUTPUT);
  pinMode(STEPPER_IN4, OUTPUT);

  while (true) {
    if (next_bin != current_bin) {      
      // Calculate how many bins over the next bin is
      int moves = (current_bin <= next_bin) ? (next_bin - current_bin) : (next_bin + 4 - current_bin);
      
      // Move the stepper motor
      int num = (moves * NUMBER_OF_STEPS_PER_REV) / 4;
      Serial.println("Num: " + num);
      
      for (int i = 0; i < num; i++) {
        Serial.println("i: " + i);
        stepper_onestep();
      }

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
  pinMode(RGB_BLUE, OUTPUT);

  while (true) {

    //Serial.println("LED status: " + led_status);
    
    switch (led_status) {      
      case LED_IDLE:
      {
        // Display white
        rgb_write(255, 255, 255);
        break;
      }
      
      case LED_PENDING:
      {
        // Display yellow
        rgb_write(255, 255, 0);
        break;
      }

      case LED_TIMEOUT:
      {
        // Will only enter this state if the timer went off
        // Display red
        rgb_write(255, 0, 0);

        // Delay task for 1 second
        vTaskDelay(pdMS_TO_TICKS(1000));

        // Move to idle
        led_status = LED_IDLE;
        break;
      }

      case LED_APPROVE:
      {

        // Display green
        rgb_write(0, 255, 0);

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

/**
 * @brief: Entry point to FreeRTOS framework
 */
void setup() {
  Serial.begin(115200);

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
  
  //vTaskDelete(NULL); // Delete setup() and loop() task
}

void loop() {
  // Not needed in FreeRTOS Implementation
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
// Helper Functions
/////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * @brief: Performs a single write to each of the stepper motor control pins
 * @reference: https://create.arduino.cc/projecthub/debanshudas23/getting-started-with-stepper-motor-28byj-48-3de8c9
 * @param[in]: in1 - the value for the in1 pin
 * @param[in]: in2 - the value for the in2 pin
 * @param[in]: in3 - the value for the in3 pin
 * @param[in]: in4 - the value for the in4 pin
 */
void stepper_write(int in1, int in2, int in3, int in4) {
  digitalWrite(STEPPER_IN1, in1);
  digitalWrite(STEPPER_IN2, in2);
  digitalWrite(STEPPER_IN3, in3);
  digitalWrite(STEPPER_IN4, in4);
}

/**
 * @brief: Moves the stepper motor one step
 * @reference: https://create.arduino.cc/projecthub/debanshudas23/getting-started-with-stepper-motor-28byj-48-3de8c9
 */
void stepper_onestep(void) {
  stepper_write(1,0,0,0);
  vTaskDelay(pdMS_TO_TICKS(DELAY_MS_PER_STEP));
  stepper_write(1,1,0,0);
  vTaskDelay(pdMS_TO_TICKS(DELAY_MS_PER_STEP));
  stepper_write(0,1,0,0);
  vTaskDelay(pdMS_TO_TICKS(DELAY_MS_PER_STEP));
  stepper_write(0,1,1,0);
  vTaskDelay(pdMS_TO_TICKS(DELAY_MS_PER_STEP));
  stepper_write(0,0,1,0);
  vTaskDelay(pdMS_TO_TICKS(DELAY_MS_PER_STEP));
  stepper_write(0,0,1,1);
  vTaskDelay(pdMS_TO_TICKS(DELAY_MS_PER_STEP));
  stepper_write(0,0,0,1);
  vTaskDelay(pdMS_TO_TICKS(DELAY_MS_PER_STEP));
  stepper_write(1,0,0,1);
  vTaskDelay(pdMS_TO_TICKS(DELAY_MS_PER_STEP));
}

/**
 * @brief: Dispatches write request to the RGB LED
 * @reference: https://create.arduino.cc/projecthub/muhammad-aqib/arduino-rgb-led-tutorial-fc003e
 * @param[in]: red - the red LED value
 * @param[in]: green - the green LED value
 * @param[in]: blue - the blue LED value
 */
void rgb_write(int red, int green, int blue) {
  analogWrite(RGB_RED, red);
  analogWrite(RGB_GREEN, green);
  analogWrite(RGB_BLUE, blue);
}
