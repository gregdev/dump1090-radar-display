#include "radar_ui.h"
#include "aircraft_data.h"
#include "config.h"
#include "land_mask.h"
#include "coord_convert.h"
#define _USE_MATH_DEFINES
#include <math.h>
#include <string.h>
#include <stdio.h>

/* ── LVGL v9 includes ──────────────────────────────────── */
#include "lvgl.h"

/* custom fonts */
#include "fonts/lv_font_monoid_12.c"

/* ── ESP Panel (ESP32 target only) ─────────────────────── */
#ifdef ARDUINO
#include "lgfx_config.h"
#include <Preferences.h>
#include <Wire.h>
#include <TAMC_GT911.h>

static LGFX *g_lcd = nullptr;  // for backlight callbacks
static TAMC_GT911 ts(19, 45, -1, -1, 480, 480);  // SDA, SCL, INT, RST, Xmax, Ymax
#else
class LGFX; /* forward decl for PC sim */
#endif

/* ── internal state ────────────────────────────────────── */
static lv_obj_t   *radar_canvas = NULL;
static uint8_t    *buf_mem     = NULL;    /* pixel buffer */
static int         buf_w, buf_h;
static lv_color_format_t buf_cf;

static lv_obj_t   *lbl_count   = NULL;    /* "Tracking N aircraft" */
static lv_obj_t   *lbl_range   = NULL;    /* range label */

static lv_obj_t   *compass_labels[4];     /* N / E / S / W */
static lv_obj_t   *ac_labels[MAX_AIRCRAFT]; /* callsign labels per aircraft */

static float      sweep_angle = 0.0f;     /* current sweep-line angle, degrees */

/* ── touch + menu state ────────────────────────────────── */
static lv_obj_t      *detail_popup  = NULL;
static lv_timer_t    *detail_timer  = NULL;
static lv_obj_t      *menu_panel    = NULL;
static lv_obj_t      *menu_content  = NULL;
static bool           menu_visible  = false;
static int            touch_start_x = 0, touch_start_y = 0;
static bool           touch_down    = false;

/* ── mutable settings (adjusted by menu) ───────────────── */
static float  current_range_nm  = RADAR_RANGE_NM;
static int    current_brightness = 80;     /* 1–100 */
static bool   show_labels       = true;
static bool   show_land         = true;
static bool   show_sweep        = false;  /* hidden in normal op, visible on error */

/* ── cached aircraft for tap-to-inspect ────────────────── */
static const aircraft_t *cached_aircraft = NULL;
static int               cached_count   = 0;

/* ── system status (updated from main) ─────────────────── */
static struct {
    bool wifi_connected;
    char wifi_ip[16];
    int  wifi_rssi;
    int  aircraft_total;
    bool feed_ok;
} sys_status = {false, "--", 0, 0, false};

/* ── colour themes ─────────────────────────────────────── */
typedef struct {
    lv_color_t bg, ring, ring_bright, sweep, aircraft, label,
               crosshair, coast, north_mark, menu_bg, menu_border,
               menu_text, menu_header;
} radar_theme_t;

static const radar_theme_t themes[] = {
    /* 0: Green */
    { lv_color_hex(0x001600), lv_color_hex(0x003300), lv_color_hex(0x006600),
      lv_color_hex(0x00FF33), lv_color_hex(0x00FF00), lv_color_hex(0x00CC00),
      lv_color_hex(0x004400), lv_color_hex(0x002210), lv_color_hex(0x00FF00),
      lv_color_hex(0x001400), lv_color_hex(0x005500), lv_color_hex(0x00AA00),
      lv_color_hex(0x00FF00) },
    /* 1: Blue */
    { lv_color_hex(0x000016), lv_color_hex(0x000033), lv_color_hex(0x000066),
      lv_color_hex(0x0033FF), lv_color_hex(0x0000FF), lv_color_hex(0x0000CC),
      lv_color_hex(0x000044), lv_color_hex(0x001022), lv_color_hex(0x0000FF),
      lv_color_hex(0x000014), lv_color_hex(0x000055), lv_color_hex(0x0000AA),
      lv_color_hex(0x0000FF) },
    /* 2: Red */
    { lv_color_hex(0x160000), lv_color_hex(0x330000), lv_color_hex(0x660000),
      lv_color_hex(0xFF3300), lv_color_hex(0xFF0000), lv_color_hex(0xCC0000),
      lv_color_hex(0x440000), lv_color_hex(0x221000), lv_color_hex(0xFF0000),
      lv_color_hex(0x140000), lv_color_hex(0x550000), lv_color_hex(0xAA0000),
      lv_color_hex(0xFF0000) },
    /* 3: Amber */
    { lv_color_hex(0x161000), lv_color_hex(0x332000), lv_color_hex(0x664000),
      lv_color_hex(0xFFCC00), lv_color_hex(0xFFAA00), lv_color_hex(0xCC8800),
      lv_color_hex(0x442200), lv_color_hex(0x221800), lv_color_hex(0xFFAA00),
      lv_color_hex(0x141000), lv_color_hex(0x553000), lv_color_hex(0xAA7700),
      lv_color_hex(0xFFAA00) },
};
#define THEME_COUNT 4

static int current_theme = 0;  /* 0=green, 1=blue, 2=red, 3=amber */

/* macro shortcuts — resolve to current theme at runtime */
#define CLR_BG           (themes[current_theme].bg)
#define CLR_RING         (themes[current_theme].ring)
#define CLR_RING_BRIGHT  (themes[current_theme].ring_bright)
#define CLR_SWEEP        (themes[current_theme].sweep)
#define CLR_AIRCRAFT     (themes[current_theme].aircraft)
#define CLR_LABEL        (themes[current_theme].label)
#define CLR_CROSSHAIR    (themes[current_theme].crosshair)
#define CLR_COAST         (themes[current_theme].coast)
#define CLR_NORTH_MARK   (themes[current_theme].north_mark)
#define CLR_MENU_BG      (themes[current_theme].menu_bg)
#define CLR_MENU_BORDER  (themes[current_theme].menu_border)
#define CLR_MENU_TEXT    (themes[current_theme].menu_text)
#define CLR_MENU_HEADER  (themes[current_theme].menu_header)

/* ── layout constants for circular cutout ──────────────── */
#define SCR_W            480
#define SCR_H            480
#define SCR_CX           240
#define SCR_CY           240
#define CIRCLE_R         RADAR_RADIUS_PX

/* forward decls */
static void apply_theme_to_widgets(void);
static void build_menu_content(lv_obj_t *parent);
static void menu_update_status_text(void);
static void save_settings(void);
static void load_settings(void);
static void handle_tap(int x, int y);

/* helper: is a screen point inside the visible radar circle? */
static inline bool pt_in_circle(int x, int y) {
    int dx = x - SCR_CX, dy = y - SCR_CY;
    return (dx * dx + dy * dy) <= (CIRCLE_R * CIRCLE_R);
}

/* ── helpers: RGB565 pixel buffer drawing ──────────────── */

/* NOTE: The canvas buffer is RGB565 (2 bytes/pixel). We must use
 * lv_color16_t (2 bytes) for direct buffer access. lv_color_t is
 * 3 bytes in LVGL v9 even for RGB565 and would overflow the buffer. */

static inline void buf_set_px(int x, int y, lv_color_t c) {
    if (x < 0 || x >= buf_w || y < 0 || y >= buf_h) return;
    int idx = y * buf_w + x;
    lv_color16_t c16;
    c16.red   = c.red   >> 3;
    c16.green = c.green >> 2;
    c16.blue  = c.blue  >> 3;
    ((lv_color16_t*)buf_mem)[idx] = c16;
}

static inline lv_color_t buf_get_px(int x, int y) {
    if (x < 0 || x >= buf_w || y < 0 || y >= buf_h) return lv_color_black();
    lv_color16_t c16 = ((lv_color16_t*)buf_mem)[y * buf_w + x];
    lv_color_t c;
    c.red   = c16.red   << 3;
    c.green = c16.green << 2;
    c.blue  = c16.blue  << 3;
    return c;
}

/* fast word-level fill for RGB565 (2 bytes/pixel) */
static void buf_fill(lv_color_t c) {
    int total = buf_w * buf_h;
    lv_color16_t c16;
    c16.red   = c.red   >> 3;
    c16.green = c.green >> 2;
    c16.blue  = c.blue  >> 3;
    uint16_t v = *(uint16_t*)&c16;
    /* write as 32-bit pairs where possible */
    uint32_t pair = ((uint32_t)v << 16) | v;
    uint32_t *dst = (uint32_t*)buf_mem;
    int i;
    for (i = 0; i < total / 2; i++) dst[i] = pair;
    if (total & 1) ((uint16_t*)buf_mem)[total - 1] = v;
}

/* Bresenham line */
static void buf_draw_line(int x0, int y0, int x1, int y1, lv_color_t c) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;
    for (;;) {
        buf_set_px(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

/* midpoint circle (outline) */
static void buf_draw_circle(int cx, int cy, int r, lv_color_t c) {
    int x = 0, y = r, d = 1 - r;
    while (x <= y) {
        buf_set_px(cx + x, cy + y, c); buf_set_px(cx - x, cy + y, c);
        buf_set_px(cx + x, cy - y, c); buf_set_px(cx - x, cy - y, c);
        buf_set_px(cx + y, cy + x, c); buf_set_px(cx - y, cy + x, c);
        buf_set_px(cx + y, cy - x, c); buf_set_px(cx - y, cy - x, c);
        if (d < 0) d += 2 * x + 3;
        else { d += 2 * (x - y) + 5; y--; }
        x++;
    }
}

/* filled circle */
static void buf_fill_circle(int cx, int cy, int r, lv_color_t c) {
    int x = 0, y = r, d = 1 - r;
    while (x <= y) {
        for (int i = cx - x; i <= cx + x; i++) {
            buf_set_px(i, cy + y, c);
            buf_set_px(i, cy - y, c);
        }
        for (int i = cx - y; i <= cx + y; i++) {
            buf_set_px(i, cy + x, c);
            buf_set_px(i, cy - x, c);
        }
        if (d < 0) d += 2 * x + 3;
        else { d += 2 * (x - y) + 5; y--; }
        x++;
    }
}

/* draw text into buffer (minimal 5x7 font) — for on-canvas labels */
static void buf_draw_char(int x, int y, char ch, lv_color_t c) {
    /* bitmaps for digits 0-9, letters A-Z */
    static const uint8_t glyphs[][7] = {
        /* 0-9 */
        {0x38,0x44,0x4C,0x54,0x64,0x44,0x38}, /* 0 */
        {0x10,0x30,0x10,0x10,0x10,0x10,0x7C}, /* 1 */
        {0x38,0x44,0x04,0x08,0x10,0x20,0x7C}, /* 2 */
        {0x38,0x44,0x04,0x18,0x04,0x44,0x38}, /* 3 */
        {0x08,0x18,0x28,0x48,0x7C,0x08,0x08}, /* 4 */
        {0x7C,0x40,0x78,0x04,0x04,0x44,0x38}, /* 5 */
        {0x38,0x40,0x78,0x44,0x44,0x44,0x38}, /* 6 */
        {0x7C,0x04,0x08,0x10,0x20,0x20,0x20}, /* 7 */
        {0x38,0x44,0x44,0x38,0x44,0x44,0x38}, /* 8 */
        {0x38,0x44,0x44,0x3C,0x04,0x44,0x38}, /* 9 */
    };
    static const uint8_t alpha[][7] = {
        /* A-Z */
        {0x10,0x28,0x44,0x44,0x7C,0x44,0x44}, /* A */
        {0x78,0x44,0x44,0x78,0x44,0x44,0x78}, /* B */
        {0x38,0x44,0x40,0x40,0x40,0x44,0x38}, /* C */
        {0x78,0x44,0x44,0x44,0x44,0x44,0x78}, /* D */
        {0x7C,0x40,0x40,0x78,0x40,0x40,0x7C}, /* E */
        {0x7C,0x40,0x40,0x78,0x40,0x40,0x40}, /* F */
        {0x38,0x44,0x40,0x4C,0x44,0x44,0x38}, /* G */
        {0x44,0x44,0x44,0x7C,0x44,0x44,0x44}, /* H */
        {0x7C,0x10,0x10,0x10,0x10,0x10,0x7C}, /* I */
        {0x04,0x04,0x04,0x04,0x04,0x44,0x38}, /* J */
        {0x44,0x48,0x50,0x60,0x50,0x48,0x44}, /* K */
        {0x40,0x40,0x40,0x40,0x40,0x40,0x7C}, /* L */
        {0x44,0x6C,0x54,0x54,0x44,0x44,0x44}, /* M */
        {0x44,0x64,0x54,0x4C,0x44,0x44,0x44}, /* N */
        {0x38,0x44,0x44,0x44,0x44,0x44,0x38}, /* O */
        {0x78,0x44,0x44,0x78,0x40,0x40,0x40}, /* P */
        {0x38,0x44,0x44,0x44,0x54,0x48,0x34}, /* Q */
        {0x78,0x44,0x44,0x78,0x50,0x48,0x44}, /* R */
        {0x38,0x44,0x40,0x38,0x04,0x44,0x38}, /* S */
        {0x7C,0x10,0x10,0x10,0x10,0x10,0x10}, /* T */
        {0x44,0x44,0x44,0x44,0x44,0x44,0x38}, /* U */
        {0x44,0x44,0x44,0x44,0x28,0x28,0x10}, /* V */
        {0x44,0x44,0x44,0x54,0x54,0x6C,0x44}, /* W */
        {0x44,0x44,0x28,0x10,0x28,0x44,0x44}, /* X */
        {0x44,0x44,0x28,0x10,0x10,0x10,0x10}, /* Y */
        {0x7C,0x04,0x08,0x10,0x20,0x40,0x7C}, /* Z */
    };

    if (ch < ' ' || ch > 'z') return;
    const uint8_t *bmp = NULL;
    if (ch >= '0' && ch <= '9')       bmp = glyphs[ch - '0'];
    else if (ch >= 'A' && ch <= 'Z')  bmp = alpha[ch - 'A'];
    else if (ch >= 'a' && ch <= 'z')  bmp = alpha[ch - 'a'];
    else return;

    for (int row = 0; row < 7; row++) {
        uint8_t bits = bmp[row];
        for (int col = 0; col < 6; col++) {
            if (bits & (0x80 >> col))
                buf_set_px(x + col, y + row, c);
        }
    }
}

static void buf_draw_text(int x, int y, const char *s, lv_color_t c) {
    while (*s) {
        buf_draw_char(x, y, *s++, c);
        x += 7;   /* 6px char + 1px gap */
    }
}

/* ── land mask overlay (nearest-neighbour scaled) ──────── */
static void radar_draw_land(void) {
    /* can't show land at ranges larger than the base mask */
    if (current_range_nm > LAND_MASK_BASE_RANGE_NM + 0.1f) return;

    float scale = current_range_nm / LAND_MASK_BASE_RANGE_NM;  /* < 1.0 = zoom in */
    const uint16_t *mask = (const uint16_t *)land_mask_rgb565;
    uint16_t *dst = (uint16_t *)buf_mem;
    int mc = LAND_MASK_W / 2;  /* mask centre */

    for (int y = 0; y < buf_h; y++) {
        int sy = mc + (int)((y - mc) * scale);
        if (sy < 0 || sy >= LAND_MASK_H) continue;
        uint16_t *drow = dst + y * buf_w;
        const uint16_t *srow = mask + sy * LAND_MASK_W;

        for (int x = 0; x < buf_w; x++) {
            int sx = mc + (int)((x - mc) * scale);
            if (sx < 0 || sx >= LAND_MASK_W) continue;
            uint16_t v = srow[sx];
            if (v != 0) drow[x] = v;
        }
    }
}

/* ── radar drawing ─────────────────────────────────────── */

static int cx, cy;   /* canvas centre (set in radar_draw_static) */

static void radar_draw_static(void) {
    if (!buf_mem) {
        Serial.println("ERROR: radar_draw_static() called with NULL buf_mem!");
        return;
    }
    buf_fill(CLR_BG);

    cx = buf_w / 2;
    cy = buf_h / 2;

    /* land mass — respect toggle */
    if (show_land) radar_draw_land();

    /* range rings */
    int rings[] = { RADAR_RADIUS_PX / 4, RADAR_RADIUS_PX / 2,
                    3 * RADAR_RADIUS_PX / 4, RADAR_RADIUS_PX };
    for (int i = 0; i < 4; i++) {
        buf_draw_circle(cx, cy, rings[i], i == 3 ? CLR_RING_BRIGHT : CLR_RING);
    }

    /* crosshair */
    buf_draw_line(cx, 0, cx, buf_h, CLR_CROSSHAIR);
    buf_draw_line(0, cy, buf_w, cy, CLR_CROSSHAIR);

    /* tick marks every 30° on outer ring */
    for (int deg = 0; deg < 360; deg += 30) {
        float rad = (float)(deg - 90) * (float)M_PI / 180.0f;
        int x1 = cx + (int)((RADAR_RADIUS_PX - 8) * cosf(rad));
        int y1 = cy + (int)((RADAR_RADIUS_PX - 8) * sinf(rad));
        int x2 = cx + (int)( RADAR_RADIUS_PX       * cosf(rad));
        int y2 = cy + (int)( RADAR_RADIUS_PX       * sinf(rad));
        buf_draw_line(x1, y1, x2, y2, deg == 0 ? CLR_NORTH_MARK : CLR_RING_BRIGHT);
    }
}

static void radar_draw_aircraft(const aircraft_t *ac, int count) {
    for (int i = 0; i < count; i++) {
        if (!ac[i].on_screen) continue;
        int px = cx + ac[i].screen_x;
        int py = cy + ac[i].screen_y;

        /* aircraft dot (small filled circle, 3px radius) */
        buf_fill_circle(px, py, 3, CLR_AIRCRAFT);

        /* direction indicator — small line in track direction */
        if (ac[i].speed > 0) {
            float trk_rad = (float)(ac[i].track - 90) * (float)M_PI / 180.0f;
            int tx = px + (int)(8.0f * cosf(trk_rad));
            int ty = py + (int)(8.0f * sinf(trk_rad));
            buf_draw_line(px, py, tx, ty, CLR_AIRCRAFT);
        }

        /* labels — respect toggle */
        if (!show_labels) continue;

        /* two-line data block (old-school ATC style) */
        const char *id = ac[i].flight[0] ? ac[i].flight : ac[i].hex;
        char line1[16], line2[16];
        snprintf(line1, sizeof(line1), "%s", id);
        snprintf(line2, sizeof(line2), "FL%03d", ac[i].altitude / 100);

        /* Position to the right of the dot; flip left if too close to
           the right edge.  Each char is 7 px (6 + 1 gap).            */
        int lx = px + 6;
        int max_w = (int)strlen(line1);
        if ((int)strlen(line2) > max_w) max_w = (int)strlen(line2);
        int label_w = max_w * 7;
        if (lx + label_w >= buf_w) lx = px - label_w - 6;
        if (lx < 0) lx = 0;

        buf_draw_text(lx, py - 9, line1, CLR_LABEL);  /* above dot */
        buf_draw_text(lx, py + 2, line2, CLR_LABEL);  /* below dot */
    }
}

static void radar_draw_sweep(void) {
    float rad = (sweep_angle - 90.0f) * (float)M_PI / 180.0f;
    int ex = cx + (int)(RADAR_RADIUS_PX * cosf(rad));
    int ey = cy + (int)(RADAR_RADIUS_PX * sinf(rad));
    buf_draw_line(cx, cy, ex, ey, CLR_SWEEP);

    /* slight afterglow: draw a second, dimmer line slightly behind */
    float rad2 = (sweep_angle - 93.0f) * (float)M_PI / 180.0f;
    int ex2 = cx + (int)(RADAR_RADIUS_PX * cosf(rad2));
    int ey2 = cy + (int)(RADAR_RADIUS_PX * sinf(rad2));
    buf_draw_line(cx, cy, ex2, ey2, CLR_RING_BRIGHT);
}

/* ── public API ────────────────────────────────────────── */

void radar_ui_init(void) {
    lv_obj_t *scr = lv_scr_act();

    /* ── prevent screen scrolling (steals drag events) ── */
    lv_obj_set_scroll_dir(scr, LV_DIR_NONE);
    lv_obj_set_style_bg_color(scr, CLR_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    /* full-screen canvas for radar graphics */
    radar_canvas = lv_canvas_create(scr);
    lv_obj_set_size(radar_canvas, 480, 480);
    lv_obj_align(radar_canvas, LV_ALIGN_CENTER, 0, 0);

    /* allocate draw buffer */
    buf_w = 480; buf_h = 480;
    buf_cf = LV_COLOR_FORMAT_RGB565;
    size_t buf_size = LV_CANVAS_BUF_SIZE(buf_w, buf_h,
        LV_COLOR_FORMAT_GET_BPP(buf_cf), LV_DRAW_BUF_STRIDE_ALIGN);
    buf_mem = (uint8_t*)heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    Serial.printf("Canvas buffer: %p (%d bytes)\n", buf_mem, (int)buf_size);
    if (!buf_mem) return;

    lv_canvas_set_buffer(radar_canvas, buf_mem, buf_w, buf_h, buf_cf);

    /* draw static elements once */
    radar_draw_static();
    lv_obj_invalidate(radar_canvas);

    /* info labels — placed INSIDE the circular cutout */
    /* bottom-left: 20px above bottom of circle (cy + r - 20) */
    lbl_count = lv_label_create(scr);
    lv_label_set_text(lbl_count, "Initializing...");
    lv_obj_set_style_text_color(lbl_count, CLR_LABEL, 0);
    lv_obj_set_style_text_font(lbl_count, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(lbl_count, SCR_CX - CIRCLE_R + 8,
                   SCR_CY + CIRCLE_R - 28);
    lv_obj_add_flag(lbl_count, LV_OBJ_FLAG_HIDDEN);

    char rng[32];
    snprintf(rng, sizeof(rng), "RNG %.0f NM", (double)current_range_nm);
    lbl_range = lv_label_create(scr);
    lv_label_set_text(lbl_range, rng);
    lv_obj_set_style_text_color(lbl_range, CLR_LABEL, 0);
    lv_obj_set_style_text_font(lbl_range, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl_range, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_set_pos(lbl_range, SCR_CX + CIRCLE_R - 65,
                   SCR_CY + CIRCLE_R - 28);

    /* compass labels (N/E/S/W) — positioned just inside the outer ring */
    static const char *dirs[]  = {"N", "E", "S", "W"};
    static const int   degs[]  = {0, 90, 180, 270};
    for (int i = 0; i < 4; i++) {
        compass_labels[i] = lv_label_create(scr);
        lv_label_set_text(compass_labels[i], dirs[i]);
        lv_obj_set_style_text_color(compass_labels[i],
            i == 0 ? CLR_NORTH_MARK : CLR_RING_BRIGHT, 0);
        lv_obj_set_style_text_font(compass_labels[i], &lv_font_montserrat_14, 0);

        float rad = (float)(degs[i] - 90) * (float)M_PI / 180.0f;
        int lx = 240 + (int)((RADAR_RADIUS_PX - 18) * cosf(rad));
        int ly = 240 + (int)((RADAR_RADIUS_PX - 18) * sinf(rad));
        lv_obj_set_pos(compass_labels[i], lx - 8, ly - 8);
    }

    /* pre-create aircraft labels (hidden until used) */
    for (int i = 0; i < MAX_AIRCRAFT; i++) {
        ac_labels[i] = lv_label_create(scr);
        lv_obj_add_flag(ac_labels[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_text_color(ac_labels[i], CLR_LABEL, 0);
        lv_obj_set_style_text_font(ac_labels[i], &lv_font_montserrat_14, 0);
    }

    /* ── canvas touch events ──────────────────────────── */
    /* taps handled directly in indev read callback below —
       LVGL_CLICKED on canvas is unreliable on some configs */
    lv_obj_add_flag(radar_canvas, LV_OBJ_FLAG_CLICKABLE);

    /* ── load persisted settings from NVS flash ─────────── */
    load_settings();
}

void radar_ui_update_aircraft(const void *aircraft_array, int count) {
    aircraft_t *ac = (aircraft_t*)aircraft_array;  /* we update screen_x/y */

    /* cache for tap-to-inspect */
    cached_aircraft = ac;
    cached_count    = count;

    /* recompute screen positions using current range */
    for (int i = 0; i < count; i++) {
        int r = geo_to_pixel(ac[i].lat, ac[i].lon,
                             HOME_LAT, HOME_LON,
                             current_range_nm, RADAR_RADIUS_PX,
                             &ac[i].screen_x, &ac[i].screen_y);
        ac[i].on_screen = (r == 0) ? 1 : 0;
    }

    /* redraw only dynamic elements: aircraft + sweep on a fresh buffer */
    radar_draw_static();
    radar_draw_aircraft(ac, count);
    if (show_sweep) radar_draw_sweep();

    /* Notify LVGL the raw canvas buffer changed and force immediate render */
    lv_draw_buf_t *dbuf = lv_canvas_get_draw_buf(radar_canvas);
    if (dbuf) lv_draw_buf_invalidate_cache(dbuf, NULL);
    lv_obj_invalidate(radar_canvas);
    lv_refr_now(lv_obj_get_display(radar_canvas));

    /* update track count */
    int visible = 0;
    for (int i = 0; i < count; i++) if (ac[i].on_screen) visible++;
    static bool first_fetch = true;
    if (first_fetch && count > 0) {
        first_fetch = false;
        Serial.printf("Aircraft: %d fetched, %d on-screen\n", count, visible);
        for (int i = 0; i < count && i < 3; i++) {
            Serial.printf("  [%d] %s lat=%.4f lon=%.4f sx=%d sy=%d on=%d alt=%d\n",
                i, ac[i].flight[0] ? ac[i].flight : ac[i].hex,
                ac[i].lat, ac[i].lon, ac[i].screen_x, ac[i].screen_y,
                ac[i].on_screen, ac[i].altitude);
        }
    }
    char buf[32];
    snprintf(buf, sizeof(buf), "Tracking %d / %d", visible, count);
    lv_label_set_text(lbl_count, buf);
}

void radar_ui_sweep_tick(void) {
    sweep_angle += RADAR_SWEEP_SPEED;
    if (sweep_angle >= 360.0f) sweep_angle -= 360.0f;
}

void radar_ui_redraw(void) {
    if (!buf_mem || !cached_aircraft) return;
    radar_draw_static();
    radar_draw_aircraft(cached_aircraft, cached_count);
    if (show_sweep) radar_draw_sweep();
    lv_draw_buf_t *dbuf = lv_canvas_get_draw_buf(radar_canvas);
    if (dbuf) lv_draw_buf_invalidate_cache(dbuf, NULL);
    lv_obj_invalidate(radar_canvas);
    lv_refr_now(lv_obj_get_display(radar_canvas));
}

/* ═══════════════════════════════════════════════════════════
 *  Detail Popup — tap an aircraft to see its data card
 * ═══════════════════════════════════════════════════════════ */

static void detail_popup_hide(void) {
    if (detail_timer) { lv_timer_del(detail_timer); detail_timer = NULL; }
    if (detail_popup) { Serial.println("POPUP: hide"); lv_obj_del(detail_popup); detail_popup = NULL; }
}

static void detail_popup_auto_hide_cb(lv_timer_t *t) {
    (void)t;
    detail_popup_hide();
}

static void show_detail_popup(int tap_x, int tap_y, int ac_idx) {
    const aircraft_t *ac = &cached_aircraft[ac_idx];
    Serial.printf("POPUP: show idx=%d flight=%s\n", ac_idx, ac->flight[0] ? ac->flight : ac->hex);
    detail_popup_hide();

    /* ── container ─────────────────────────────────── */
    detail_popup = lv_obj_create(lv_scr_act());
    lv_obj_set_style_bg_color(detail_popup, CLR_MENU_BG, 0);
    lv_obj_set_style_bg_opa(detail_popup, LV_OPA_90, 0);
    lv_obj_set_style_border_color(detail_popup, CLR_MENU_BORDER, 0);
    lv_obj_set_style_border_width(detail_popup, 1, 0);
    lv_obj_set_style_radius(detail_popup, 4, 0);
    lv_obj_set_style_pad_all(detail_popup, 6, 0);
    lv_obj_set_size(detail_popup, 156, 74);

    /* ── text ──────────────────────────────────────── */
    const char *id = ac->flight[0] ? ac->flight : ac->hex;
    int fl  = ac->altitude / 100;
    int kt  = ac->speed;
    int trk = ac->track;
    int vr  = ac->vert_rate;

    /* distance from home in NM */
    float dx_px = (float)ac->screen_x;
    float dy_px = (float)ac->screen_y;
    float dist_nm = sqrtf(dx_px * dx_px + dy_px * dy_px)
                    * (current_range_nm / CIRCLE_R);

    char buf[192];
    snprintf(buf, sizeof(buf),
        "%s\n"
        "FL%03d  %d kt\n"
        "Trk %03d\u00b0  %s%d fpm\n"
        "%.0f NM",
        id,
        fl, kt,
        trk, vr >= 0 ? "\u2191" : "\u2193", abs(vr),
        dist_nm);

    lv_obj_t *label = lv_label_create(detail_popup);
    lv_label_set_text(label, buf);
    lv_obj_set_style_text_color(label, CLR_MENU_TEXT, 0);
    lv_obj_set_style_text_font(label, &lv_font_monoid_12, 0);

    /* ── position: near tap but clamp inside circle ─── */
    const int PW = 156, PH = 74;
    int px = tap_x + 12;
    int py = tap_y - PH / 2;

    /* clamp right edge */
    if (px + PW > SCR_CX + CIRCLE_R - 4) px = tap_x - PW - 12;
    /* clamp left edge */
    if (px < SCR_CX - CIRCLE_R + 4) px = SCR_CX - CIRCLE_R + 4;
    /* clamp top/bottom */
    if (py < SCR_CY - CIRCLE_R + 4) py = SCR_CY - CIRCLE_R + 4;
    if (py + PH > SCR_CY + CIRCLE_R - 4) py = SCR_CY + CIRCLE_R - PH - 4;

    lv_obj_set_pos(detail_popup, px, py);
    lv_obj_move_foreground(detail_popup);

    /* pass clicks through to canvas (popup sits on top) */
    lv_obj_add_flag(detail_popup, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(detail_popup, [](lv_event_t *e) {
        lv_indev_t *indev = lv_indev_active();
        if (!indev) return;
        lv_point_t pt;
        lv_indev_get_point(indev, &pt);
        handle_tap((int)pt.x, (int)pt.y);
    }, LV_EVENT_CLICKED, NULL);

    /* ── auto-hide after 4 s ───────────────────────── */
    detail_timer = lv_timer_create(detail_popup_auto_hide_cb, 4000, NULL);
    lv_timer_set_repeat_count(detail_timer, 1);
}

/* ═══════════════════════════════════════════════════════════
 *  Hidden Swipe Menu — slides up from botom of circle
 * ═══════════════════════════════════════════════════════════ */

#define MENU_Y_HIDDEN  SCR_H        /* off-screen below */
#define MENU_Y_VISIBLE (SCR_CY + CIRCLE_R - 208)  /* bottom 208px of circle */
#define MENU_H         208
#define MENU_W         (CIRCLE_R * 2 - 16)  /* 424 px — fits in circle */
#define MENU_X         (SCR_CX - MENU_W / 2)

static void menu_set_range_cb(lv_event_t *e) {
    lv_obj_t *btn = (lv_obj_t*)lv_event_get_target(e);
    lv_obj_t *lbl = lv_obj_get_child(btn, 0);
    const char *txt = lv_label_get_text(lbl);
    current_range_nm = (float)atof(txt);
    /* update range label */
    char rng[32];
    snprintf(rng, sizeof(rng), "RNG %.0f NM", (double)current_range_nm);
    lv_label_set_text(lbl_range, rng);

    /* recompute positions + redraw immediately */
    if (cached_aircraft && cached_count > 0)
        radar_ui_update_aircraft(cached_aircraft, cached_count);

    save_settings();
}

static void menu_toggle_labels_cb(lv_event_t *e) {
    show_labels = !show_labels;
    lv_obj_t *btn = (lv_obj_t*)lv_event_get_target(e);
    lv_obj_t *lbl = lv_obj_get_child(btn, 0);
    lv_label_set_text(lbl, show_labels ? "Labels: ON " : "Labels: OFF");
    save_settings();
}

static void menu_toggle_land_cb(lv_event_t *e) {
    show_land = !show_land;
    lv_obj_t *btn = (lv_obj_t*)lv_event_get_target(e);
    lv_obj_t *lbl = lv_obj_get_child(btn, 0);
    lv_label_set_text(lbl, show_land ? "Land: ON " : "Land: OFF");
    save_settings();
}

static void menu_set_brightness_cb(lv_event_t *e) {
    lv_obj_t *slider = (lv_obj_t*)lv_event_get_target(e);
    int val = (int)lv_slider_get_value(slider);
    current_brightness = val;
    extern void radar_ui_set_backlight(int percent);
    radar_ui_set_backlight(val);
    save_settings();
}

/* bridge: radar_ui.cpp -> backlight (set from outside) */
#ifdef ARDUINO
static void (*backlight_setter)(int percent) = NULL;

void radar_ui_set_backlight(int percent) {
    if (backlight_setter) backlight_setter(percent);
}
#else
void radar_ui_set_backlight(int percent) {
    (void)percent;  /* no-op on PC */
}
#endif

static void build_menu_content(lv_obj_t *parent) {
    /* parent is menu_content — uses flexbox column layout */
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(parent, 8, 0);
    lv_obj_set_style_pad_row(parent, 4, 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_0, 0);  /* transparent child */

    /* helper to make a section header */
    auto mk_hdr = [&](const char *txt) -> lv_obj_t* {
        lv_obj_t *l = lv_label_create(parent);
        lv_label_set_text(l, txt);
        lv_obj_set_style_text_color(l, CLR_MENU_HEADER, 0);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
        return l;
    };

    auto mk_btn = [&](const char *txt) -> lv_obj_t* {
        lv_obj_t *b = lv_obj_create(parent);
        lv_obj_set_size(b, LV_PCT(100), 32);
        lv_obj_set_style_bg_color(b, CLR_RING, 0);
        lv_obj_set_style_border_color(b, CLR_MENU_BORDER, 0);
        lv_obj_set_style_border_width(b, 1, 0);
        lv_obj_set_style_radius(b, 3, 0);
        lv_obj_set_style_pad_all(b, 2, 0);
        lv_obj_t *l = lv_label_create(b);
        lv_label_set_text(l, txt);
        lv_obj_center(l);
        lv_obj_set_style_text_color(l, CLR_MENU_TEXT, 0);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
        return b;
    };

    /* ── RANGE ─────────────────────────────────────── */
    mk_hdr("RANGE  (tap to set)");
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, LV_PCT(100), 32);
    lv_obj_set_style_bg_opa(row, LV_OPA_0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row, 4, 0);

    static const int ranges[] = {10, 20, 40, 80};
    for (int i = 0; i < 4; i++) {
        char rng[8]; snprintf(rng, sizeof(rng), "%d", ranges[i]);
        lv_obj_t *b = lv_obj_create(row);
        lv_obj_set_size(b, 54, 28);
        lv_obj_set_style_bg_color(b, CLR_RING, 0);
        lv_obj_set_style_border_color(b, CLR_MENU_BORDER, 0);
        lv_obj_set_style_border_width(b, 1, 0);
        lv_obj_set_style_radius(b, 3, 0);
        lv_obj_set_style_pad_all(b, 1, 0);
        lv_obj_t *l = lv_label_create(b);
        lv_label_set_text(l, rng);
        lv_obj_center(l);
        lv_obj_set_style_text_color(l, CLR_MENU_TEXT, 0);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
        lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(b, menu_set_range_cb, LV_EVENT_CLICKED, NULL);
    }

    /* ── DISPLAY ───────────────────────────────────── */
    mk_hdr("DISPLAY");

    lv_obj_t *btn = mk_btn("Labels: ON ");
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btn, menu_toggle_labels_cb, LV_EVENT_CLICKED, NULL);

    btn = mk_btn("Land: ON ");
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btn, menu_toggle_land_cb, LV_EVENT_CLICKED, NULL);

    /* brightness slider */
    lv_obj_t *sl_row = lv_obj_create(parent);
    lv_obj_set_size(sl_row, LV_PCT(100), 28);
    lv_obj_set_style_bg_opa(sl_row, LV_OPA_0, 0);
    lv_obj_set_style_pad_all(sl_row, 0, 0);
    lv_obj_set_style_border_width(sl_row, 0, 0);
    lv_obj_set_flex_flow(sl_row, LV_FLEX_FLOW_ROW);

    lv_obj_t *b_lbl = lv_label_create(sl_row);
    lv_label_set_text(b_lbl, "Bright");
    lv_obj_set_style_text_color(b_lbl, CLR_MENU_TEXT, 0);
    lv_obj_set_style_text_font(b_lbl, &lv_font_montserrat_14, 0);

    lv_obj_t *slider = lv_slider_create(sl_row);
    lv_obj_set_size(slider, LV_PCT(70), 10);
    lv_slider_set_range(slider, 1, 100);
    lv_slider_set_value(slider, current_brightness, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(slider, CLR_RING, LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider, CLR_SWEEP, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, CLR_AIRCRAFT, LV_PART_KNOB);
    lv_obj_add_event_cb(slider, menu_set_brightness_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* ── COLOUR THEME ──────────────────────────────── */
    mk_hdr("COLOUR");
    lv_obj_t *trow = lv_obj_create(parent);
    lv_obj_set_size(trow, LV_PCT(100), 28);
    lv_obj_set_style_bg_opa(trow, LV_OPA_0, 0);
    lv_obj_set_style_pad_all(trow, 0, 0);
    lv_obj_set_style_border_width(trow, 0, 0);
    lv_obj_set_flex_flow(trow, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(trow, 4, 0);

    static const char *tnames[] = {"Green", "Blue", "Red", "Amber"};
    for (int i = 0; i < 4; i++) {
        lv_obj_t *tb = lv_obj_create(trow);
        lv_obj_set_size(tb, 64, 28);
        lv_obj_set_style_bg_color(tb, themes[i].menu_bg, 0);
        lv_obj_set_style_border_color(tb, themes[i].menu_border, 0);
        lv_obj_set_style_border_width(tb, 1, 0);
        lv_obj_set_style_radius(tb, 3, 0);
        lv_obj_set_style_pad_all(tb, 1, 0);
        lv_obj_t *tl = lv_label_create(tb);
        lv_label_set_text(tl, tnames[i]);
        lv_obj_center(tl);
        lv_obj_set_style_text_color(tl, themes[i].menu_text, 0);
        lv_obj_set_style_text_font(tl, &lv_font_montserrat_14, 0);
        lv_obj_add_flag(tb, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(tb, [](lv_event_t *e) {
            lv_obj_t *tgt = (lv_obj_t*)lv_event_get_target(e);
            lv_obj_t *lbl = lv_obj_get_child(tgt, 0);
            const char *name = lv_label_get_text(lbl);
            if      (strcmp(name, "Green") == 0) current_theme = 0;
            else if (strcmp(name, "Blue")  == 0) current_theme = 1;
            else if (strcmp(name, "Red")   == 0) current_theme = 2;
            else                                 current_theme = 3;

            /* update all LVGL widget styles */
            apply_theme_to_widgets();

            /* repaint canvas buffer with new theme colours */
            radar_draw_static();
            if (cached_aircraft && cached_count > 0)
                radar_draw_aircraft(cached_aircraft, cached_count);
            if (show_sweep) radar_draw_sweep();
            lv_obj_invalidate(radar_canvas);

            /* rebuild menu content with new theme colours */
            if (menu_content) {
                lv_obj_clean(menu_content);
                build_menu_content(menu_content);
                menu_update_status_text();
            }
            save_settings();
        }, LV_EVENT_CLICKED, NULL);
    }

    /* ── SYSTEM STATUS ─────────────────────────────── */
    mk_hdr("SYSTEM");

    lv_obj_t *st = lv_label_create(parent);
    lv_obj_set_style_text_font(st, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(st, CLR_MENU_TEXT, 0);
    /* placeholder — updated by radar_ui_update_status */
    lv_label_set_text(st, "WiFi: --");
    /* store ref for updates (LVGL v9: user_data is void*) */
    lv_obj_set_user_data(parent, (void*)st);
}

/* ── re-apply theme to all persistent LVGL widgets ─────── */
static void apply_theme_to_widgets(void) {
    lv_obj_t *scr = lv_scr_act();

    /* screen background */
    lv_obj_set_style_bg_color(scr, CLR_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    /* bottom status labels */
    lv_obj_set_style_text_color(lbl_count, CLR_LABEL, 0);
    lv_obj_set_style_text_color(lbl_range,  CLR_LABEL, 0);

    /* compass labels */
    static const int cmap[4] = {0, 1, 1, 1}; /* N uses north_mark, E/S/W use ring_bright */
    for (int i = 0; i < 4; i++) {
        lv_obj_set_style_text_color(compass_labels[i],
            cmap[i] ? CLR_RING_BRIGHT : CLR_NORTH_MARK, 0);
    }

    /* pre-created aircraft labels */
    for (int i = 0; i < MAX_AIRCRAFT; i++) {
        if (ac_labels[i]) lv_obj_set_style_text_color(ac_labels[i], CLR_LABEL, 0);
    }

    /* detail popup (if visible) */
    if (detail_popup) {
        lv_obj_set_style_bg_color(detail_popup, CLR_MENU_BG, 0);
        lv_obj_set_style_border_color(detail_popup, CLR_MENU_BORDER, 0);
        /* update popup text label */
        lv_obj_t *child = lv_obj_get_child(detail_popup, 0);
        if (child) lv_obj_set_style_text_color(child, CLR_MENU_TEXT, 0);
    }

    /* menu panel (if visible) */
    if (menu_panel && menu_visible) {
        lv_obj_set_style_bg_color(menu_panel, CLR_MENU_BG, 0);
        lv_obj_set_style_border_color(menu_panel, CLR_MENU_BORDER, 0);
    }
}

/* ═══════════════════════════════════════════════════════════
 *  NVS Persistence (ESP32 only — settings survive reboot)
 * ═══════════════════════════════════════════════════════════ */

static void save_settings(void) {
#ifdef ARDUINO
    Preferences prefs;
    prefs.begin("radar", false);
    prefs.putInt("theme",      current_theme);
    prefs.putInt("range_x10",  (int)(current_range_nm * 10.0f + 0.5f));
    prefs.putInt("brightness", current_brightness);
    prefs.putBool("labels",    show_labels);
    prefs.putBool("land",      show_land);
    prefs.end();
#endif
}

static void load_settings(void) {
#ifdef ARDUINO
    Preferences prefs;
    prefs.begin("radar", true);  /* read-only */
    current_theme       = prefs.getInt("theme",      0);
    int rng             = prefs.getInt("range_x10",  (int)(RADAR_RANGE_NM * 10.0f));
    current_range_nm    = (float)rng / 10.0f;
    current_brightness  = prefs.getInt("brightness", 80);
    show_labels         = prefs.getBool("labels",    true);
    show_land           = prefs.getBool("land",      true);
    prefs.end();

    /* apply loaded brightness to backlight */
    radar_ui_set_backlight(current_brightness);
#endif
}

static void menu_update_status_text(void) {
    if (!menu_content) return;
    lv_obj_t *st = (lv_obj_t*)lv_obj_get_user_data(menu_content);
    if (!st) return;

    char buf[256];
    const char *wifi_state = sys_status.wifi_connected ? "OK" : "OFF";
    const char *feed_state = sys_status.feed_ok ? "OK" : "ERR";
    int rssi = sys_status.wifi_rssi;

    snprintf(buf, sizeof(buf),
        "WiFi: %s  IP: %s  RSSI: %d dBm\n"
        "Feed: %s  Tracking: %d aircraft",
        wifi_state, sys_status.wifi_ip, rssi,
        feed_state, sys_status.aircraft_total);
    lv_label_set_text(st, buf);
}

static void show_menu(void) {
    if (menu_visible) return;
    menu_visible = true;
    Serial.println("MENU: show");
    detail_popup_hide();  /* dismiss popup if open */

    if (!menu_panel) {
        /* create once */
        menu_panel = lv_obj_create(lv_scr_act());
        lv_obj_set_size(menu_panel, MENU_W, MENU_H);
        lv_obj_set_style_bg_color(menu_panel, CLR_MENU_BG, 0);
        lv_obj_set_style_bg_opa(menu_panel, LV_OPA_90, 0);
        lv_obj_set_style_border_color(menu_panel, CLR_MENU_BORDER, 0);
        lv_obj_set_style_border_width(menu_panel, 1, 0);
        lv_obj_set_style_radius(menu_panel, 6, 0);
        lv_obj_set_style_pad_all(menu_panel, 4, 0);

        /* scrollable content area — high threshold to avoid
         * accidental scroll when tapping menu buttons */
        menu_content = lv_obj_create(menu_panel);
        lv_obj_set_size(menu_content, LV_PCT(100), LV_PCT(100));
        lv_obj_set_style_bg_opa(menu_content, LV_OPA_0, 0);
        lv_obj_set_style_border_width(menu_content, 0, 0);
        lv_obj_set_style_pad_all(menu_content, 0, 0);
        lv_obj_set_scroll_dir(menu_content, LV_DIR_VER);
        lv_obj_set_scrollbar_mode(menu_content, LV_SCROLLBAR_MODE_OFF);
        lv_obj_set_scroll_snap_y(menu_content, LV_SCROLL_SNAP_NONE);
        lv_obj_clear_flag(menu_content, LV_OBJ_FLAG_SCROLL_ELASTIC);

        build_menu_content(menu_content);
    }

    menu_update_status_text();
    lv_obj_set_pos(menu_panel, MENU_X, MENU_Y_VISIBLE);
    lv_obj_move_foreground(menu_panel);
    lv_obj_clear_flag(menu_panel, LV_OBJ_FLAG_HIDDEN);
}

static void hide_menu(void) {
    if (!menu_visible) return;
    menu_visible = false;
    Serial.println("MENU: hide");
    if (menu_panel) {
        lv_obj_add_flag(menu_panel, LV_OBJ_FLAG_HIDDEN);
    }
}

/* ═══════════════════════════════════════════════════════════
 *  Touch Input Handler
 * ═══════════════════════════════════════════════════════════ */

static void handle_tap(int x, int y) {
    /* if popup is open, tapping anywhere dismisses it */
    if (detail_popup) {
        detail_popup_hide();
        return;
    }

    /* if menu is open, tapping outside it closes it */
    if (menu_visible) {
        lv_area_t ma;
        lv_obj_get_coords(menu_panel, &ma);
        if (x < ma.x1 || x > ma.x2 || y < ma.y1 || y > ma.y2) {
            hide_menu();
        }
        return;  /* taps inside menu handled by LVGL button callbacks */
    }

    /* tap outside circle? ignore */
    if (!pt_in_circle(x, y)) return;

    /* ── double-tap on empty area → toggle menu ──────── */
    static lv_point_t last_pt  = {-999, -999};
    static uint32_t   last_ms  = 0;
    uint32_t now = lv_tick_get();

    Serial.printf("  dt=%lu dx=%d dy=%d ac=%d\n",
                  (unsigned long)(now - last_ms),
                  abs(x - last_pt.x), abs(y - last_pt.y),
                  (cached_aircraft && cached_count > 0) ? cached_count : 0);

    if (abs(x - last_pt.x) < 25 && abs(y - last_pt.y) < 25
        && (now - last_ms) < 400 && last_pt.x != -999)
    {
        /* double-tap detected — toggle menu */
        last_pt.x = -999;  /* reset so triple-tap doesn't toggle again */
        show_menu();
        return;
    }
    last_pt.x = x;
    last_pt.y = y;
    last_ms   = now;

    /* ── single tap: find nearest aircraft ───────────── */
    if (!cached_aircraft || cached_count <= 0) return;

    int best_idx = -1;
    int best_dist2 = 484;  /* 22 px hit radius squared */

    for (int i = 0; i < cached_count; i++) {
        if (!cached_aircraft[i].on_screen) continue;
        int ax = SCR_CX + cached_aircraft[i].screen_x;
        int ay = SCR_CY + cached_aircraft[i].screen_y;
        int dx = x - ax, dy = y - ay;
        int d2 = dx * dx + dy * dy;
        if (d2 < best_dist2) {
            best_dist2 = d2;
            best_idx = i;
        }
    }

    if (best_idx >= 0) {
        show_detail_popup(x, y, best_idx);
    } else {
        /* single tap on empty area — do nothing
           (double-tap will open menu next time) */
    }
}

void radar_ui_handle_input(int16_t x, int16_t y, bool pressed) {
    if (pressed) {
        touch_start_x = x;
        touch_start_y = y;
        touch_down = true;
    } else if (touch_down) {
        touch_down = false;
        int dx = x - touch_start_x;
        int dy = y - touch_start_y;
        int adx = abs(dx), ady = abs(dy);

        /* distinguish swipe vs tap */
        if (ady > 40 && ady > adx) {
            /* vertical swipe */
            if (dy < -20 && pt_in_circle(touch_start_x, touch_start_y)) {
                /* swipe UP → show menu */
                show_menu();
            } else if (dy > 20 && menu_visible) {
                /* swipe DOWN → hide menu */
                hide_menu();
            }
        } else {
            /* anything else: treat as tap */
            handle_tap(x, y);
        }
    }
}

/* ═══════════════════════════════════════════════════════════
 *  Status Update (called from main loop)
 * ═══════════════════════════════════════════════════════════ */

void radar_ui_update_status(int wifi_rssi, const char *wifi_ip,
                            bool wifi_connected, int total_aircraft,
                            bool feed_ok) {
    sys_status.wifi_rssi     = wifi_rssi;
    sys_status.wifi_connected = wifi_connected;
    sys_status.feed_ok       = feed_ok;
    sys_status.aircraft_total = total_aircraft;
    if (wifi_ip) {
        strncpy(sys_status.wifi_ip, wifi_ip, sizeof(sys_status.wifi_ip) - 1);
        sys_status.wifi_ip[sizeof(sys_status.wifi_ip) - 1] = '\0';
    }

    /* sweep visible only when WiFi or dump1090 is down */
    show_sweep = !(wifi_connected && feed_ok);

    /* refresh menu status text if visible */
    if (menu_visible) menu_update_status_text();
}

/* ═══════════════════════════════════════════════════════════
 *  Touch Device Setup (called from main after init)
 * ═══════════════════════════════════════════════════════════ */

void * radar_ui_setup_touch(void *lcd_ptr) {
    lv_indev_t *indev = nullptr;
#ifdef ARDUINO
    g_lcd = static_cast<LGFX*>(lcd_ptr);

    /* Store backlight control for the brightness slider */
    backlight_setter = [](int percent) {
        if (g_lcd) g_lcd->setBrightness(constrain(percent, 0, 100) * 255 / 100);
    };

    /* Init GT911 via TAMCTec library (no LGFX throttling) */
    Wire.begin(19, 45, 400000);
    ts.begin();
    ts.setRotation(ROTATION_NORMAL);
    Serial.println("Touch: GT911 (TAMCTec) ready");

    /* Create LVGL input device for touch */
    indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, [](lv_indev_t *id, lv_indev_data_t *data) {
        static bool was_pressed = false;
        static lv_point_t press_pt = {0, 0};

        ts.read();
        if (ts.isTouched && ts.points[0].x > 0 && ts.points[0].y > 0) {
            int tx = map(ts.points[0].x, 480, 0, 0, 479);
            int ty = map(ts.points[0].y, 480, 0, 0, 479);
            data->point.x = tx;
            data->point.y = ty;
            data->state   = LV_INDEV_STATE_PRESSED;
            if (!was_pressed) {
                press_pt.x = tx;
                press_pt.y = ty;
            }
            was_pressed = true;
        } else {
            if (was_pressed) {
                Serial.printf("TAP: %d,%d\n", press_pt.x, press_pt.y);
                handle_tap(press_pt.x, press_pt.y);
            }
            data->state = LV_INDEV_STATE_RELEASED;
            was_pressed = false;
        }
    });

    Serial.println("Touch: LVGL indev registered");
#else
    (void)lcd_ptr;  /* no-op on PC simulator */
#endif
    return indev;
}

/* ═══════════════════════════════════════════════════════════
 *  Range Getter (called from main.cpp for coord conversion)
 * ═══════════════════════════════════════════════════════════ */

float radar_ui_get_range(void) {
    return current_range_nm;
}
