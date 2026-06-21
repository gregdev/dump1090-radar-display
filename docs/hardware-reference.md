# Guition ESP32-4848S040 — Hardware Reference

## Board Identification
- **Library ID**: `Jingcai:ESP32_4848S040C_I_Y_3`
- **ESP32_Display_Panel**: Officially supported (board/board_jingcai.md)
- **MCU**: ESP32-S3
- **PSRAM**: 8 MB Octal
- **Flash**: 16 MB
- **USB**: USB-C (UART or CDC, selectable)

## Display
- **Controller**: Sitronix ST7701
- **Resolution**: 480 × 480 px
- **Interface**: 3-wire SPI (control) + RGB (refresh), 16-bit data width
- **Color format**: RGB666 over RGB565 data bus
- **PCLK**: 26 MHz

## LCD RGB Pin Mapping (16-bit)

| LCD Pin | ESP32-S3 GPIO | Notes |
|---------|---------------|-------|
| HSYNC   | 16 | |
| VSYNC   | 17 | |
| DE      | 18 | |
| PCLK    | 21 | |
| DATA0   | 4  | B0 (RGB565) / B0-3 (RGB666/888) |
| DATA1   | 5  | B1 |
| DATA2   | 6  | B2 |
| DATA3   | 7  | B3 |
| DATA4   | 15 | B4 |
| DATA5   | 8  | G0 |
| DATA6   | 20 | G1 |
| DATA7   | 3  | G2 |
| DATA8   | 46 | G3 |
| DATA9   | 9  | G4 |
| DATA10  | 10 | G5 |
| DATA11  | 11 | R0-1 / R0-3 |
| DATA12  | 12 | R2 |
| DATA13  | 13 | R3 |
| DATA14  | 14 | R4 |
| DATA15  | 0  | R5 |
| DISP    | -1 | Not used |

## 3-wire SPI Control Pins

| Signal | ESP32-S3 GPIO |
|--------|---------------|
| CS     | 39 |
| SCK    | 48 |
| SDA    | 47 |

## Touch Panel
- **Controller**: GOODiX GT911
- **Bus**: I2C (Host 0, 400 kHz)
- **I2C SCL**: GPIO 45
- **I2C SDA**: GPIO 19
- **Address**: 0x5D (default) or 0x14
- **RST/INT**: Not connected (-1)

## Backlight
- **Type**: PWM (LEDC)
- **GPIO**: 38
- **Active level**: High (1 = on)
- **PWM Frequency**: 1000 Hz
- **Duty Resolution**: 10-bit
- **Idle-off**: No (backlight stays on after init)

## IO Expander
- Not present on this board.

## Free GPIOs (not used by display/touch/backlight)

Available for user peripherals (rotary encoder, buttons, etc.):

| GPIO | Notes |
|------|-------|
| 1    | ADC1_CH0 — safe to use |
| 2    | ADC1_CH1 — safe to use |
| 26   | General purpose — likely exposed on pads |
| 27   | General purpose — likely exposed on pads |
| 28–37 | General purpose — check physical availability |
| 40   | General purpose |
| 41   | General purpose |
| 42   | General purpose |
| 43   | U0TXD (serial TX — may be used for USB serial) |
| 44   | U0RXD (serial RX — may be used for USB serial) |

> **Note**: Physical pin availability depends on the specific board revision.
> Inspect the board for header pads/holes. GPIOs 26–27 and 40–42 are
> the most commonly broken-out GPIOs on this style of board.

## ESP32_Display_Panel Backlight API

```cpp
#include <ESP_Panel_Library.h>

ESP_Panel *panel = new ESP_Panel();
panel->init();

// Get backlight driver
auto backlight = panel->getBacklight();  // returns drivers::Backlight*

// Brightness control (0–100%)
backlight->setBrightness(75);            // 75%
backlight->setBrightness(0);             // off
backlight->setBrightness(100);           // full

// Quick on/off
backlight->on();                         // = setBrightness(100)
backlight->off();                        // = setBrightness(0)

// Query current brightness
int pct = backlight->getBrightness();    // 0–100
```

## PlatformIO Build Config

```ini
platform    = espressif32@~6.9.0
board       = esp32-s3-devkitc-1
framework   = arduino
board_build.flash_mode = qio
board_build.psram_type = opi
board_build.arduino.memory_type = qio_opi
build_flags = -DESP_PANEL_USE_RGB=1
```

## Arduino IDE Board Settings

| Setting | Value |
|---------|-------|
| Board | ESP32S3 Dev Module |
| PSRAM | OPI |
| Flash Mode | QIO 80MHz |
| Flash Size | 16MB |
| USB CDC On Boot | Disabled (UART) / Enabled (USB) |
| Partition Scheme | 16M Flash (3MB) |

## Useful Links

- Board config header: `src/board/supported/jingcai/BOARD_JINGCAI_ESP32_4848S040C_I_Y_3.h`
- Board docs: `docs/board/board_jingcai.md`
- ESP32_Display_Panel repo: https://github.com/esp-arduino-libs/ESP32_Display_Panel
- LCD datasheet reference: ST7701 (Sitronix)
- Touch datasheet reference: GT911 (GOODiX)
- Manufacturer: Shenzhen Jingcai Intelligent (displaysmodule.com)
