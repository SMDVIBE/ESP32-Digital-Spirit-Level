#define MODE_BUTTON 5
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Adafruit_ST7789.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <math.h>
#include <Preferences.h>

// ===================== ESP32-C3 =====================
#define MPU_SDA 0
#define MPU_SCL 1

#define TFT_SCLK 4
#define TFT_MOSI 6
#define TFT_CS   7
#define TFT_DC   3
#define TFT_RST  10

// ===================== Display =====================
SPIClass spi(FSPI);
Adafruit_ST7789 tft(&spi, TFT_CS, TFT_DC, TFT_RST);
Adafruit_MPU6050 mpu;
Preferences preferences;

// Off-screen framebuffer: everything is drawn here first,
// then transferred to the display in one operation to eliminate flicker.
GFXcanvas16 canvas(320, 170);

// ===================== Colors ======================
#define COLOR_BG     0x0000
#define COLOR_PANEL  0x18E3
#define COLOR_WHITE  0xFFFF
#define COLOR_GREY   0xC618
#define COLOR_GREEN  0x07E0
#define COLOR_YELLOW 0xFFE0
#define COLOR_RED    0xF800
#define COLOR_CYAN   0x07FF

// ===================== Bubble ======================
// Single-axis horizontal spirit level
const int LEVEL_LEFT  = 42;
const int LEVEL_RIGHT = 178;
const int LEVEL_CY    = 92;
const int TUBE_RADIUS = 26;
const int BUBBLE_RADIUS = 16;

const int CX = 110;
const int CY = 92;
const int DUAL_RADIUS = 56;
const int DUAL_INNER = 45;
const int DUAL_BUBBLE = 14;

float angleX = 0;
float angleY = 0;
float displayX = 0;
float displayY = 0;

float bubbleX = 110;
float bubbleY = 92;
bool dualMode = false;
bool lastButtonState = HIGH;
bool buttonWasPressed = false;
unsigned long buttonPressStart = 0;
unsigned long lastButtonTime = 0;
bool longPressHandled = false;

// Calibration feedback shown on the TFT
uint8_t calibrationMessage = 0; // 0=none, 1=CALIBRATING, 2=ZERO SET
unsigned long calibrationMessageUntil = 0;

// Separate calibration offsets for each mode (stored in ESP32 NVS).
// Single-axis calibration never affects dual-axis mode, and vice versa.
float zeroSingleX = 0.0f;
float zeroDualX = 0.0f;
float zeroDualY = 0.0f;

// Small amount of smoothing: removes tiny jitter without making the level slow.
const float ANGLE_FILTER = 0.18f;
const float BUBBLE_FILTER = 0.22f;
const float ZERO_DEAD_ZONE = 0.12f;  // degrees

// ===================== Text values =================
float lastShownX = 999;
float lastShownY = 999;
int lastStatus = -1;

// Draw the complete interface into an off-screen buffer.
// The TFT receives the finished frame in one transfer, so the bubble
// never appears/disappears during the redraw.
void renderFrame(float bx, float by, uint16_t bubbleColor,
                 float shownX, float shownY, float maxAngle) {
  canvas.fillScreen(COLOR_BG);
  canvas.fillRect(0, 0, 220, 170, COLOR_PANEL);

  canvas.setTextSize(1);
  canvas.setTextColor(COLOR_GREY);
  canvas.setCursor(8, 8);
  canvas.print(dualMode ? "DUAL AXIS LEVEL" : "DIGITAL LEVEL");

  if (!dualMode) {
    canvas.drawRoundRect(LEVEL_LEFT - TUBE_RADIUS,
                         LEVEL_CY - TUBE_RADIUS,
                         (LEVEL_RIGHT - LEVEL_LEFT) + TUBE_RADIUS * 2,
                         TUBE_RADIUS * 2, TUBE_RADIUS, COLOR_WHITE);

    canvas.drawRoundRect(LEVEL_LEFT - TUBE_RADIUS + 7,
                         LEVEL_CY - TUBE_RADIUS + 7,
                         (LEVEL_RIGHT - LEVEL_LEFT) + TUBE_RADIUS * 2 - 14,
                         TUBE_RADIUS * 2 - 14, TUBE_RADIUS - 7, COLOR_GREY);

    canvas.drawFastVLine(LEVEL_LEFT, LEVEL_CY - 18, 36, COLOR_GREY);
    canvas.drawFastVLine(LEVEL_RIGHT, LEVEL_CY - 18, 36, COLOR_GREY);
    canvas.drawFastVLine(110, LEVEL_CY - 22, 44, COLOR_WHITE);

    canvas.fillCircle((int)bx, LEVEL_CY, BUBBLE_RADIUS, bubbleColor);
    canvas.drawCircle((int)bx, LEVEL_CY, BUBBLE_RADIUS, COLOR_WHITE);
  } else {
    canvas.drawCircle(CX, CY, DUAL_RADIUS, COLOR_WHITE);
    canvas.drawCircle(CX, CY, DUAL_INNER, COLOR_GREY);
    canvas.drawFastHLine(CX - 40, CY, 80, COLOR_GREY);
    canvas.drawFastVLine(CX, CY - 40, 80, COLOR_GREY);
    canvas.fillCircle(CX, CY, 3, COLOR_WHITE);

    canvas.fillCircle((int)bx, (int)by, DUAL_BUBBLE, bubbleColor);
    canvas.drawCircle((int)bx, (int)by, DUAL_BUBBLE, COLOR_WHITE);
  }

  canvas.fillRect(220, 0, 100, 170, COLOR_BG);
  canvas.drawFastVLine(220, 0, 170, COLOR_GREY);

  canvas.setTextSize(1);
  canvas.setTextColor(COLOR_GREY);

  if (dualMode) {
    canvas.setCursor(232, 16);
    canvas.print("X ANGLE");
    canvas.setTextSize(2);
    canvas.setTextColor(COLOR_WHITE);
    canvas.setCursor(230, 34);
    if (shownX >= 0) canvas.print("+");
    canvas.print(shownX, 1);
    canvas.print((char)247);

    canvas.setTextSize(1);
    canvas.setTextColor(COLOR_GREY);
    canvas.setCursor(232, 72);
    canvas.print("Y ANGLE");
    canvas.setTextSize(2);
    canvas.setTextColor(COLOR_WHITE);
    canvas.setCursor(230, 90);
    if (shownY >= 0) canvas.print("+");
    canvas.print(shownY, 1);
    canvas.print((char)247);

    canvas.setTextSize(1);
    canvas.setTextColor(COLOR_GREY);
    canvas.setCursor(232, 128);
    canvas.print("STATUS");

    canvas.setTextSize(2);
    canvas.setCursor(232, 146);
  } else {
    canvas.setCursor(232, 28);
    canvas.print("X ANGLE");
    canvas.setTextSize(2);
    canvas.setTextColor(COLOR_WHITE);
    canvas.setCursor(230, 48);
    if (shownX >= 0) canvas.print("+");
    canvas.print(shownX, 1);
    canvas.print((char)247);

    canvas.setTextSize(1);
    canvas.setTextColor(COLOR_GREY);
    canvas.setCursor(232, 112);
    canvas.print("STATUS");

    canvas.setTextSize(2);
    canvas.setCursor(232, 130);
  }

  if (maxAngle < 0.8f) {
    canvas.setTextColor(COLOR_GREEN);
    canvas.print("LEVEL");
  } else if (maxAngle < 4.0f) {
    canvas.setTextColor(COLOR_YELLOW);
    canvas.print("TILT");
  } else {
    canvas.setTextColor(COLOR_RED);
    canvas.print("TILT");
  }

  // Calibration feedback overlay (drawn over the framebuffer)
  if (calibrationMessage != 0 && millis() < calibrationMessageUntil) {
    const char *message = calibrationMessage == 1 ? "CALIBRATING..." : "ZERO SET!";
    canvas.fillRoundRect(22, 58, 276, 54, 8, COLOR_BG);
    canvas.drawRoundRect(22, 58, 276, 54, 8, COLOR_WHITE);
    canvas.setTextSize(2);
    canvas.setTextColor(calibrationMessage == 1 ? COLOR_YELLOW : COLOR_GREEN);
    int16_t x1, y1;
    uint16_t w, h;
    canvas.getTextBounds(message, 0, 0, &x1, &y1, &w, &h);
    canvas.setCursor((320 - (int)w) / 2, 78);
    canvas.print(message);
  } else if (calibrationMessage != 0) {
    calibrationMessage = 0;
  }

  tft.drawRGBBitmap(0, 0, canvas.getBuffer(), 320, 170);
}

void setup() {
  Serial.begin(115200);
  delay(300);

  Wire.begin(MPU_SDA, MPU_SCL);

  if (!mpu.begin(0x68, &Wire)) {
    while (1) {
      delay(100);
    }
  }

  mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
  mpu.setGyroRange(MPU6050_RANGE_250_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  spi.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);

  tft.init(170, 320);
  tft.setRotation(3);
  tft.invertDisplay(true);
  tft.fillScreen(COLOR_BG);
  pinMode(MODE_BUTTON, INPUT_PULLUP);

  preferences.begin("level-cal", false);
  zeroSingleX = preferences.getFloat("singleX", 0.0f);
  zeroDualX   = preferences.getFloat("dualX", 0.0f);
  zeroDualY   = preferences.getFloat("dualY", 0.0f);
}

void loop() {
  bool buttonState = digitalRead(MODE_BUTTON);

  // Detect press/release. Short press changes mode; 3-second hold calibrates.
  if (lastButtonState == HIGH && buttonState == LOW) {
    buttonWasPressed = true;
    buttonPressStart = millis();
    longPressHandled = false;
  }

  // Show a brief CALIBRATING... message during the final half-second
  // of the long-press, before the zero offsets are applied.
  if (buttonWasPressed && buttonState == LOW && !longPressHandled &&
      millis() - buttonPressStart >= 2500) {
    calibrationMessage = 1;
    calibrationMessageUntil = millis() + 600;
  }

  if (buttonWasPressed && buttonState == LOW &&
      !longPressHandled && millis() - buttonPressStart >= 3000) {
    longPressHandled = true;

    // Independent calibration:
    // Single-axis mode stores only its own X zero.
    // Dual-axis mode stores its own X and Y zeros.
    if (dualMode) {
      zeroDualX = angleX;
      zeroDualY = angleY;
      preferences.putFloat("dualX", zeroDualX);
      preferences.putFloat("dualY", zeroDualY);
      Serial.println("DUAL AXIS ZERO CALIBRATED (X/Y)");
    } else {
      zeroSingleX = angleX;
      preferences.putFloat("singleX", zeroSingleX);
      Serial.println("SINGLE AXIS ZERO CALIBRATED (X)");
    }
    calibrationMessage = 2;
    calibrationMessageUntil = millis() + 1300;
  }

  if (buttonWasPressed && lastButtonState == LOW && buttonState == HIGH) {
    unsigned long pressDuration = millis() - buttonPressStart;

    if (pressDuration < 3000 && millis() - lastButtonTime > 250) {
      dualMode = !dualMode;
      lastButtonTime = millis();
    }

    buttonWasPressed = false;
  }

  lastButtonState = buttonState;

  sensors_event_t accel, gyro, temp;
  mpu.getEvent(&accel, &gyro, &temp);

  float rawX = atan2(
    accel.acceleration.y,
    sqrt(accel.acceleration.x * accel.acceleration.x +
         accel.acceleration.z * accel.acceleration.z)
  ) * 180.0f / PI;

  float rawY = atan2(
    -accel.acceleration.x,
    sqrt(accel.acceleration.y * accel.acceleration.y +
         accel.acceleration.z * accel.acceleration.z)
  ) * 180.0f / PI;

  // Filter raw sensor angles first, then apply the stored zero offsets.
  angleX += ANGLE_FILTER * (rawX - angleX);
  angleY += ANGLE_FILTER * (rawY - angleY);

  // Apply the active mode's own calibration values.
  float calibratedSingleX = angleX - zeroSingleX;
  float calibratedDualX = angleX - zeroDualX;
  float calibratedDualY = angleY - zeroDualY;

  if (fabs(calibratedSingleX) < ZERO_DEAD_ZONE) calibratedSingleX = 0;
  if (fabs(calibratedDualX) < ZERO_DEAD_ZONE) calibratedDualX = 0;
  if (fabs(calibratedDualY) < ZERO_DEAD_ZONE) calibratedDualY = 0;

  displayX = dualMode ? calibratedDualX : calibratedSingleX;

  float targetSingleX =
    110.0f + constrain(calibratedSingleX, -15.0f, 15.0f) *
    (LEVEL_RIGHT - LEVEL_LEFT - BUBBLE_RADIUS * 2) / 30.0f;

  // X stays natural: right -> right, left -> left.
  // Y is inverted: away/toward direction is flipped.
  float rotatedX = calibratedDualX;
  float rotatedY = -calibratedDualY;

  float targetDualX =
    CX + constrain(rotatedX, -15.0f, 15.0f) *
    (DUAL_RADIUS - DUAL_BUBBLE - 3) / 15.0f;

  float targetDualY =
    CY + constrain(rotatedY, -15.0f, 15.0f) *
    (DUAL_RADIUS - DUAL_BUBBLE - 3) / 15.0f;

  if (dualMode) {
    bubbleX += BUBBLE_FILTER * (targetDualX - bubbleX);
    bubbleY += BUBBLE_FILTER * (targetDualY - bubbleY);
  } else {
    bubbleX += BUBBLE_FILTER * (targetSingleX - bubbleX);
    bubbleY = LEVEL_CY;
  }

  float maxAngle = dualMode ? max(fabs(calibratedDualX), fabs(calibratedDualY))
                            : fabs(calibratedSingleX);

  uint16_t bubbleColor;
  if (maxAngle < 0.8f) bubbleColor = COLOR_GREEN;
  else if (maxAngle < 4.0f) bubbleColor = COLOR_YELLOW;
  else bubbleColor = COLOR_RED;

  // Display the same calibrated coordinate system.
  float shownDualX = calibratedDualX;
  float shownDualY = -calibratedDualY;

  renderFrame(bubbleX, dualMode ? bubbleY : LEVEL_CY,
              bubbleColor,
              dualMode ? shownDualX : calibratedSingleX,
              dualMode ? shownDualY : calibratedSingleX,
              maxAngle);

  delay(25);
}
