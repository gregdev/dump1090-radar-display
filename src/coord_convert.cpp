#include "coord_convert.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ΓöÇΓöÇ helpers ΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇ */

static double deg2rad(double d) { return d * M_PI / 180.0; }
static double rad2deg(double r) { return r * 180.0 / M_PI; }

/* nautical miles per degree of latitude (WGS-84) */
static double nm_per_deg_lat(double /*lat*/) {
    /* 1┬░ lat Γëê 60.04 NM (varies only ~0.1% over earth) */
    return 60.04;
}

/* nautical miles per degree of longitude at a given latitude */
static double nm_per_deg_lon(double lat) {
    return 60.04 * cos(deg2rad(lat));
}

/* ΓöÇΓöÇ public API ΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇ */

int geo_to_pixel(double aircraft_lat, double aircraft_lon,
                 double home_lat,   double home_lon,
                 float range_nm,    int radius_px,
                 int *out_x, int *out_y)
{
    /* differences in nautical miles */
    double dlat_nm = (aircraft_lat - home_lat) * nm_per_deg_lat(home_lat);
    double dlon_nm = (aircraft_lon - home_lon) * nm_per_deg_lon(home_lat);

    /* scale: pixels per nautical mile */
    float scale = (float)radius_px / range_nm;

    /* screen coords: x = east, y = north (inverted for screen) */
    int x = (int)( dlon_nm * scale);
    int y = (int)(-dlat_nm * scale);   /* north = up ΓåÆ negative y */

    /* distance from centre in pixels */
    float dist_px = sqrtf((float)(x*x + y*y));

    if (dist_px > (float)radius_px) {
        /* optionally clamp to edge of circle
        float s = (float)radius_px / dist_px;
        x = (int)(x * s);
        y = (int)(y * s); */
        return -1;   /* aircraft outside radar range */
    }

    *out_x = x;
    *out_y = y;
    return 0;
}

void pixel_to_geo(int x, int y,
                  double home_lat, double home_lon,
                  float range_nm,  int radius_px,
                  double *out_lat, double *out_lon)
{
    float scale = (float)radius_px / range_nm;   /* px per NM */
    double dlon_nm =  (double)x / scale;
    double dlat_nm = -(double)y / scale;         /* y increases downward */

    *out_lat = home_lat + dlat_nm / nm_per_deg_lat(home_lat);
    *out_lon = home_lon + dlon_nm / nm_per_deg_lon(home_lat);
}
