/**
 * LVGL v9 configuration for ESP32-S3 (ESP32-4848S040, ST7701, 8MB PSRAM).
 *
 * PlatformIO places this via build_flags -DLV_CONF_INCLUDE_SIMPLE -I.
 */

#ifndef LV_CONF_H
#define LV_CONF_H

/*──────────────────────────────────────────────────────────
 *  Colour depth — RGB565 for the 480×480 panel
 *──────────────────────────────────────────────────────────*/
#define LV_COLOR_DEPTH          16
#define LV_COLOR_16_SWAP        0       /* 0 = little-endian (ESP32 is LE) */

/*──────────────────────────────────────────────────────────
 *  Memory — PSRAM on S3 (via system malloc), SRAM on C6
 *──────────────────────────────────────────────────────────*/
#ifdef PLATFORM_C6_ILI9341
  #define LV_USE_STDLIB_MALLOC        LV_STDLIB_CLIB
  #define LV_MEM_SIZE                 (128 * 1024U)       /* 128 KB from SRAM */
#else
  #define LV_USE_STDLIB_MALLOC        LV_STDLIB_CLIB       /* system malloc (PSRAM on S3) */
  #define LV_MEM_SIZE                 0                    /* not used by CLIB */
#endif

/*──────────────────────────────────────────────────────────
 *  Tick source — Arduino millis()
 *──────────────────────────────────────────────────────────*/
#define LV_TICK_CUSTOM          1
#define LV_TICK_CUSTOM_INCLUDE  <stdint.h>
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (lv_tick_elapsed_ms())

/*──────────────────────────────────────────────────────────
 *  Feature toggles — keep it lean
 *──────────────────────────────────────────────────────────*/
#define LV_USE_LOG              1
#define LV_USE_DEBUG            0
#define LV_USE_DRAW_SW          1
#define LV_USE_DRAW_SDL         0
#define LV_USE_DRAW_RENESAS     0
#define LV_USE_DRAW_NANOVG      0
#define LV_USE_DRAW_VG_LITE     0
#define LV_USE_OBSOLETE         0

#define LV_USE_SNAPSHOT         0
#define LV_USE_SYSMON           0
#define LV_USE_PERF_MONITOR     0

#define LV_USE_DEMO_WIDGETS     0
#define LV_USE_DEMO_KEYPAD_AND_ENCODER 0
#define LV_USE_DEMO_BENCHMARK   0
#define LV_USE_DEMO_STRESS      0
#define LV_USE_DEMO_MUSIC       0
#define LV_USE_DEMO_FLEX_LAYOUT 0
#define LV_USE_DEMO_MULTILANG   0
#define LV_USE_DEMO_TRANSFORM   0
#define LV_USE_DEMO_SCROLL      0
#define LV_USE_DEMO_VECTOR_GRAPHIC 0

/*──────────────────────────────────────────────────────────
 *  Fonts
 *──────────────────────────────────────────────────────────*/
#define LV_FONT_MONTSERRAT_12   1
#define LV_FONT_MONTSERRAT_14   1
#define LV_FONT_MONTSERRAT_18   0
#define LV_FONT_MONTSERRAT_24   0
#define LV_FONT_UNSCII_8    1
#define LV_FONT_UNSCII_16   1
#define LV_FONT_MONOID_12   1
#define LV_FONT_DEFAULT         &lv_font_montserrat_14

#endif /* LV_CONF_H */
