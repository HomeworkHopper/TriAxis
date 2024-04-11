#include <SPI.h>
#include <Adafruit_SH110X.h>
#include <Adafruit_MPU6050.h>

// System pinout (final)
//               _________
// BAT_CHARGE > |D0/A0  5v|
// MPU_A_INT  > |D1    GND|
// MPU_B_INT  > |D2    3v3|
// Pushbutton > |D3    D10| < MOSI (SPI)
// SDA (I2C)  > |D4     D9| < MISO (SPI) [unused]
// SCL (I2C)  > |D5     D8| < CLK (SPI)
// OLED_DC    > |D6     D7| < OLED_EN
//               ‾‾‾‾‾‾‾‾‾
// Note: Attach OLED RST pin to the EN pin of the XIAO

#define BAT_CHARGE_PIN A0    // Battery Analog Read (see BAT_READ_SAMPLES)
#define MPU_A_INT_PIN  D1    // MPU A Interrupt (Data Ready + GPIO Wakeup)
#define MPU_B_INT_PIN  D2    // MPU B Interrupt (Data Ready + GPIO Wakeup)
#define PUTBUTTON_PIN  D3    // Pushbutton (Menu Navigation + GPIO Wakeup)
#define OLED_DC_PIN    D6    // OLED DC pin (crutial for proper operation)
#define OLED_EN_PIN    D7    // OLED EN pin (TODO: Could we hold this low)

#define BAT_READ_SAMPLES 16  // Samples to take when reading battery voltage

#define OLED_WIDTH  128      // Width of the OLED screen, in pixels
#define OLED_HEIGHT 64       // Height of the OLED screen, in pixels

#define MPU_A_ADDR MPU6050_I2CADDR_DEFAULT      // AD0 must be pulled low (default)
#define MPU_B_ADDR MPU6050_I2CADDR_DEFAULT + 1  // AD0 must be pulled high (~3.3v)

#define OLED_CLEAR(oled) do {                         \
                     oled.clearDisplay();             \
                     oled.setTextSize(1);             \
                     oled.setTextColor(SH110X_WHITE); \
                     oled.setCursor(0, 0);            \
                   } while(0);

// OLED Display
static Adafruit_SH1106G oled_display = Adafruit_SH1106G(OLED_WIDTH, OLED_HEIGHT, &SPI, OLED_DC_PIN, /*IGNORE RST PIN*/ -1, OLED_EN_PIN);

// MPU A, B
static Adafruit_MPU6050 mpu_a, mpu_b;

// MPU status
static bool mpu_a_status, mpu_b_status;

// MPU Data Capture Structs
typedef struct { float x; float y; float z; } capture_component_t;
struct mpu_capture_t {
  uint32_t timestamp;
  sensors_event_t accel;
  sensors_event_t gyro;
  sensors_event_t temp;
} static mpu_a_capture = { 0 }, mpu_b_capture = { 0 };

static bool configureMPU(Adafruit_MPU6050 *mpu, uint8_t addr) {
  if (mpu->begin(addr)) {
    mpu->setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu->setGyroRange(MPU6050_RANGE_500_DEG);
    mpu->setFilterBandwidth(MPU6050_BAND_21_HZ);
    return true;
  }
  return false;
}

static inline void captureMPU(Adafruit_MPU6050 *mpu, struct mpu_capture_t * const capture) {
  const uint32_t timestamp = micros();
  mpu->getEvent(&capture->accel, &capture->gyro, &capture->temp);
}

static void MPU_A_ISR() { captureMPU(&mpu_a, &mpu_a_capture); }
static void MPU_B_ISR() { captureMPU(&mpu_b, &mpu_b_capture); }

static inline float readBatteryVoltage() {
  static constexpr uint32_t Vdivisor = 500 * BAT_READ_SAMPLES;

  uint32_t Vtotal = 0;
  for(int i = 0; i < BAT_READ_SAMPLES; Vtotal += analogReadMilliVolts(BAT_CHARGE_PIN), i++) {}
  return Vtotal / Vdivisor;
}

static inline void error(Adafruit_SH1106G *oled = nullptr, const char *msg = "General Error") {
  if(oled) {
    Adafruit_SH1106G _oled = *oled;
    OLED_CLEAR(_oled);
    _oled.println("Error:");
    _oled.print(msg);
  }

  while(true) { delay(10); }
}

void setup() {

  // Configure OLED Display
  if(!oled_display.begin()) {
    error();
  }

  // Configure MPU A
  if(!configureMPU(&mpu_a, MPU_A_ADDR)) {
    error(&oled_display, "Failed to configure MPU A");
  }

  // Configure MPU B
  if(!configureMPU(&mpu_b, MPU_B_ADDR)) {
    error(&oled_display, "Failed to configure MPU B");
  }

  // Show splash image
  oled_display.display();

  // WAIT FOR PUSH BUTTON HERE
}

void loop() {

  OLED_CLEAR(oled_display);

  oled_display.print("Battery: ");
  oled_display.print(readBatteryVoltage());
  oled_display.println("V");

  noInterrupts();
  struct mpu_capture_t captureACopy = mpu_a_capture;
  struct mpu_capture_t captureBCopy = mpu_b_capture;
  interrupts();

  oled_display.display();
  delay(500);
}
