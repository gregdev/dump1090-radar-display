/* Patched esp_lcd_panel_rgb.h — intercepts the framework header and adds
 * missing fields required by ESP32_Display_Panel v1.x (IDF v5.x additions).
 * Works via PlatformIO -I path ordering (project includes before -isystem). */
#pragma once
#include_next "esp_lcd_panel_rgb.h"
