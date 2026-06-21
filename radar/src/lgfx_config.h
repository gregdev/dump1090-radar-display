/*
 * LovyanGFX configuration for ESP32-4848S040C_I_Y_3
 * (Jingcai/Guition board: ST7701 RGB 480x480, GT911 touch)
 *
 * Uses LGFX's built-in Panel_ST7701_guition_esp32_4848S040
 * which has the correct LCD init sequence for this board.
 */
#pragma once

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>

class LGFX : public lgfx::LGFX_Device
{
    lgfx::Panel_ST7701_guition_esp32_4848S040 _panel_instance;
    lgfx::Bus_RGB    _bus_instance;
    lgfx::Touch_GT911 _touch_instance;
    lgfx::Light_PWM  _light_instance;

public:
    LGFX(void)
    {
        {
            auto cfg = _bus_instance.config();
            cfg.freq_write = 26000000;

            cfg.pin_pclk  = 21;
            cfg.pin_vsync = 17;
            cfg.pin_hsync = 16;
            cfg.pin_henable = -1;

            cfg.pin_d0  = 4;   cfg.pin_d1  = 5;
            cfg.pin_d2  = 6;   cfg.pin_d3  = 7;
            cfg.pin_d4  = 15;  cfg.pin_d5  = 8;
            cfg.pin_d6  = 20;  cfg.pin_d7  = 3;
            cfg.pin_d8  = 46;  cfg.pin_d9  = 9;
            cfg.pin_d10 = 10;  cfg.pin_d11 = 11;
            cfg.pin_d12 = 12;  cfg.pin_d13 = 13;
            cfg.pin_d14 = 14;  cfg.pin_d15 = 0;

            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }

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
            auto cfg = _touch_instance.config();
            cfg.pin_sda  = 19;
            cfg.pin_scl  = 45;
            cfg.i2c_addr = 0x5D;
            cfg.i2c_port = 1;
            cfg.freq = 400000;
            cfg.x_min = 0;
            cfg.x_max = 479;
            cfg.y_min = 0;
            cfg.y_max = 479;
            _touch_instance.config(cfg);
            _panel_instance.setTouch(&_touch_instance);
        }

        {
            auto cfg = _light_instance.config();
            cfg.pin_bl = 38;
            cfg.invert = false;
            cfg.freq   = 5000;
            cfg.pwm_channel = 0;
            _light_instance.config(cfg);
            _panel_instance.setLight(&_light_instance);
        }

        setPanel(&_panel_instance);
    }
};
