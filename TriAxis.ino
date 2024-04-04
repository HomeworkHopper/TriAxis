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

typedef struct { float x; float y; float z; } capture_component_t;
struct mpu_capture_t {
  unsigned long time;
  capture_component_t accel;
  capture_component_t gyro;
  capture_component_t mag;
  float temp;
} static volatile mpu_a_capture, mpu_b_capture, mpu_c_capture;

bfs::Mpu9250 mpu_a(&SPI, MPU_A_EN_PIN);    // MPU A
bfs::Mpu9250 mpu_b(&SPI, MPU_B_EN_PIN);    // MPU B
bfs::Mpu9250 mpu_c(&SPI, MPU_C_EN_PIN);    // MPU C

// This should probably be a macro. I need to spend some time considering if it will expand properly in all cases, though... (should there really ever be an "if" statement in a macro!?)
static inline void mpu_capture(bfs::Mpu9250 mpu, struct mpu_capture_t volatile *capture) {
  if (mpu.Read()) { // Should always be true, since this function is invoked in reasponse to a data-ready interrupt
    capture->time = micros();
    capture->accel.x = mpu.accel_x_mps2();
    capture->accel.y = mpu.accel_y_mps2();
    capture->accel.z = mpu.accel_z_mps2();
    capture->gyro.x = mpu.gyro_x_radps();
    capture->gyro.y = mpu.gyro_y_radps();
    capture->gyro.z = mpu.gyro_z_radps();
    capture->mag.x = mpu.mag_x_ut();
    capture->mag.y = mpu.mag_y_ut();
    capture->mag.z = mpu.mag_z_ut();
    capture->temp = mpu.die_temp_c();
  } else {
    error("Attempted to capture MPU data when none was present");
  }
}

void MPU_A_ISR() { mpu_capture(mpu_a, &mpu_a_capture); }
void MPU_B_ISR() { mpu_capture(mpu_b, &mpu_b_capture); }
void MPU_C_ISR() { mpu_capture(mpu_c, &mpu_c_capture); }

bool configureMPU(bfs::Mpu9250 mpu, uint8_t interruptPin, void (*isr)()) {
  static constexpr unsigned int mpu_srd = ((1000 - MPU_SAMPLE_RATE) / MPU_SAMPLE_RATE);
  if(mpu.Begin() && mpu.ConfigSrd(mpu_srd) && mpu.EnableDrdyInt()) {
    pinMode(interruptPin, INPUT);    // Default pinmode
    attachInterrupt(interruptPin, isr, RISING);
    return true;
  }
  return false;
}

// NEVER call this function directly from an IRQ! (oops)
static volatile 
static inline void error(char *msg) {

  Serial.print("error(): ");
  Serial.print(msg);
  Serial.flush(); // Push out all remaining serial data before the stop interrupts.
                  // This is important because serial transmissions are interrupt-based

  noInterrupts(); // Ignore all future interrupts
  while(true){}   // Loop until reset
}

void setup() {
  
  Serial.begin(115200);
  while(!Serial) {}

  SPI.begin();

  interrupts();
  if(!configureMPU(mpu_a, MPU_A_INT_PIN, MPU_A_ISR) || !configureMPU(mpu_b, MPU_B_INT_PIN, MPU_B_ISR) || !configureMPU(mpu_c, MPU_C_INT_PIN, MPU_C_ISR)) {
    error("Unable to configure the MPUs");
  }
  noInterrupts();
}

void loop() {
}
