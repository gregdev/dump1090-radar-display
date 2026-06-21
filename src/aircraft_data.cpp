#include "aircraft_data.h"
#include "config.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef ESP32
/* ── ESP32 path ─────────────────────────────────────────── */
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

int aircraft_fetch(aircraft_t *aircraft, int max_count) {
    if (WiFi.status() != WL_CONNECTED) return -1;

    char url[128];
    snprintf(url, sizeof(url), "http://%s:%d%s",
             DUMP1090_HOST, DUMP1090_PORT, DUMP1090_PATH);

    HTTPClient http;
    http.begin(url);
    http.setTimeout(3000);

    int code = http.GET();
    if (code != 200) {
        http.end();
        return -1;
    }

    String payload = http.getString();
    http.end();

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) return -1;

    JsonArray arr = doc["aircraft"].as<JsonArray>();
    if (!arr) return -1;

    int count = 0;
    for (JsonObject a : arr) {
        if (count >= max_count) break;

        /* skip aircraft without position */
        if (!a["lat"].is<double>() || !a["lon"].is<double>()) continue;

        strncpy(aircraft[count].hex,    a["hex"]    | "", sizeof(aircraft[count].hex) - 1);
        strncpy(aircraft[count].flight, a["flight"] | "", sizeof(aircraft[count].flight) - 1);

        /* trim trailing spaces from callsign */
        char *p = aircraft[count].flight;
        while (*p) p++;
        while (p > aircraft[count].flight && *(p-1) == ' ') *--p = '\0';

        aircraft[count].lat       = a["lat"];
        aircraft[count].lon       = a["lon"];
        aircraft[count].altitude  = a["alt_geom"] | a["alt_baro"] | 0;
        aircraft[count].speed     = a["gs"]       | 0;
        aircraft[count].track     = a["track"]    | 0;
        aircraft[count].vert_rate = a["vert_rate"]| 0;
        aircraft[count].seen      = a["seen"]     | 0;
        count++;
    }

    return count;
}

#else  /* ── PC / simulator path ──────────────────────────── */

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "ws2_32.lib")
  #define socklen_t int
  typedef SOCKET sock_t;
  #define SOCK_ERR SOCKET_ERROR
  #define sock_close closesocket
  static int sock_init(void) {
      WSADATA wsa;
      return WSAStartup(MAKEWORD(2,2), &wsa);
  }
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <netdb.h>
  #include <unistd.h>
  #include <fcntl.h>
  #include <errno.h>
  typedef int sock_t;
  #define SOCK_ERR (-1)
  #define sock_close close
  static int sock_init(void) { return 0; }
#endif

/* minimal JSON pull parser — just enough for dump1090 aircraft.json */
typedef struct {
    const char *buf;
    int len;
    int pos;
} JsonParser;

static void skip_ws(JsonParser *j) {
    while (j->pos < j->len &&
           (j->buf[j->pos] == ' '  || j->buf[j->pos] == '\t' ||
            j->buf[j->pos] == '\n' || j->buf[j->pos] == '\r'))
        j->pos++;
}

static int expect(JsonParser *j, char c) {
    skip_ws(j);
    if (j->pos < j->len && j->buf[j->pos] == c) { j->pos++; return 1; }
    return 0;
}

static int read_string(JsonParser *j, char *out, int out_sz) {
    skip_ws(j);
    if (!expect(j, '"')) return 0;
    int n = 0;
    while (j->pos < j->len && j->buf[j->pos] != '"' && n < out_sz - 1) {
        out[n++] = j->buf[j->pos++];
    }
    out[n] = '\0';
    return expect(j, '"');
}

static double read_number(JsonParser *j) {
    skip_ws(j);
    char *end;
    double v = strtod(j->buf + j->pos, &end);
    j->pos = (int)(end - j->buf);
    return v;
}

static void skip_value(JsonParser *j) {
    skip_ws(j);
    if (j->pos >= j->len) return;
    char c = j->buf[j->pos];
    if (c == '"') {
        j->pos++;
        while (j->pos < j->len && j->buf[j->pos] != '"') j->pos++;
        j->pos++; /* closing quote */
    } else if (c == '{') {
        int depth = 1; j->pos++;
        while (j->pos < j->len && depth) {
            if (j->buf[j->pos] == '{') depth++;
            if (j->buf[j->pos] == '}') depth--;
            j->pos++;
        }
    } else if (c == '[') {
        int depth = 1; j->pos++;
        while (j->pos < j->len && depth) {
            if (j->buf[j->pos] == '[') depth++;
            if (j->buf[j->pos] == ']') depth--;
            j->pos++;
        }
    } else {
        while (j->pos < j->len && j->buf[j->pos] != ',' &&
               j->buf[j->pos] != '}' && j->buf[j->pos] != ']') j->pos++;
    }
}

static char *http_get(const char *host, int port, const char *path,
                      int *out_len) {
    sock_init();
    sock_t s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == (sock_t)SOCK_ERR) return NULL;

    struct hostent *he = gethostbyname(host);
    if (!he) { sock_close(s); return NULL; }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((u_short)port);
    memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);

    /* ── non-blocking connect with 2-second timeout ─── */
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(s, FIONBIO, &mode);
#else
    int flags = fcntl(s, F_GETFL, 0);
    fcntl(s, F_SETFL, flags | O_NONBLOCK);
#endif

    int cr = connect(s, (struct sockaddr*)&addr, sizeof(addr));
#ifdef _WIN32
    if (cr == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK)
#else
    if (cr < 0 && errno == EINPROGRESS)
#endif
    {
        fd_set wfds;
        FD_ZERO(&wfds);
        FD_SET(s, &wfds);
        struct timeval tv = { 2, 0 };  /* 2-second connect timeout */
        cr = select((int)(s + 1), NULL, &wfds, NULL, &tv);
        if (cr <= 0) { sock_close(s); return NULL; }
    } else if (cr < 0) {
        sock_close(s); return NULL;
    }

    /* back to blocking mode */
#ifdef _WIN32
    mode = 0;
    ioctlsocket(s, FIONBIO, &mode);
#else
    fcntl(s, F_SETFL, flags);
#endif

    /* 3-second I/O timeout */
    struct timeval tv = { 3, 0 };
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));


    char req[256];
    snprintf(req, sizeof(req),
             "GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n",
             path, host);
    send(s, req, (int)strlen(req), 0);

    char *buf = (char*)malloc(65536);
    int total = 0;
    while (total < 65535) {
        int n = recv(s, buf + total, 65535 - total, 0);
        if (n <= 0) break;
        total += n;
    }
    sock_close(s);

    /* find \r\n\r\n header separator */
    char *body = buf;
    for (int i = 0; i < total - 3; i++) {
        if (body[i] == '\r' && body[i+1] == '\n' &&
            body[i+2] == '\r' && body[i+3] == '\n') {
            body += i + 4;
            total -= (i + 4);
            break;
        }
    }
    /* realloc body to front */
    memmove(buf, body, total);
    buf[total] = '\0';
    *out_len = total;
    return buf;
}

int aircraft_fetch(aircraft_t *aircraft, int max_count) {
    char url[128];
    snprintf(url, sizeof(url), "http://%s:%d%s",
             DUMP1090_HOST, DUMP1090_PORT, DUMP1090_PATH);

    int data_len = 0;
    char *json = http_get(DUMP1090_HOST, DUMP1090_PORT, DUMP1090_PATH,
                          &data_len);
    if (!json) return -1;

    JsonParser jp = { json, data_len, 0 };
    skip_ws(&jp);

    /* dump1090 wraps aircraft in {"aircraft":[...],"now":...,...} */
    expect(&jp, '{');
    /* skip to "aircraft" key */
    int found = 0;
    while (jp.pos < jp.len && !found) {
        skip_ws(&jp);
        char key[32];
        if (!read_string(&jp, key, sizeof(key))) break;
        expect(&jp, ':');
        if (strcmp(key, "aircraft") == 0) { found = 1; break; }
        skip_value(&jp);
        if (jp.buf[jp.pos] == ',') jp.pos++;
    }
    if (!found) { free(json); return -1; }

    skip_ws(&jp);
    if (!expect(&jp, '[')) { free(json); return -1; }

    int count = 0;
    while (count < max_count) {
        skip_ws(&jp);
        if (jp.pos >= jp.len || jp.buf[jp.pos] == ']') break;
        if (!expect(&jp, '{')) break;

        memset(&aircraft[count], 0, sizeof(aircraft_t));
        double lat = 0, lon = 0;
        int has_lat = 0, has_lon = 0;

        while (jp.pos < jp.len && jp.buf[jp.pos] != '}') {
            skip_ws(&jp);
            if (jp.buf[jp.pos] == '}') break;
            if (jp.buf[jp.pos] == ',') { jp.pos++; continue; }

            char key[32];
            if (!read_string(&jp, key, sizeof(key))) break;
            expect(&jp, ':');

            if      (strcmp(key, "hex")       == 0) read_string(&jp, aircraft[count].hex, 8);
            else if (strcmp(key, "flight")    == 0) {
                read_string(&jp, aircraft[count].flight, 12);
                /* trim trailing spaces */
                char *p = aircraft[count].flight;
                while (*p) p++;
                while (p > aircraft[count].flight && *(p-1) == ' ') *--p = '\0';
            }
            else if (strcmp(key, "lat")       == 0) { lat = read_number(&jp); has_lat = 1; }
            else if (strcmp(key, "lon")       == 0) { lon = read_number(&jp); has_lon = 1; }
            else if (strcmp(key, "alt_geom")  == 0) aircraft[count].altitude = (int)read_number(&jp);
            else if (strcmp(key, "alt_baro")  == 0 && !aircraft[count].altitude)
                                                  aircraft[count].altitude = (int)read_number(&jp);
            else if (strcmp(key, "gs")        == 0) aircraft[count].speed = (int)read_number(&jp);
            else if (strcmp(key, "track")     == 0) aircraft[count].track = (int)read_number(&jp);
            else if (strcmp(key, "vert_rate") == 0) aircraft[count].vert_rate = (int)read_number(&jp);
            else if (strcmp(key, "seen")      == 0) aircraft[count].seen = (int)read_number(&jp);
            else skip_value(&jp);
        }
        expect(&jp, '}');

        if (has_lat && has_lon) {
            aircraft[count].lat = lat;
            aircraft[count].lon = lon;
            count++;
        }
        skip_ws(&jp);
        if (jp.buf[jp.pos] == ',') jp.pos++;
    }

    free(json);
    return count;
}
#endif  /* ESP32 / PC */
