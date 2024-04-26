#include <SPI.h>
#include <Adafruit_SH110X.h>
#include <Adafruit_MPU6050.h>

#include "AtomicBlock.h"

// System pinout
//               _________
// BAT_CHARGE > |A0     5v|
// MPU_A_INT  > |D1    GND|
// MPU_B_INT  > |D2    3v3|
// Pushbutton > |D3    D10| < MOSI (SPI)
// SDA (I2C)  > |D4     D9| < MISO (SPI) [unused]
// SCL (I2C)  > |D5     D8| < CLK (SPI)
// OLED_DC    > |D6     D7| < OLED_EN
//               ‾‾‾‾‾‾‾‾‾
// Note: OLED RST pin attached to the EN pin of the XIAO

#define BAT_CHARGE_PIN A0    // Battery Analog Read (see BAT_READ_SAMPLES)
#define MPU_A_INT_PIN  D1    // MPU A Interrupt (Data Ready + GPIO Wakeup)
#define MPU_B_INT_PIN  D2    // MPU B Interrupt (Data Ready + GPIO Wakeup)
#define PUSHBUTTON_PIN D3    // Pushbutton (Menu Navigation + GPIO Wakeup)
#define OLED_DC_PIN    D6    // OLED DC pin (crutial for proper operation)
#define OLED_EN_PIN    D7    // OLED EN pin (TODO: Could we hold this low)

#define BAT_READ_SAMPLES 16  // Samples to take when reading battery voltage

#define OLED_WIDTH  128      // Width of the OLED screen, in pixels
#define OLED_HEIGHT 64       // Height of the OLED screen, in pixels

#define MPU_A_ADDR MPU6050_I2CADDR_DEFAULT      // AD0 must be pulled low (sensor has it's own pull-down)
#define MPU_B_ADDR MPU6050_I2CADDR_DEFAULT + 1  // AD0 must be pulled high (~3.3v)

#define OLED_CLEAR(oled) do {                         \
                     oled.clearDisplay();             \
                     oled.setTextSize(1);             \
                     oled.setTextColor(SH110X_WHITE); \
                     oled.setCursor(0, 0);            \
                   } while(0);

// OLED Display
static Adafruit_SH1106G oled_display = Adafruit_SH1106G(OLED_WIDTH, OLED_HEIGHT, &SPI, OLED_DC_PIN, /*IGNORE RST PIN*/ -1, OLED_EN_PIN);

// The MPU sensors
static Adafruit_MPU6050 mpu_a, mpu_b;

// Data capture structures
struct triaxis_capture_t {
  struct { float x, y, z; } accel;
  struct { float x, y, z; } gyro;
  int64_t dataTimestamp;
  float temp;
};

// The TRI structures (basically just MPU sensors with packaged metadata)
struct triaxis_sensor_t {
  volatile int64_t dataTimestamp;
  Adafruit_MPU6050 *mpuSensor;
  volatile bool dataReady;
} static tri_a, tri_b;

// TRI structure operations
#define GET_TRI_DATA_RDY(tri) tri.dataReady
#define SET_TRI_DATA_RDY(tri) tri.dataTimestamp = esp_timer_get_time(); tri.dataReady = true;
#define CLR_TRI_DATA_RDY(tri) tri.dataReady = false

// Interrupt Service Routines (DATA READY)
void MPU_A_ISR() { SET_TRI_DATA_RDY(tri_a); }
void MPU_B_ISR() { SET_TRI_DATA_RDY(tri_b); }

// Push button status (and interrupt)
static volatile bool pushbutton_interrupt;
void PUSHBUTTON_ISR() {
  pushbutton_interrupt = true;
}

// Configure MPU sensor
static inline bool configureMPU(Adafruit_MPU6050 *mpu, const uint8_t i2cAddr) {
  if (mpu && mpu->begin(i2cAddr, &Wire)) {
    mpu->setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu->setGyroRange(MPU6050_RANGE_500_DEG);
    mpu->setFilterBandwidth(MPU6050_BAND_21_HZ);
    return true;
  } else {
    return false;
  }
}

// Configure TRI structure
static inline bool configureTRI(struct triaxis_sensor_t *tri, Adafruit_MPU6050 *mpu) {
  if(mpu) {
    tri->dataTimestamp = -1;
    tri->mpuSensor = mpu;
    tri->dataReady = false;
    return true;
  } else {
    return false;
  }
}

// Data processing function which displays readings on the OLED screen
static inline void processData(const struct triaxis_sensor_t  *tri,
    void (*processingFunction)(const struct triaxis_capture_t *capture)) {
  
  // Data storage
  static sensors_event_t a, g, t;
  static triaxis_capture_t captureData;

  // Read sensor data
  tri->mpuSensor->getEvent(&a, &g, &t);

  // Copy timestamp (atomically)
  ATOMIC_BLOCK {
    // This timestamp could have ^just^ changed, but we can't do much about that
    // Something we CAN do, though, is copy it atomically so we don't merge two timestamps into one!
    captureData.dataTimestamp = tri->dataTimestamp;
  }

  // Copy acceleration data
  captureData.accel.x = a.acceleration.x;
  captureData.accel.y = a.acceleration.y;
  captureData.accel.z = a.acceleration.z;

  // Copy gyroscope data
  captureData.gyro.x = g.gyro.x;
  captureData.gyro.y = g.gyro.y;
  captureData.gyro.z = g.gyro.z;

  // Copy temperature data
  captureData.temp = t.temperature;

  // Process the new data
  processingFunction(&captureData);
}

static inline void displayCaptureDataOLED(const struct triaxis_capture_t *capture) {
  // Print out the sensor values
  oled_display.print("Timestamp: ");
  oled_display.println(capture->dataTimestamp);
  oled_display.print(capture->accel.x);
  oled_display.print(", ");
  oled_display.print(capture->accel.y);
  oled_display.print(", ");
  oled_display.println(capture->accel.z);
  oled_display.print(capture->gyro.x);
  oled_display.print(", ");
  oled_display.print(capture->gyro.y);
  oled_display.print(", ");
  oled_display.println(capture->gyro.z);
}

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
    _oled.display();
  }

  // Never return
  while(true) { delay(10); }
}

void setup() {

  // Configure OLED Display
  if(!oled_display.begin()) {
    error();
  }

  // Configure TRI A
  if(!configureMPU(&mpu_a, MPU_A_ADDR) || !configureTRI(&tri_a, &mpu_a)) {
    error(&oled_display, "Failed to configure TRI A");
  }

  // Configure TRI B
  if(!configureMPU(&mpu_b, MPU_B_ADDR) || !configureTRI(&tri_b, &mpu_b)) {
    error(&oled_display, "Failed to configure TRI B");
  }

  // Show splash image
  oled_display.display();

  // Wait for button press
  attachInterrupt(PUSHBUTTON_PIN, PUSHBUTTON_ISR, FALLING);
  for(pushbutton_interrupt = false; !pushbutton_interrupt;) {
    delay(100); // Average human button press time
  }

  // Start listening for interrupts
  attachInterrupt(MPU_A_INT_PIN, MPU_A_ISR, RISING);
  attachInterrupt(MPU_B_INT_PIN, MPU_B_ISR, RISING);
}

void loop() {

  // Clear the screen
  OLED_CLEAR(oled_display);

  // Print battery voltage
  oled_display.print("Battery: ");
  oled_display.print(readBatteryVoltage());
  oled_display.println("V");

  // Check if TRI A has new data
  if(GET_TRI_DATA_RDY(tri_a)) {
    processData(&tri_a, &displayCaptureDataOLED);
    CLR_TRI_DATA_RDY(tri_a);
  }

  // Check if TRI B has new data
  if(GET_TRI_DATA_RDY(tri_b)) {
    processData(&tri_b, &displayCaptureDataOLED);
    CLR_TRI_DATA_RDY(tri_b);
  }

  // Update the display
  oled_display.display();
  delay(500);
}
