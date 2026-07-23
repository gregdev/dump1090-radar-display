#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Convert lat/lon to pixel XY on the radar canvas.
 *
 * Uses an equirectangular approximation (fine for < 100 km).
 * Origin (0,0) = centre of canvas; y increases downward.
 *
 * Returns 0 if the aircraft is within the radar circle, -1 if out of range.
 */
int geo_to_pixel(double aircraft_lat, double aircraft_lon,
                 double home_lat,   double home_lon,
                 float range_nm,    int radius_px,
                 int *out_x, int *out_y);

/**
 * Inverse of geo_to_pixel ΓÇö pixel XY ΓåÆ lat/lon.
 * `x` and `y` are relative to canvas centre (like the output of geo_to_pixel).
 */
void pixel_to_geo(int x, int y,
                  double home_lat, double home_lon,
                  float range_nm,  int radius_px,
                  double *out_lat, double *out_lon);

#ifdef __cplusplus
}
#endif
