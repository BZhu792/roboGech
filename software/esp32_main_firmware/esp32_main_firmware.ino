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
#define SCALE_FACTOR 1.07

#include <Stepper.h>
Stepper stepper = Stepper(NUMBER_OF_STEPS_PER_REV, STEPPER_IN1, STEPPER_IN2, STEPPER_IN3, STEPPER_IN4);

static uint8_t current_bin = DEFAULT_BIN;
static uint8_t next_bin = DEFAULT_BIN;

/**
 * RGB LED
 */
#define RGB_RED   D6
#define RGB_GREEN D5
#define RGB_BLUE  D4

void rgb_write(int red, int green, int blue);


/**
 * @brief: Entry point to FreeRTOS framework
 */
void setup() {
  Serial.begin(115200);
  
  pinMode(RGB_RED, OUTPUT);
  pinMode(RGB_GREEN, OUTPUT);
  pinMode(RGB_BLUE, OUTPUT);
  
  // Set speed, 10 rpm is about the limit
  stepper.setSpeed(10);

  
  rgb_write(0, 0, 255);
  int steps = (int) SCALE_FACTOR * NUMBER_OF_STEPS_PER_REV;
  stepper.step(-steps);
}

void loop() {
    rgb_write(255, 255, 255);
    
    if (next_bin != current_bin) {
      rgb_write(255, 0, 0);
                  
      // Calculate how many bins over the next bin is
      int moves = (current_bin <= next_bin) ? (next_bin - current_bin) : (next_bin + 4 - current_bin);
      
      // Move the stepper motor
      int steps = (int) SCALE_FACTOR * (moves * NUMBER_OF_STEPS_PER_REV) / 4;
      stepper.step(-steps);

      // Update the current bin
      current_bin = next_bin;
      
      rgb_write(0, 255, 0);
      delay(2000);
    }
    
    if (Serial.available())
    {
      char readByte = Serial.read();
      next_bin = readByte - '0';
    }
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
