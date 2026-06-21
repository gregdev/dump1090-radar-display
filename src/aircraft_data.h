#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define MAX_AIRCRAFT 64

typedef struct {
    char  hex[8];         /* ICAO 24-bit hex */
    char  flight[12];     /* callsign, e.g. "BAW123" */
    double lat;
    double lon;
    int    altitude;      /* feet, barometric or geometric */
    int    speed;         /* knots */
    int    track;         /* degrees true */
    int    vert_rate;     /* ft/min */
    int    seen;          /* seconds since last message */
    /* derived */
    int    screen_x;
    int    screen_y;
    int    on_screen;     /* 1 = within radar circle */
} aircraft_t;

/**
 * Fetch aircraft.json from dump1090, parse into array.
 * Returns number of aircraft placed in `aircraft` buffer (max MAX_AIRCRAFT).
 * Returns -1 on HTTP / parse error.
 */
int aircraft_fetch(aircraft_t *aircraft, int max_count);

#ifdef __cplusplus
}
#endif
