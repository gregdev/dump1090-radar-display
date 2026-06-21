/**
 * LVGL configuration for PC simulator.
 *
 * Minimal settings — enough for the radar display canvas.
 * Copy this to your lv_conf.h include path.
 */

#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_CONF_SUPPRESS_DEF_DEFINE_H   /* don't auto-include lv_conf_internal */

/*──────────────────────────────────────────────────────────
 *  Colour depth & performance
 *──────────────────────────────────────────────────────────*/
#define LV_COLOR_DEPTH          16
#define LV_COLOR_16_SWAP        0       /* 0 for little-endian, 1 for big */

/*──────────────────────────────────────────────────────────
 *  Memory
 *──────────────────────────────────────────────────────────*/
#define LV_MEM_SIZE             (1024 * 1024)   /* 1 MB heap for LVGL */

/*──────────────────────────────────────────────────────────
 *  Tick source
 *──────────────────────────────────────────────────────────*/
#define LV_TICK_CUSTOM          1
#define LV_TICK_CUSTOM_INCLUDE  <stdint.h>
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (lv_tick_elapsed_ms())

/*──────────────────────────────────────────────────────────
 *  Feature toggles — disable what we don't need
 *──────────────────────────────────────────────────────────*/
#define LV_USE_LOG              1
#define LV_USE_DEBUG            0
#define LV_USE_DRAW_SW          1
#define LV_USE_OBSOLETE         0

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
#define LV_FONT_MONTSERRAT_12   0
#define LV_FONT_MONTSERRAT_14   0
#define LV_FONT_MONTSERRAT_18   0
#define LV_FONT_MONTSERRAT_24   0
#define LV_FONT_UNSCII_8    1
#define LV_FONT_UNSCII_16   1
#define LV_FONT_MONOID_12   1
#define LV_FONT_DEFAULT         &lv_font_unscii_16

#endif /* LV_CONF_H */
