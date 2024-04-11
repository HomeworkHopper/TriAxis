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


static inline uint8_t __ENTER(void) {
  taskENTER_CRITICAL(nullptr);
  return 1;
}

static inline void __EXIT(const uint8_t *dummy) {
  taskEXIT_CRITICAL(nullptr)
  __asm__ volatile ("" ::: "memory");
}

#define ATOMIC_BLOCK for (uint8_t dummy __attribute__((__cleanup__(__EXIT))) = 0, __ToDo = __ENTER(); __ToDo ; __ToDo = 0 )

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

// MPU Data Capture Structs
typedef struct {
  float x;                     //  4 bytes
  float y;                     //  4 bytes
  float z;                     //  4 bytes
} capture_component_t;

struct mpu_capture_t {
  capture_component_t accel;   //  12 bytes
  capture_component_t gyro;    //  12 bytes
  uint32_t temp;               //  4 bytes
  volatile int64_t timestamp;  //  4 bytes
} static mpu_a_capture, mpu_b_capture;

static bool configureMPU(Adafruit_MPU6050 *mpu, uint8_t addr) {
  if (mpu->begin(addr)) {
    mpu->setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu->setGyroRange(MPU6050_RANGE_500_DEG);
    mpu->setFilterBandwidth(MPU6050_BAND_21_HZ);
    return true;
  }
  return false;
}

#define NEW_CAPTURE(capture) capture.timestamp >= 0
#define VALIDATE_CAPTURE(capture) capture.timestamp = esp_timer_get_time()
#define INVALIDATE_CAPTURE (capture) capture.timestamp = -1

void MPU_A_ISR() { VALIDATE_CAPTURE(mpu_a_capture); }
void MPU_B_ISR() { VALIDATE_CAPTURE(mpu_b_capture); }

  //mpu->getEvent(&capture->accel, &capture->gyro, &capture->temp);
static inline float readBatteryVoltage() {
  static constexpr uint32_t Vdivisor = 500 * BAT_READ_SAMPLES;

  uint32_t Vtotal = 0;
  for(int i = 0; i < BAT_READ_SAMPLES; i++) {
    Vtotal += analogReadMilliVolts(BAT_CHARGE_PIN);
  }
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

  if(NEW_CAPTURE(mpu_a_capture) || NEW_CAPTURE(mpu_b_capture)) {
    ATOMIC_BLOCK {
    }
  }

  oled_display.display();
  delay(500);
}
