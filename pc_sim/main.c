/**
 * radar_sim — PC simulator using SDL2 + LVGL.
 *
 * Displays the radar UI in a window, fetching live data from dump1090.
 *
 * Requires SDL2. On Windows:
 *   winget install SDL2
 * Or download SDL2-devel-2.x.x-VC.zip from libsdl.org,
 * extract to C:\SDL2, then set SDL2_DIR=C:\SDL2\cmake
 */

#define SDL_MAIN_HANDLED
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <SDL.h>

#include "lvgl.h"
#include "config.h"
#include "radar_ui.h"
#include "aircraft_data.h"
#include "coord_convert.h"

/* ── globals ────────────────────────────────────────────── */
static SDL_Window   *window  = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture  *texture  = NULL;
static lv_display_t *disp     = NULL;

static uint32_t      fb_buf[480 * 480];   /* ARGB8888 framebuffer */
static bool          running  = true;

static aircraft_t    aircraft[MAX_AIRCRAFT];
static int           aircraft_count = 0;

static uint32_t      last_fetch = 0;
static uint32_t      last_sweep = 0;
static bool          fetch_failed = false;   /* backoff on unreachable host */
#define FETCH_BACKOFF_MS  10000  /* wait 10 s after failure before retry */

/* ── LVGL tick (ms since start) ─────────────────────────── */
static uint32_t tick_start = 0;

uint32_t lv_tick_elapsed_ms(void) {
    return SDL_GetTicks() - tick_start;
}

/* ── flush callback: LVGL → SDL texture ─────────────────── */
static void flush_cb(lv_display_t *d, const lv_area_t *area,
                     uint8_t *px_map) {
    int w = lv_area_get_width(area);
    int h = lv_area_get_height(area);

    /* convert RGB565 → ARGB8888 into our framebuffer */
    uint16_t *src = (uint16_t *)px_map;
    for (int y = area->y1; y <= area->y2; y++) {
        for (int x = area->x1; x <= area->x2; x++) {
            uint16_t c16 = *src++;
            uint8_t r = (c16 >> 11) & 0x1F;
            uint8_t g = (c16 >> 5)  & 0x3F;
            uint8_t b =  c16        & 0x1F;
            uint32_t c32 = 0xFF000000
                | ((uint32_t)(r << 3 | r >> 2) << 16)
                | ((uint32_t)(g << 2 | g >> 4) << 8)
                |  (uint32_t)(b << 3 | b >> 2);
            fb_buf[y * 480 + x] = c32;
        }
    }

    lv_display_flush_ready(d);
}

/* ── SDL mouse → LVGL input (event-driven, no missed clicks) ── */
static lv_indev_t *mouse_indev = NULL;
static int         mouse_x  = 0, mouse_y  = 0;
static lv_indev_state_t mouse_state = LV_INDEV_STATE_RELEASED;
static bool        mouse_dirty = false;  /* new event since last read */

static void mouse_read_cb(lv_indev_t *indev, lv_indev_data_t *data) {
    (void)indev;
    data->point.x = mouse_x;
    data->point.y = mouse_y;
    data->state   = mouse_state;
    mouse_dirty   = false;   /* consumed */
}

/* ── init ───────────────────────────────────────────────── */
static bool sdl_init(void) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return false;
    }

    window = SDL_CreateWindow("Radar Display — PC Simulator",
                              SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED,
                              480, 480,
                              SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        return false;
    }

    renderer = SDL_CreateRenderer(window, -1,
                                  SDL_RENDERER_ACCELERATED |
                                  SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());
        return false;
    }

    texture = SDL_CreateTexture(renderer,
                                SDL_PIXELFORMAT_ARGB8888,
                                SDL_TEXTUREACCESS_STREAMING,
                                480, 480);
    if (!texture) {
        fprintf(stderr, "SDL_CreateTexture: %s\n", SDL_GetError());
        return false;
    }

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    return true;
}

static void lvgl_init(void) {
    lv_init();

    tick_start = SDL_GetTicks();

    /* display */
    disp = lv_display_create(480, 480);
    lv_display_set_flush_cb(disp, flush_cb);

    static lv_color_t buf1[480 * 480 / 10];
    lv_display_set_buffers(disp, buf1, NULL,
                           sizeof(buf1),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);

    /* mouse input */
    mouse_indev = lv_indev_create();
    lv_indev_set_type(mouse_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(mouse_indev, mouse_read_cb);
}

/* ── main ───────────────────────────────────────────────── */
int main(int argc, char **argv) {
    (void)argc; (void)argv;

    printf("=== Radar PC Simulator ===\n");
    printf("dump1090: http://%s:%d%s\n", DUMP1090_HOST, DUMP1090_PORT, DUMP1090_PATH);
    printf("Home:     %.6f, %.6f\n", HOME_LAT, HOME_LON);
    printf("Range:    %.0f NM (%d px radius)\n\n", (double)RADAR_RANGE_NM, RADAR_RADIUS_PX);

    if (!sdl_init()) return 1;
    lvgl_init();
    radar_ui_init();

    /* initial data fetch */
    aircraft_count = aircraft_fetch(aircraft, MAX_AIRCRAFT);
    if (aircraft_count < 0) aircraft_count = 0;

    radar_ui_update_aircraft(aircraft, aircraft_count);  /* computes positions */
    last_fetch = SDL_GetTicks();
    last_sweep = SDL_GetTicks();

    printf("Running — close window to exit.\n");

    /* ── main loop ──────────────────────────────────────── */
    while (running) {
        /* handle SDL events */
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT)
                running = false;
            else if (e.type == SDL_WINDOWEVENT &&
                     e.window.event == SDL_WINDOWEVENT_CLOSE)
                running = false;
            /* mouse: queue state so LVGL never misses a click */
            else if (e.type == SDL_MOUSEBUTTONDOWN &&
                     e.button.button == SDL_BUTTON_LEFT) {
                mouse_x     = e.button.x;
                mouse_y     = e.button.y;
                mouse_state = LV_INDEV_STATE_PRESSED;
                mouse_dirty = true;
            }
            else if (e.type == SDL_MOUSEBUTTONUP &&
                     e.button.button == SDL_BUTTON_LEFT) {
                mouse_x     = e.button.x;
                mouse_y     = e.button.y;
                mouse_state = LV_INDEV_STATE_RELEASED;
                mouse_dirty = true;
            }
            else if (e.type == SDL_MOUSEMOTION) {
                if (mouse_state == LV_INDEV_STATE_PRESSED)
                    continue;  /* SDL_BUTTON_DOWN already set coords */
                mouse_x     = e.motion.x;
                mouse_y     = e.motion.y;
                mouse_dirty = true;
            }
        }

        uint32_t now = SDL_GetTicks();

        /* LVGL tick */
        lv_timer_handler();
        lv_tick_inc(5);

        /* data refresh — back off after failures to avoid UI freezes */
        uint32_t interval = fetch_failed ? FETCH_BACKOFF_MS : DATA_REFRESH_MS;
        if (now - last_fetch >= interval) {
            int n = aircraft_fetch(aircraft, MAX_AIRCRAFT);
            if (n >= 0) {
                fetch_failed = false;
                aircraft_count = n;
            } else {
                fetch_failed = true;
            }
            radar_ui_update_aircraft(aircraft, aircraft_count);
            last_fetch = now;
        }

        /* update system status — drives sweep visibility, menu status */
        radar_ui_update_status(0, "127.0.0.1", true,
                               aircraft_count, !fetch_failed);

        /* sweep animation */
        if (now - last_sweep >= SWEEP_UPDATE_MS) {
            radar_ui_sweep_tick();
            radar_ui_update_aircraft(aircraft, aircraft_count);
            last_sweep = now;
        }

        /* render framebuffer to screen */
        SDL_UpdateTexture(texture, NULL, fb_buf, 480 * sizeof(uint32_t));
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);

        SDL_Delay(5);
    }

    /* cleanup */
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
