/*
 * ESP-IDF v5.x struct compatibility shim for Arduino framework (IDF v4.x).
 *
 * This header is force-included (-include) BEFORE any other headers.
 * It pre-defines the esp_lcd_rgb_panel_config_t with the extra fields
 * that ESP32_Display_Panel v1.x expects but Arduino's IDF v4.x lacks.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Forward-declare types that the real header would provide */
#ifdef __cplusplus
extern "C" {
#endif

/* Clock source type — will be properly defined by the real header through
   include_next, but we need to declare it here */
#if __has_include("soc/soc_caps.h")
#include "soc/soc_caps.h"
#endif

/* These types must match what the real IDF header provides */
#ifndef SOC_LCD_RGB_DATA_WIDTH
#define SOC_LCD_RGB_DATA_WIDTH 16
#endif

/* Define the missing enum if not present */
#ifndef LCD_RGB_ELEMENT_ORDER_RGB
typedef enum {
    LCD_RGB_ELEMENT_ORDER_RGB = 0,
    LCD_RGB_ELEMENT_ORDER_BGR = 1,
} lcd_rgb_element_order_t;
#endif

/* Pre-declare the struct with the v5.x fields so that when the real header
   is included later, it won't redefine the struct (include guard prevents it) */
#ifndef __ESP_LCD_PANEL_RGB_H__
#define __ESP_LCD_PANEL_RGB_H__

/* Include the core LCD types needed for the struct */
#include "esp_lcd_types.h"

typedef int lcd_clock_source_t;  /* Will be overridden by real header */

typedef struct {
    unsigned int pclk_hz;
    unsigned int h_res;
    unsigned int v_res;
    unsigned int hsync_pulse_width;
    unsigned int hsync_back_porch;
    unsigned int hsync_front_porch;
    unsigned int vsync_pulse_width;
    unsigned int vsync_back_porch;
    unsigned int vsync_front_porch;
    struct {
        unsigned int hsync_idle_low: 1;
        unsigned int vsync_idle_low: 1;
        unsigned int de_idle_high: 1;
        unsigned int pclk_active_neg: 1;
        unsigned int pclk_idle_high: 1;
    } flags;
} esp_lcd_rgb_timing_t;

typedef struct {} esp_lcd_rgb_panel_event_data_t;
typedef struct esp_lcd_panel_t *esp_lcd_panel_handle_t;
typedef bool (*esp_lcd_rgb_panel_frame_trans_done_cb_t)(
    esp_lcd_panel_handle_t panel,
    esp_lcd_rgb_panel_event_data_t *edata,
    void *user_ctx);

/* The extended struct with IDF v5.1+ / v5.2+ fields */
typedef struct {
    lcd_clock_source_t clk_src;
    esp_lcd_rgb_timing_t timings;
    size_t data_width;
    size_t sram_trans_align;
    size_t psram_trans_align;
    int hsync_gpio_num;
    int vsync_gpio_num;
    int de_gpio_num;
    int pclk_gpio_num;
    int data_gpio_nums[SOC_LCD_RGB_DATA_WIDTH];
    int disp_gpio_num;
    esp_lcd_rgb_panel_frame_trans_done_cb_t on_frame_trans_done;
    void *user_ctx;
    struct {
        unsigned int disp_active_low: 1;
        unsigned int relax_on_idle: 1;
        unsigned int fb_in_psram: 1;
        /* --- IDF v5.1+ additions --- */
        unsigned int refresh_on_demand: 1;
        unsigned int double_fb: 1;
        unsigned int no_fb: 1;
        unsigned int bb_invalidate_cache: 1;
    } flags;
    /* --- IDF v5.2+ additions --- */
    unsigned int num_fbs;
    unsigned int bounce_buffer_size_px;
} esp_lcd_rgb_panel_config_t;

/* Declare the create function */
typedef int esp_err_t;
#define ESP_OK 0
esp_err_t esp_lcd_new_rgb_panel(
    const esp_lcd_rgb_panel_config_t *rgb_panel_config,
    esp_lcd_panel_handle_t *ret_panel);

#endif /* __ESP_LCD_PANEL_RGB_H__ */

#ifdef __cplusplus
}
#endif
