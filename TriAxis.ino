#include <SPI.h>
#include <Adafruit_SH110X.h>
#include <Adafruit_MPU6050.h>

// System pinout (final)
//               _________
// BAT_CHARGE > |D0     5v|
// MPU_A_INT  > |D1    GND|
// MPU_B_INT  > |D2    3v3|
// Pushbutton > |D3    D10| < MOSI (SPI)
// SDA (I2C)  > |D4     D9| < MISO (SPI, not used by OLED)
// SCL (I2C)  > |D5     D8| < CLK (SPI)
// OLED_DC    > |D6     D7| < OLED_EN
//               ‾‾‾‾‾‾‾‾‾
// Note: Attach OLED RST pin to the EN pin of the XIAO

#define BAT_CHARGE_PIN D0    // Battery Analog Read
#define MPU_A_INT_PIN  D1    // MPU A Interrupt
#define MPU_B_INT_PIN  D2    // MPU B Interrupt
#define PUTBUTTON_PIN  D3    // Pushbutton

#define OLED_DC_PIN    D6    // OLED DC pin
#define OLED_EN_PIN    D7    // OLED EN pin

#define MPU_A_ADDR MPU6050_I2CADDR_DEFAULT      // AD0 must be pulled low (default)
#define MPU_B_ADDR MPU6050_I2CADDR_DEFAULT + 1  // AD0 must be pulled high (~3.3v)

#define BAT_READ_SAMPLES 16

// OLED Display
static Adafruit_SH1106G display = Adafruit_SH1106G(128, 64, &SPI, OLED_DC_PIN, /*IGNORE RST PIN*/ -1, OLED_EN_PIN);

// MPU A, B
static Adafruit_MPU6050 mpu_a, mpu_b;

// MPU status
static bool mpu_a_status, mpu_b_status;

// MPU Data Capture Structs
typedef struct { float x; float y; float z; } capture_component_t;
struct mpu_capture_t {
  unsigned long time;
  capture_component_t accel;
  capture_component_t gyro;
  float temp;
} static volatile mpu_a_capture = {
  0,
  {0.0, 0.0, 0.0},
  {0.0, 0.0, 0.0},
  0.0
}, mpu_b_capture = {
  0,
  {0.0, 0.0, 0.0},
  {0.0, 0.0, 0.0},
  0.0
};

static bool configureMPU(Adafruit_MPU6050 *mpu, uint8_t addr) {
  if (mpu->begin(addr)) {
    mpu->setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu->setGyroRange(MPU6050_RANGE_500_DEG);
    mpu->setFilterBandwidth(MPU6050_BAND_21_HZ);
    return true;
  }
  return false;
}

static inline void captureMPU(const Adafruit_MPU6050 *mpu, struct mpu_capture_t volatile * const capture) {}

static void MPU_A_ISR() { captureMPU(&mpu_a, &mpu_a_capture); }
static void MPU_B_ISR() { captureMPU(&mpu_b, &mpu_b_capture); }

static inline float readBatteryVoltage() {
  static constexpr uint32_t Vdivisor = 500 * BAT_READ_SAMPLES;
  for(uint32_t i = 1, e = BAT_READ_SAMPLES, Vtotal = 0; true; Vtotal += analogReadMilliVolts(BAT_CHARGE_PIN), i++) {
    if(i > e) {
      return Vtotal / Vdivisor;
    }
  }
}

void setup() {

  pinMode(A0, INPUT);

  mpu_a_status = configureMPU(&mpu_a, MPU_A_ADDR);
  mpu_b_status = configureMPU(&mpu_b, MPU_B_ADDR);

  display.begin();
  display.display();
  delay(2000);
}

static sensors_event_t a, g, temp;

void loop() {

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
  } else {
    display.println("ERROR");
  }

  display.println();

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
