/**
 * @brief: ESP32 HUZZAH32 firmware to control the stepper motor and RGB LED from serial feedback in a FreeRTOS setting
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
  RECYCLE_BIN         = 0,
  COMPOST_BIN         = 1,
  ELECTRONICS_BIN     = 2,
  DEFAULT_BIN         = 3,
  
} bin_t;

#define STEPPER_IN1 A0
#define STEPPER_IN2 A1
#define STEPPER_IN3 A2
#define STEPPER_IN4 A3
#define NUMBER_OF_STEPS_PER_REV 2048
#define SCALE_FACTOR 1.03

#include <Stepper.h>
Stepper stepper = Stepper(NUMBER_OF_STEPS_PER_REV, STEPPER_IN1, STEPPER_IN2, STEPPER_IN3, STEPPER_IN4);

static uint8_t current_bin = DEFAULT_BIN;
static volatile uint8_t next_bin = ELECTRONICS_BIN;

/**
 * RGB LED
 */
typedef enum {
  LED_IDLE    = 0,
  LED_PENDING = 1,
  LED_APPROVE = 2,
  
} led_status_t;

#define RGB_RED   A11
#define RGB_GREEN A10
#define RGB_BLUE  A9

static volatile uint8_t led_status = LED_IDLE;

void rgb_write(int red, int green, int blue);


/////////////////////////////////////////////////////////////////////////////////////////////////////
// Task Functions
/////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * @brief: Receives bin movement commands and relays to the stepper motor
 * @param[in]: N/A -- need void pointer for FreeRTOS implementation
 */
void control_stepper(void * no_params) {
  // Set speed, 10 rpm is about the limit
  stepper.setSpeed(5);

  while (true) {
    if (next_bin != current_bin) {
      led_status = LED_PENDING;
                  
      // Calculate how many bins over the next bin is
      int moves = (current_bin <= next_bin) ? (next_bin - current_bin) : (next_bin + 4 - current_bin);
      
      // Move the stepper motor
      int steps = (int) SCALE_FACTOR * (moves * NUMBER_OF_STEPS_PER_REV) / 4;
      stepper.step(steps);

      // Update the current bin
      current_bin = next_bin;
      
      led_status = LED_APPROVE;
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

  while (true) {
    switch (led_status) {
      case LED_IDLE:
      {
        // Display white
        rgb_write(255, 255, 255);

        break;
      }
      
      case LED_PENDING:
      {
        // Display red
        rgb_write(255, 0, 0);
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
 * @brief: Parses commands from the serial input and copies them into the corresponding volatile variables
 */
void parse_serial_commands(void * no_params) {
  pinMode(LED_BUILTIN, OUTPUT);
  
  while (true) {
    if (Serial.available())
    {
      digitalWrite(LED_BUILTIN, HIGH);
      char readByte = Serial.read();
      next_bin = readByte - '0';
    }
  }
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

  xTaskCreatePinnedToCore(
    parse_serial_commands,
    "Parse Serial Commands",
    1024,
    NULL,
    1, // Lowest priority
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
