#include "libraries/invensense-imu/src/mpu9250.h"

// System pinout (final)
//               _________
// BAT_CHARGE > |D0     5v|
// MPU_A_INT  > |D1    GND|
// MPU_B_INT  > |D2    3v3|
// MPU_C_INT  > |D3    D10| < SPI_MOSI
// MPU_A_EN   > |D4     D9| < SPI_MISO & OLED_DC
// MPU_B_EN   > |D5     D8| < SPI_CLK
// MPU_C_EN   > |D6     D7| < OLED_EN
//               ‾‾‾‾‾‾‾‾‾

#define BAT_CHARGE_PIN D0    // Battery Analog Read
#define MPU_A_INT_PIN  D1    // MPU A Interrupt
#define MPU_B_INT_PIN  D2    // MPU B Interrupt
#define MPU_C_INT_PIN  D3    // MPU C Interrupt
#define MPU_A_EN_PIN   D4    // MPU A Enable
#define MPU_B_EN_PIN   D5    // MPU B Enable
#define MPU_C_EN_PIN   D6    // MPU C Enable
#define OLED_EN_PIN    D7    // OLED Enable
#define OLED_DC_PIN    D9    // OLED DC pin, NOTE: D9 is also the MISO pin, 
                             // but the OLED display does not use the MISO pin,
                             // so we're able to use it as the DC pin. The Adafruit
                             // library had to be slightly modified to support this

#define MPU_SAMPLE_RATE 100  // The accelerometer/gyroscope can sample at up to 1000hz (1Mhz),
                             // but the magnometer can only run at 100hz, max. In practice,
                             // there would be no harm in running the sensor at 1Mhz, but for
                             // development and debugging purposes I want to keep it at 100hz

typedef struct { float x; float y; float z; } component_t;
struct mpu_capture_t {
  unsigned long time;
  component_t accel;
  component_t gyro;
  component_t mag;
  float temp;
} volatile
*const mpu_a_capture = NULL,
*const mpu_b_capture = NULL,
*const mpu_c_capture = NULL;

bfs::Mpu9250 mpu_a(&SPI, MPU_A_EN_PIN);    // MPU A
bfs::Mpu9250 mpu_b(&SPI, MPU_B_EN_PIN);    // MPU B
bfs::Mpu9250 mpu_c(&SPI, MPU_C_EN_PIN);    // MPU C

/*


static inline void mpu_capture(bfs::Mpu9250 mpu, struct mpu_capture_t *ret) {
  if (mpu.Read()) {
    ret->time = micros();
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

*/

void MPU_A_ISR() {  }
void MPU_B_ISR() { }
void MPU_C_ISR() {  }

bool configureMPU(bfs::Mpu9250 mpu, uint8_t interruptPin, void (*isr)()) {
  static constexpr unsigned int mpu_srd = ((1000 - MPU_SAMPLE_RATE_HZ) / MPU_SAMPLE_RATE_HZ);
  if(mpu.Begin() && mpu.ConfigSrd(mpu_srd) && mpu.EnableDrdyInt()) {
    pinMode(interruptPin, INPUT);    // Default pinmode
    attachInterrupt(interruptPin, isr, RISING);
    return true;
  }
  return false;
}

void error() {
  while(true){}
}

void setup() {
  
  Serial.begin(115200);
  while(!Serial) {}

  SPI.begin();

  interrupts();
  if(!configureMPU(mpu_a, MPU_A_INT_PIN, MPU_A_ISR) || !configureMPU(mpu_b, MPU_B_INT_PIN, MPU_B_ISR) || !configureMPU(mpu_c, MPU_C_INT_PIN, MPU_C_ISR)) {
    Serial.println("Unable to configure the MPUs");
    error();
  }
  noInterrupts();
}

void loop() {
}
