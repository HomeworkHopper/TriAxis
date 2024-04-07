#include <SPI.h>
#include <Adafruit_SH110X.h>
#include <Adafruit_MPU6050.h>

// System pinout (final)
//               _________
// BAT_CHARGE > |D0     5v|
// MPU_A_INT  > |D1    GND|
// MPU_B_INT  > |D2    3v3|
//            > |D3    D10| < MOSI (SPI)
// SDA (I2C)  > |D4     D9| < MISO (SPI, not used by OLED)
// SCL (I2C)  > |D5     D8| < CLK (SPI)
// OLED_DC    > |D6     D7| < OLED_EN
//               ‾‾‾‾‾‾‾‾‾
// Note: Attach OLED RST pin to the EN pin of the XIAO

#define BAT_CHARGE_PIN D0    // Battery Analog Read
#define MPU_A_INT_PIN  D1    // MPU A Interrupt
#define MPU_B_INT_PIN  D2    // MPU B Interrupt
#define OLED_EN_PIN    D7    // OLED EN pin
#define OLED_DC_PIN    D6    // OLED DC pin

#define MPU_SAMPLE_RATE 1000

// OLED Display
static Adafruit_SH1106G display = Adafruit_SH1106G(128, 64, &SPI, OLED_DC_PIN, /*IGNORE RST PIN*/ -1, OLED_EN_PIN);

// MPU A, B
static Adafruit_MPU6050 mpu_a, mpu_b;

// MPU Data Capture Structs
typedef struct { float x; float y; float z; } capture_component_t;
struct mpu_capture_t {
  unsigned long time;
  capture_component_t accel;
  capture_component_t gyro;
  float temp;
} static volatile mpu_a_capture, mpu_b_capture;

/*
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
    capture->temp = mpu.die_temp_c();
  } else {

  }
}
*/

/*
void MPU_A_ISR() { mpu_capture(mpu_a, &mpu_a_capture); }
void MPU_B_ISR() { mpu_capture(mpu_b, &mpu_b_capture); }
*/

bool configureMPU(Adafruit_MPU6050 *mpu, uint8_t addr) {
  if (!mpu->begin(addr)) {
    Serial.println("Failed to find MPU6050 chip");
    return false;
  }
  Serial.println("MPU6050 Found!");

  mpu->setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu->setGyroRange(MPU6050_RANGE_500_DEG);
  mpu->setFilterBandwidth(MPU6050_BAND_21_HZ);

  return true;
}


float readBatteryVoltage() {
  uint32_t Vbatt = 0;
  for(int i = 0; i < 16; i++) {
    Vbatt = Vbatt + analogReadMilliVolts(A0);  
  }
  return 2 * Vbatt / 16 / 1000.0;
}

// NEVER call this function directly from an IRQ! (oops)
static volatile bool global_error = false;
static inline void error(char *msg) {

  Serial.print("error(): ");
  Serial.print(msg);
  Serial.flush(); // Push out all remaining serial data before the stop interrupts.
                  // This is important because serial transmissions are interrupt-based
  noInterrupts(); // Ignore all future interrupts
  while(true){}   // Loop until reset
}

static bool mpu_a_status, mpu_b_status;

void setup() {
  
  Serial.begin(115200);
  while(!Serial) { delay(10); }

  pinMode(A0, INPUT);

  mpu_a_status = configureMPU(&mpu_a, MPU6050_I2CADDR_DEFAULT);
  mpu_b_status = configureMPU(&mpu_b, MPU6050_I2CADDR_DEFAULT + 1);

  Serial.println(mpu_a_status);
  Serial.println(mpu_b_status);

  // display.setContrast (0);
  display.begin();
  display.display();
  delay(2000);
}

static sensors_event_t a, g, temp;

void loop() {
  // If an error occured (probably in an ISR) then we need to stop
  if(global_error) {
    error("Error");  // Never returns
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);

  display.print("Battery: ");
  display.print(readBatteryVoltage());
  display.println("V");
  display.println("");

  if(mpu_a_status) {
    mpu_a.getEvent(&a, &g, &temp);

    /* Print out the values */
    display.print(a.acceleration.x);
    display.print(", ");
    display.print(a.acceleration.y);
    display.print(", ");
    display.println(a.acceleration.z);
    display.print(g.gyro.x);
    display.print(", ");
    display.print(g.gyro.y);
    display.print(", ");
    display.println(g.gyro.z);

    display.println("");
  } else {
    display.println("ERROR");
  }

  if(mpu_b_status) {
    mpu_b.getEvent(&a, &g, &temp);
    display.print(a.acceleration.x);
    display.print(", ");
    display.print(a.acceleration.y);
    display.print(", ");
    display.println(a.acceleration.z);
    display.print(g.gyro.x);
    display.print(", ");
    display.print(g.gyro.y);
    display.print(", ");
    display.println(g.gyro.z);
  } else {
    display.println("ERROR");
  }

  display.display();
  delay(500);
}
