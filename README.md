# DIY Digital Spirit Level with ESP32-C3

A DIY digital spirit level built around an **ESP32-C3 SuperMini**, an **MPU-6050** motion sensor and a **1.9-inch ST7789 IPS display**.

The device measures inclination and visualizes it as a moving bubble. It supports a traditional single-axis level and a dual-axis level, with independent zero calibration for each mode.

<p align="center">
  <img src="images/digital-spirit-level.jpg" alt="DIY digital spirit level assembled on a white bar" width="800">
</p>

## Features

- **Single-axis mode** — horizontal spirit-level style bubble with X-axis angle readout.
- **Dual-axis mode** — circular bubble display with X and Y angle readouts.
- **Independent zero calibration**:
  - Hold the button for 3 seconds in Single-axis mode to calibrate its X zero.
  - Hold the button for 3 seconds in Dual-axis mode to calibrate both X and Y zeros.
  - Calibration values for the two modes are stored separately in ESP32-C3 non-volatile storage.
- **On-screen feedback** — `CALIBRATING...` and `ZERO SET!` messages.
- **Reduced display flicker** — the interface is drawn to a framebuffer before being sent to the TFT.
- **Filtered measurements** — light smoothing helps reduce sensor jitter.

## Hardware

| Component | Description |
|---|---|
| Single-side prototype PCB | 50 × 70 mm |
| ESP32-C3 SuperMini | Main controller |
| 1.9-inch IPS LCD | ST7789, 170 × 320 |
| MPU-6050 | Accelerometer / gyroscope module |
| CKCS BS01 DC-DC converter | 3.7 V to 5 V boost converter |
| LX-LBES USB-C charging board | Li-ion charging module |
| Li-ion battery | 500 mAh |
| Pin header connectors | Board interconnections |
| Micro tactile push button | Mode selection / zero calibration |
| Miniature slide switch | Power on/off |

## Wiring

The wiring illustration below shows the overall power and signal connections used in the build.

<p align="center">
  <img src="images/wiring-diagram.jpg" alt="Wiring diagram for ESP32-C3 digital spirit level" width="900">
</p>

### Signal pin assignment

The table below reflects the GPIO assignments in the included Arduino sketch.

| Module | Signal | ESP32-C3 GPIO |
|---|---|---:|
| MPU-6050 | SDA | GPIO 0 |
| MPU-6050 | SCL | GPIO 1 |
| ST7789 display | SCLK / SCL | GPIO 4 |
| ST7789 display | MOSI / SDA | GPIO 6 |
| ST7789 display | CS | GPIO 7 |
| ST7789 display | DC | GPIO 3 |
| ST7789 display | RES / RST | GPIO 10 |
| Push button | One terminal | GPIO 5 |
| Push button | Other terminal | GND |

The button uses the ESP32-C3 internal pull-up (`INPUT_PULLUP`), so no external pull-up resistor is required.

**Power note:** follow the module labels and the wiring diagram when assembling the battery, charger and boost-converter section. Check the output voltage and polarity with a multimeter before connecting the controller, sensor or display. Do not connect a Li-ion cell directly to a 5 V input.

## How to use

1. Power on the device.
2. A short press of the button switches between Single-axis and Dual-axis modes.
3. Place the level on a reference surface.
4. Hold the button for approximately 3 seconds to set the zero for the currently active mode:
   - **Single-axis:** stores the X-axis zero for Single-axis mode only.
   - **Dual-axis:** stores X and Y zeros for Dual-axis mode only.
5. The calibration is saved in ESP32-C3 non-volatile storage and remains available after restart.

For the most accurate result, keep the device still on a known level reference surface while calibrating.

## Firmware

The Arduino sketch is located here:

[`firmware/ESP32_C3_Digital_Level/ESP32_C3_Digital_Level.ino`](firmware/ESP32_C3_Digital_Level/ESP32_C3_Digital_Level.ino)

### Arduino libraries

Install these libraries using Arduino IDE Library Manager:

- **Adafruit GFX Library**
- **Adafruit ST7735 and ST7789 Library**
- **Adafruit MPU6050**
- **Adafruit Unified Sensor**

The sketch also uses `Wire`, `SPI` and `Preferences`, which are provided by the ESP32 Arduino core.

### Upload

1. Install ESP32 board support in Arduino IDE.
2. Select the ESP32-C3 board that matches your module (for example, **ESP32C3 Dev Module**).
3. Select the correct serial port.
4. Install the libraries listed above.
5. Open `ESP32_C3_Digital_Level.ino` and upload it.

## Project structure

```text
.
├── README.md
├── images/
│   ├── digital-spirit-level.jpg
│   └── wiring-diagram.jpg
└── firmware/
    └── ESP32_C3_Digital_Level/
        └── ESP32_C3_Digital_Level.ino
```

## Notes

- The display is initialized as a 170 × 320 ST7789 panel and rotated for the landscape user interface.
- The MPU-6050 is accessed at I²C address `0x68`.
- The sketch currently uses the sensor's accelerometer-derived tilt angles.
- Zero calibration adjusts the displayed reference angle; it is not a substitute for mechanically aligning the sensor or verifying the reference surface.

## License

No license has been selected for this project yet. Add a `LICENSE` file before publishing if you want to specify how others may use, modify or redistribute the code and design.
