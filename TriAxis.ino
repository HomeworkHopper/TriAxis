#include "mpu9250.h"

// System pinout (final)
//               _________
// BAT_CHARGE > |D0     5v| < USB-C PWR
// MPU_A_INT  > |D1    GND| < Common Ground
// MPU_B_INT  > |D2    3v3| < Battery PWR
// MPU_C_INT  > |D3    D10| < SPI_MOSI  
// MPU_A_EN   > |D4     D9| < SPI_MISO  
// MPU_B_EN   > |D5     D8| < SPI_CLK  
// MPU_C_EN   > |D6     D7| < STATUS_LED (NPN + PNP resistor)
//               ‾‾‾‾‾‾‾‾‾

#define BAT_CHARGE_PIN D0    // Battery Analog Read
#define MPU_A_INT_PIN  D1    // MPU A Interrupt
#define MPU_B_INT_PIN  D2    // MPU B Interrupt
#define MPU_C_INT_PIN  D3    // MPU C Interrupt
#define MPU_A_EN_PIN   D4    // MPU A Enable
#define MPU_B_EN_PIN   D5    // MPU B Enable
#define MPU_C_EN_PIN   D6    // MPU C Enable
#define STATUS_LED_PIN D7    // Status LED

bfs::Mpu9250 mpu_a(&SPI, MPU_A_EN_PIN);    // MPU A
bfs::Mpu9250 mpu_b(&SPI, MPU_B_EN_PIN);    // MPU B
bfs::Mpu9250 mpu_c(&SPI, MPU_C_EN_PIN);    // MPU C

typedef enum { STARTING, RUNNING, SLEEPING, ERROR } SYSTEM_STATE;
volatile RTC_DATA_ATTR SYSTEM_STATE system_state = STARTING;    // System State

void MPU_A_ISR() {}
void MPU_B_ISR() {}
void MPU_C_ISR() {}

bool configureMPU(bfs::Mpu9250 mpu) {
  return mpu.Begin()        // Establish communication with MPU
    && mpu.ConfigSrd(19)    // Set MPU sample rate
    && mpu.EnableDrdyInt(); // Enable data ready interrupts
}

SYSTEM_STATE processStateRunning() {
}

SYSTEM_STATE processStateSleeping() {

  noInterrupts();
  // esp_sleep_enable_ext1_wakeup(ext_wakeup_pin_1_mask | ext_wakeup_pin_2_mask, ESP_EXT1_WAKEUP_ANY_HIGH);
  esp_deep_sleep_enable_gpio_wakeup(BIT(MPU_A_INT_PIN), ESP_GPIO_WAKEUP_GPIO_HIGH);
  esp_deep_sleep_start();    // Execution will stop here
  
  return RUNNING;
}

// SYSTEM_STATE (*)()

void setup() {

  struct timeval now;
    gettimeofday(&now, NULL);
    int sleep_time_ms = (now.tv_sec - sleep_enter_time.tv_sec) * 1000 + (now.tv_usec - sleep_enter_time.tv_usec) / 1000;


  if(system_state == SLEEPING) {

  } else {

  }
  
  // Initialize serial
  Serial.begin(115200);
  while(!Serial) {}

  // Start the SPI bus
  SPI.begin();

  noInterrupts();
  if(configureMPU(mpu_a) && configureMPU(mpu_b) && configureMPU(mpu_c)) {
    system_state = RUNNING;
  } else {
    Serial.println("Unable to configure the MPUs");
    system_state = ERROR;
  }
}

void loop() {
  switch(system_state) {
    case RUNNING:
      system_state = processStateRunning();
      break;
    case SLEEPING:
      system_state = processStateSleeping();
      break;
    case ERROR:
      
      while(true) ;
    default:
      system_state = ERROR;
      break;
  }
}
