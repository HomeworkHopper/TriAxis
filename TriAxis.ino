#include "mpu9250.h"

// System pinout (final)
//               _________
// BAT_CHARGE > |D0     5v| < USB-C PWR
// MPU_A_INT  > |D1    GND| < Common Ground
// MPU_B_INT  > |D2    3v3| < Battery PWR
// MPU_C_INT  > |D3    D10| < SPI_MOSI  
// MPU_A_EN   > |D4     D9| < SPI_MISO  
// MPU_B_EN   > |D5     D8| < SPI_CLK  
// MPU_C_EN   > |D6     D7| < STATUS_LED (NPN + PNP transistor)
//               ‾‾‾‾‾‾‾‾‾

#define BAT_CHARGE_PIN D0    // Battery Analog Read
#define MPU_A_INT_PIN  D1    // MPU A Interrupt
#define MPU_B_INT_PIN  D2    // MPU B Interrupt
#define MPU_C_INT_PIN  D3    // MPU C Interrupt
#define MPU_A_EN_PIN   D4    // MPU A Enable
#define MPU_B_EN_PIN   D5    // MPU B Enable
#define MPU_C_EN_PIN   D6    // MPU C Enable
#define STATUS_LED_PIN D7    // Status LED

#define MPU_SAMPLE_RATE_HZ 100

typedef struct { float x; float y; float z; } component_t;
typedef struct {
  component_t accel;
  component_t gyro;
  component_t mag;
  float temp;
} mpu_capture_t;

bfs::Mpu9250 mpu_a(&SPI, MPU_A_EN_PIN);    // MPU A
bfs::Mpu9250 mpu_b(&SPI, MPU_B_EN_PIN);    // MPU B
bfs::Mpu9250 mpu_c(&SPI, MPU_C_EN_PIN);    // MPU C

mpu_capture_t* mpu_a_capture = NULL;
mpu_capture_t* mpu_b_capture = NULL;
mpu_capture_t* mpu_c_capture = NULL;

void mpu_read(bfs::Mpu9250 mpu, mpu_capture_t* ret) {
  if (mpu.Read()) {
    ret->accel.x = mpu.accel_x_mps2();
    ret->accel.y = mpu.accel_y_mps2();
    ret->accel.z = mpu.accel_z_mps2();
    ret->gyro.x = mpu.gyro_x_radps();
    ret->gyro.y = mpu.gyro_y_radps();
    ret->gyro.z = mpu.gyro_z_radps();
    ret->mag.x = mpu.mag_x_ut();
    ret->mag.y = mpu.mag_y_ut();
    ret->mag.z = mpu.mag_z_ut();
    ret->temp = mpu.die_temp_c();
  }
}

void MPU_A_ISR() { mpu_read(mpu_a, mpu_a_capture); }
void MPU_B_ISR() { mpu_read(mpu_b, mpu_b_capture); }
void MPU_C_ISR() { mpu_read(mpu_c, mpu_c_capture); }

bool configureMPU(bfs::Mpu9250 mpu, uint8_t pin, void (*isr)()) {
  static constexpr unsigned int mpu_srd = ((1000 - MPU_SAMPLE_RATE_HZ) / MPU_SAMPLE_RATE_HZ);
  if(mpu.Begin() && mpu.ConfigSrd(mpu_srd) && mpu.EnableDrdyInt()) {
    attachInterrupt(pin, isr, RISING);
    return true;
  }
  return false;
}

void setup() {
  
  Serial.begin(115200);
  while(!Serial) {}

  SPI.begin();

  noInterrupts();
  if(!configureMPU(mpu_a, MPU_A_INT_PIN, MPU_A_ISR) || !configureMPU(mpu_b, MPU_B_INT_PIN, MPU_B_ISR) || !configureMPU(mpu_c, MPU_C_INT_PIN, MPU_C_ISR)) {
    Serial.println("Unable to configure the MPUs");
  }
  interrupts();
}

void loop() {
}
