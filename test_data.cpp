/**
 * test_data.cpp — Standalone console test for the dump1090 data pipeline.
 * Builds with MSVC (no LVGL, no SDL, no Arduino).
 *
 * Compile from Developer PowerShell:
 *   cl /EHsc /std:c++17 /DPC_SIMULATOR /I src test_data.cpp src\aircraft_data.cpp src\coord_convert.cpp /Fe:test_data.exe /link ws2_32.lib
 *
 * Or from regular PowerShell after running vcvars:
 *   & "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
 *   cl /EHsc /std:c++17 /DPC_SIMULATOR /I src test_data.cpp src\aircraft_data.cpp src\coord_convert.cpp /Fe:test_data.exe /link ws2_32.lib
 *
 * Then run:
 *   .\test_data.exe
 */

#include <stdio.h>
#include <math.h>
#include "aircraft_data.h"
#include "coord_convert.h"
#include "config.h"

int main(void) {
    printf("=== Radar Data Pipeline Test ===\n");
    printf("dump1090: http://%s:%d%s\n", DUMP1090_HOST, DUMP1090_PORT, DUMP1090_PATH);
    printf("Home:     %.6f, %.6f\n", HOME_LAT, HOME_LON);
    printf("Range:    %.0f NM (radius %d px)\n", (double)RADAR_RANGE_NM, RADAR_RADIUS_PX);
    printf("\nFetching aircraft data...\n\n");

    aircraft_t aircraft[MAX_AIRCRAFT];
    int count = aircraft_fetch(aircraft, MAX_AIRCRAFT);

    if (count < 0) {
        printf("ERROR: Failed to fetch or parse aircraft.json\n");
        printf("Make sure dump1090 is running at http://%s:%d%s\n",
               DUMP1090_HOST, DUMP1090_PORT, DUMP1090_PATH);
        return 1;
    }

    printf("Received %d aircraft with position data.\n\n", count);

    if (count == 0) {
        printf("No aircraft in the air right now — try again later.\n");
        return 0;
    }

    printf("%-8s %-10s %7s %7s %6s %5s %4s %5s | %5s %5s %s\n",
           "Hex", "Flight", "Alt(ft)", "Spd(kt)", "Track", "VRate",
           "Seen", "DistNM", "X(px)", "Y(px)", "OnScreen");
    printf("--------------------------------------------------------------------------------\n");

    int on_screen = 0;
    for (int i = 0; i < count; i++) {
        int r = geo_to_pixel(aircraft[i].lat, aircraft[i].lon,
                             HOME_LAT, HOME_LON,
                             RADAR_RANGE_NM, RADAR_RADIUS_PX,
                             &aircraft[i].screen_x, &aircraft[i].screen_y);
        aircraft[i].on_screen = (r == 0) ? 1 : 0;
        if (aircraft[i].on_screen) on_screen++;

        /* rough distance in NM */
        double dlat = (aircraft[i].lat - HOME_LAT) * 60.04;
        double dlon = (aircraft[i].lon - HOME_LON) * 60.04 *
                       cos(HOME_LAT * 3.14159265 / 180.0);
        double dist_nm = sqrt(dlat*dlat + dlon*dlon);

        printf("%-8s %-10s %7d %7d %6d %5d %4d %5.1f | %5d %5d %s\n",
               aircraft[i].hex,
               aircraft[i].flight[0] ? aircraft[i].flight : "(none)",
               aircraft[i].altitude,
               aircraft[i].speed,
               aircraft[i].track,
               aircraft[i].vert_rate,
               aircraft[i].seen,
               dist_nm,
               aircraft[i].screen_x,
               aircraft[i].screen_y,
               aircraft[i].on_screen ? "YES" : "---");
    }

    printf("\n%d / %d aircraft within %d NM radar range.\n",
           on_screen, count, (int)RADAR_RANGE_NM);

    return 0;
}
