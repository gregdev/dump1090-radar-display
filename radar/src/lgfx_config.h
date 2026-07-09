/*
 * LovyanGFX configuration — supports two platforms:
 *   ESP32-S3:  ST7701 RGB 480x480 (ESP32-4848S040)
 *   ESP32-C6:  ILI9341 SPI 240x320 (square 240x240 crop)
 *
 * Select via build flag: -DPLATFORM_C6_ILI9341
 */
#pragma once

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

#ifdef PLATFORM_C6_ILI9341
/* ═══════════════════════════════════════════════════════════
 *  ESP32-C6 + ILI9341 2.4" SPI TFT  (240×320 panel, 240×240 crop)
 * ═══════════════════════════════════════════════════════════ */
#include "config.h"

class LGFX : public lgfx::LGFX_Device
{
    lgfx::Panel_ILI9341  _panel_instance;
    lgfx::Bus_SPI        _bus_instance;
    lgfx::Light_PWM      _light_instance;

public:
    LGFX(void)
    {
        {
            auto cfg = _bus_instance.config();
            cfg.spi_host   = SPI2_HOST;
            cfg.spi_mode   = 0;
            cfg.freq_write = 40000000;   // 40 MHz SPI
            cfg.freq_read  = 16000000;
            cfg.spi_3wire  = false;
            cfg.use_lock   = true;
            cfg.dma_channel = SPI_DMA_CH_AUTO;
            cfg.pin_sclk   = ILI9341_PIN_SCLK;
            cfg.pin_mosi   = ILI9341_PIN_MOSI;
            cfg.pin_miso   = ILI9341_PIN_MISO;
            cfg.pin_dc     = ILI9341_PIN_DC;
            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }

        {
            auto cfg = _panel_instance.config();
            cfg.pin_cs     = ILI9341_PIN_CS;
            cfg.pin_rst    = ILI9341_PIN_RST;
            cfg.panel_width  = 240;
            cfg.panel_height = 320;
            cfg.offset_x     = 0;
            cfg.offset_y     = 0;       // logical top of panel
            cfg.offset_rotation = 0;
            cfg.readable     = false;
            cfg.invert       = false;
            cfg.rgb_order    = false;   // BGR
            cfg.dlen_16bit   = false;
            cfg.bus_shared   = false;
            _panel_instance.config(cfg);
        }

        {
            auto cfg = _light_instance.config();
            cfg.pin_bl = ILI9341_PIN_BL;
            cfg.invert = false;
            cfg.freq   = 12000;         // 12 kHz PWM
            cfg.pwm_channel = 0;
            _light_instance.config(cfg);
        }
        _panel_instance.setLight(&_light_instance);

        setPanel(&_panel_instance);
    }
};

#else
/* ═══════════════════════════════════════════════════════════
 *  ESP32-S3 + ST7701 RGB 480×480  (ESP32-4848S040)
 * ═══════════════════════════════════════════════════════════ */

#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>

class LGFX : public lgfx::LGFX_Device
{
    lgfx::Panel_ST7701_guition_esp32_4848S040 _panel_instance;
    lgfx::Bus_RGB    _bus_instance;
    lgfx::Light_PWM   _light_instance;

public:
    LGFX(void)
    {
        {
            auto cfg = _panel_instance.config();
            cfg.memory_width  = 480;
            cfg.memory_height = 480;
            cfg.panel_width   = 480;
            cfg.panel_height  = 480;
            cfg.offset_x = 0;
            cfg.offset_y = 0;
            _panel_instance.config(cfg);
        }

        {
            // 3-wire SPI for ST7701 init commands
            auto cfg = _panel_instance.config_detail();
            cfg.pin_cs   = 39;
            cfg.pin_sclk = 48;
            cfg.pin_mosi = 47;  // SDA
            _panel_instance.config_detail(cfg);
        }

        {
            auto cfg = _bus_instance.config();
            cfg.panel = &_panel_instance;        // ← REQUIRED! bus needs panel ref for dimensions

            cfg.pin_d0   = GPIO_NUM_4;   // B0
            cfg.pin_d1   = GPIO_NUM_5;   // B1
            cfg.pin_d2   = GPIO_NUM_6;   // B2
            cfg.pin_d3   = GPIO_NUM_7;   // B3
            cfg.pin_d4   = GPIO_NUM_15;  // B4
            cfg.pin_d5   = GPIO_NUM_8;   // G0
            cfg.pin_d6   = GPIO_NUM_20;  // G1
            cfg.pin_d7   = GPIO_NUM_3;   // G2
            cfg.pin_d8   = GPIO_NUM_46;  // G3
            cfg.pin_d9   = GPIO_NUM_9;   // G4
            cfg.pin_d10  = GPIO_NUM_10;  // G5
            cfg.pin_d11  = GPIO_NUM_11;  // R0
            cfg.pin_d12  = GPIO_NUM_12;  // R1
            cfg.pin_d13  = GPIO_NUM_13;  // R2
            cfg.pin_d14  = GPIO_NUM_14;  // R3
            cfg.pin_d15  = GPIO_NUM_0;   // R4

            cfg.pin_henable = GPIO_NUM_18;
            cfg.pin_vsync   = GPIO_NUM_17;
            cfg.pin_hsync   = GPIO_NUM_16;
            cfg.pin_pclk    = GPIO_NUM_21;
            cfg.freq_write  = 14000000;

            cfg.hsync_polarity    = 0;
            cfg.hsync_front_porch = 10;
            cfg.hsync_pulse_width = 8;
            cfg.hsync_back_porch  = 50;
            cfg.vsync_polarity    = 0;
            cfg.vsync_front_porch = 10;
            cfg.vsync_pulse_width = 8;
            cfg.vsync_back_porch  = 20;
            cfg.pclk_idle_high    = 0;
            cfg.de_idle_high      = 1;

            _bus_instance.config(cfg);
        }
        _panel_instance.setBus(&_bus_instance);

        {
            auto cfg = _light_instance.config();
            cfg.pin_bl = GPIO_NUM_38;
            _light_instance.config(cfg);
        }
        _panel_instance.setLight(&_light_instance);

        setPanel(&_panel_instance);
    }
};
#endif
