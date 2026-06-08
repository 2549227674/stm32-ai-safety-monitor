/*
 * imx_tracker.c - Minimal visual tracking loop for i.MX6ULL
 *
 * Captures MJPEG frames -> POSTs to OPi5 YOLO /api/track/frame ->
 * reads tracking_hint.offset_px -> computes gimbal step ->
 * calls pca9685_set_pose to move toward target.
 *
 * Writes /run/imx_visual_state.json for imx_safetyd double-confirm.
 * Never controls pump/relay/buzzer directly (safety domain of imx_safetyd).
 *
 * Usage:
 *   imx_tracker --mode once    (single frame, for testing)
 *   imx_tracker --mode loop    (continuous tracking loop)
 *
 * Environment variables:
 *   TRACK_ENABLE          0|1 (default 1)
 *   TRACK_STEP_US         gimbal step in us per frame (default 40)
 *   TRACK_DEADBAND_PX     pixel deadband before moving (default 40)
 *   TRACK_PAN_MIN_US      min pan pulse (default 1100)
 *   TRACK_PAN_MAX_US      max pan pulse (default 1900)
 *   TRACK_TILT_MIN_US     min tilt pulse (default 1100)
 *   TRACK_TILT_MAX_US     max tilt pulse (default 1900)
 *   OPI5_URL              OPi5 AI service URL
 *   CAPTURE_DEVICE        v4l2 device path
 *   VISUAL_STATE_PATH     path for visual state JSON (default /run/imx_visual_state.json)
 *   TRACK_INTERVAL_SEC    loop interval (default 2)
 *   AI_CONNECT_TIMEOUT_SEC  curl connect timeout (default 5)
 *   AI_MAX_TIME_SEC       curl max time (default 15)
 */

#include <errno.h>
#include <math.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define DEFAULT_OPI5_URL "http://127.0.0.1:8080"
#define DEFAULT_CAPTURE_DEVICE "/dev/video1"
#define DEFAULT_VISUAL_STATE_PATH "/run/imx_visual_state.json"
#define DEFAULT_TRACK_INTERVAL_SEC 2
#define DEFAULT_CONNECT_TIMEOUT 5
#define DEFAULT_MAX_TIME 15
#define DEFAULT_STEP_US 40
#define DEFAULT_DEADBAND_PX 40
#define DEFAULT_PAN_MIN 1100.0
#define DEFAULT_PAN_MAX 1900.0
#define DEFAULT_TILT_MIN 1100.0
#define DEFAULT_TILT_MAX 1900.0

#define POSE_TOOL "/opt/edge-ai-safety-monitor/pca9685_set_pose"
#define PCA9685_BUS "/dev/i2c-0"
#define PCA9685_ADDR 0x40

static volatile sig_atomic_t keep_running = 1;

static void handle_signal(int sig) {
    (void)sig;
    keep_running = 0;
}

static long long now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ((long long)ts.tv_sec * 1000LL) + (ts.tv_nsec / 1000000LL);
}

static void iso_time_utc(char *buf, size_t size) {
    time_t t = time(NULL);
    struct tm tm_utc;
    gmtime_r(&t, &tm_utc);
    strftime(buf, size, "%Y-%m-%dT%H:%M:%SZ", &tm_utc);
}

static int run_shell(const char *cmd) {
    int rc = system(cmd);
    if (rc == -1) return -1;
    if (WIFEXITED(rc)) return WEXITSTATUS(rc);
    return 128;
}

static int read_file(const char *path, char *buf, size_t size) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return -1;
    size_t n = fread(buf, 1, size - 1, fp);
    buf[n] = '\0';
    fclose(fp);
    return (int)n;
}

static int write_file(const char *path, const char *content) {
    FILE *fp = fopen(path, "w");
    if (!fp) return -1;
    fputs(content, fp);
    fclose(fp);
    return 0;
}

/* Simple JSON value extraction (no external dependency) */
static int json_extract_bool(const char *json, const char *key) {
    char needle[128];
    snprintf(needle, sizeof(needle), "\"%s\"", key);
    const char *p = strstr(json, needle);
    if (!p) return -1;
    p = strchr(p + strlen(needle), ':');
    if (!p) return -1;
    p++;
    while (*p == ' ' || *p == '\t') p++;
    if (strncmp(p, "true", 4) == 0) return 1;
    if (strncmp(p, "false", 5) == 0) return 0;
    return -1;
}

/* Extract tracking_hint.offset_px[0] and [1] */
static int extract_offset_px(const char *json, int *ox, int *oy) {
    const char *p = strstr(json, "\"offset_px\"");
    if (!p) return -1;
    p = strchr(p, '[');
    if (!p) return -1;
    p++;
    *ox = atoi(p);
    const char *comma = strchr(p, ',');
    if (!comma) return -1;
    *oy = atoi(comma + 1);
    return 0;
}

/* Extract best_fire.score */
static int extract_best_fire_score(const char *json, double *score) {
    const char *p = strstr(json, "\"best_fire\"");
    if (!p) return -1;
    const char *s = strstr(p, "\"score\"");
    if (!s) return -1;
    s = strchr(s, ':');
    if (!s) return -1;
    s++;
    while (*s == ' ' || *s == '\t') s++;
    *score = atof(s);
    return 0;
}

/* Extract best_fire.bbox */
static int extract_best_fire_bbox(const char *json, int bbox[4]) {
    const char *p = strstr(json, "\"best_fire\"");
    if (!p) return -1;
    const char *b = strstr(p, "\"bbox\"");
    if (!b) return -1;
    b = strchr(b, '[');
    if (!b) return -1;
    b++;
    for (int i = 0; i < 4; i++) {
        while (*b == ' ' || *b == '\t') b++;
        bbox[i] = atoi(b);
        b = strchr(b, (i < 3) ? ',' : ']');
        if (!b) return -1;
        b++;
    }
    return 0;
}

/* Capture a single MJPEG frame via v4l2-ctl */
static int capture_frame(const char *device, const char *output) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "v4l2-ctl -d %s --set-fmt-video=width=640,height=480,pixelformat=MJPG "
             "--stream-mmap=3 --stream-count=1 --stream-to=%s >/dev/null 2>&1",
             device, output);
    return run_shell(cmd);
}

/* POST image to OPi5 /api/track/frame and parse response */
static int post_track_frame(const char *opi5_url, const char *image_path,
                            const char *response_path, const char *http_path,
                            int connect_timeout, int max_time,
                            int *offset_x, int *offset_y,
                            double *fire_score, int fire_bbox[4]) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "curl -sS -X POST -F image=@%s "
             "-F 'metadata={\"device_id\":\"imx_tracker\",\"frame_id\":0}' "
             "%s/api/track/frame -o %s -w '%%{http_code}' "
             "--connect-timeout %d --max-time %d > %s 2>/dev/null",
             image_path, opi5_url, response_path,
             connect_timeout, max_time, http_path);

    int rc = run_shell(cmd);
    if (rc != 0) return -1;

    /* Read HTTP status code */
    char http_buf[32] = {0};
    read_file(http_path, http_buf, sizeof(http_buf));
    int http_code = atoi(http_buf);
    if (http_code != 200) return -2;

    /* Read response JSON */
    char json_buf[8192] = {0};
    if (read_file(response_path, json_buf, sizeof(json_buf)) <= 0) return -3;

    /* Verify control_allowed=false */
    int ca = json_extract_bool(json_buf, "control_allowed");
    if (ca != 0) {
        fprintf(stderr, "[tracker] ERROR: control_allowed is not false!\n");
        return -4;
    }

    /* Extract tracking offset */
    if (extract_offset_px(json_buf, offset_x, offset_y) != 0) {
        *offset_x = 0;
        *offset_y = 0;
    }

    /* Extract best_fire */
    *fire_score = 0;
    if (extract_best_fire_score(json_buf, fire_score) != 0) {
        *fire_score = 0;
    }
    if (extract_best_fire_bbox(json_buf, fire_bbox) != 0) {
        fire_bbox[0] = fire_bbox[1] = fire_bbox[2] = fire_bbox[3] = 0;
    }

    return 0;
}

/* Write visual state for imx_safetyd to read */
static void write_visual_state(const char *path, double fire_score, const char *label,
                               const char *alarm_phase) {
    char ts[32];
    iso_time_utc(ts, sizeof(ts));
    char buf[512];
    snprintf(buf, sizeof(buf),
             "{\n"
             "  \"fire_confirmed\": %s,\n"
             "  \"score\": %.3f,\n"
             "  \"label\": \"%s\",\n"
             "  \"ts\": \"%s\",\n"
             "  \"alarm_phase\": \"%s\"\n"
             "}\n",
             fire_score > 0 ? "true" : "false",
             fire_score,
             label ? label : "none",
             ts,
             alarm_phase ? alarm_phase : "IDLE");
    write_file(path, buf);
}

/* Move gimbal by offset */
static int move_gimbal(int offset_x, int offset_y, double step_us, double deadband_px,
                       double *cur_pan_us, double *cur_tilt_us,
                       double pan_min, double pan_max,
                       double tilt_min, double tilt_max) {
    /* Deadband check */
    if (abs(offset_x) < (int)deadband_px && abs(offset_y) < (int)deadband_px) {
        return 0;  /* Within deadband, no move needed */
    }

    /* Tilt first (Y axis, inverted: positive offset = target below center = tilt up) */
    if (abs(offset_y) >= (int)deadband_px) {
        double tilt_step = (offset_y > 0) ? -step_us : step_us;
        /* Positive offset_y means target below center, so we need to tilt down (increase pulse) */
        tilt_step = (offset_y > 0) ? step_us : -step_us;
        double new_tilt = *cur_tilt_us + tilt_step;
        if (new_tilt < tilt_min) new_tilt = tilt_min;
        if (new_tilt > tilt_max) new_tilt = tilt_max;
        *cur_tilt_us = new_tilt;
    }

    /* Pan (X axis: positive offset = target right = pan right) */
    if (abs(offset_x) >= (int)deadband_px) {
        double pan_step = (offset_x > 0) ? step_us : -step_us;
        double new_pan = *cur_pan_us + pan_step;
        if (new_pan < pan_min) new_pan = pan_min;
        if (new_pan > pan_max) new_pan = pan_max;
        *cur_pan_us = new_pan;
    }

    /* Call pca9685_set_pose */
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "%s --bus %s --addr 0x%02x --pan-us %.0f --tilt-us %.0f --hold-ms 200",
             POSE_TOOL, PCA9685_BUS, PCA9685_ADDR,
             *cur_pan_us, *cur_tilt_us);
    return run_shell(cmd);
}

static void usage(const char *prog) {
    fprintf(stderr,
            "Usage: %s --mode once|loop [options]\n"
            "  --opi5-url URL\n"
            "  --capture-device PATH\n"
            "  --visual-state PATH\n"
            "  --step-us N\n"
            "  --deadband-px N\n"
            "  --interval-sec N\n"
            "  --pan-us N (initial)\n"
            "  --tilt-us N (initial)\n"
            "  --help\n",
            prog);
}

int main(int argc, char **argv) {
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    const char *mode = "once";
    const char *opi5_url = getenv("OPI5_URL") ? getenv("OPI5_URL") : DEFAULT_OPI5_URL;
    const char *capture_dev = getenv("CAPTURE_DEVICE") ? getenv("CAPTURE_DEVICE") : DEFAULT_CAPTURE_DEVICE;
    const char *visual_state = getenv("VISUAL_STATE_PATH") ? getenv("VISUAL_STATE_PATH") : DEFAULT_VISUAL_STATE_PATH;
    int step_us = DEFAULT_STEP_US;
    int deadband_px = DEFAULT_DEADBAND_PX;
    int interval_sec = DEFAULT_TRACK_INTERVAL_SEC;
    double cur_pan = 1500.0;
    double cur_tilt = 1500.0;
    int connect_timeout = DEFAULT_CONNECT_TIMEOUT;
    int max_time = DEFAULT_MAX_TIME;

    const char *e;
    if ((e = getenv("TRACK_STEP_US"))) step_us = atoi(e);
    if ((e = getenv("TRACK_DEADBAND_PX"))) deadband_px = atoi(e);
    if ((e = getenv("TRACK_PAN_MIN_US"))) { /* just read, used below */ }
    if ((e = getenv("TRACK_INTERVAL_SEC"))) interval_sec = atoi(e);
    if ((e = getenv("AI_CONNECT_TIMEOUT_SEC"))) connect_timeout = atoi(e);
    if ((e = getenv("AI_MAX_TIME_SEC"))) max_time = atoi(e);

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--mode") == 0 && i + 1 < argc) {
            mode = argv[++i];
        } else if (strcmp(argv[i], "--opi5-url") == 0 && i + 1 < argc) {
            opi5_url = argv[++i];
        } else if (strcmp(argv[i], "--capture-device") == 0 && i + 1 < argc) {
            capture_dev = argv[++i];
        } else if (strcmp(argv[i], "--visual-state") == 0 && i + 1 < argc) {
            visual_state = argv[++i];
        } else if (strcmp(argv[i], "--step-us") == 0 && i + 1 < argc) {
            step_us = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--deadband-px") == 0 && i + 1 < argc) {
            deadband_px = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--interval-sec") == 0 && i + 1 < argc) {
            interval_sec = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--pan-us") == 0 && i + 1 < argc) {
            cur_pan = atof(argv[++i]);
        } else if (strcmp(argv[i], "--tilt-us") == 0 && i + 1 < argc) {
            cur_tilt = atof(argv[++i]);
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "unknown arg: %s\n", argv[i]);
            return 1;
        }
    }

    if (strcmp(mode, "once") != 0 && strcmp(mode, "loop") != 0) {
        fprintf(stderr, "--mode must be 'once' or 'loop'\n");
        return 1;
    }

    double pan_min = DEFAULT_PAN_MIN, pan_max = DEFAULT_PAN_MAX;
    double tilt_min = DEFAULT_TILT_MIN, tilt_max = DEFAULT_TILT_MAX;
    if ((e = getenv("TRACK_PAN_MIN_US"))) pan_min = atof(e);
    if ((e = getenv("TRACK_PAN_MAX_US"))) pan_max = atof(e);
    if ((e = getenv("TRACK_TILT_MIN_US"))) tilt_min = atof(e);
    if ((e = getenv("TRACK_TILT_MAX_US"))) tilt_max = atof(e);

    /* Temp files */
    char tmp_img[] = "/tmp/imx_tracker_frame.jpg";
    char tmp_resp[] = "/tmp/imx_tracker_response.json";
    char tmp_http[] = "/tmp/imx_tracker_http.txt";

    fprintf(stderr, "[tracker] mode=%s opi5=%s step_us=%d deadband=%d\n",
            mode, opi5_url, step_us, deadband_px);
    fprintf(stderr, "[tracker] pan=%.0f tilt=%.0f range=[%.0f-%.0f, %.0f-%.0f]\n",
            cur_pan, cur_tilt, pan_min, pan_max, tilt_min, tilt_max);

    do {
        long long t0 = now_ms();

        /* 1. Capture frame */
        if (capture_frame(capture_dev, tmp_img) != 0) {
            fprintf(stderr, "[tracker] capture failed\n");
            write_visual_state(visual_state, 0, "none", "CAPTURE_FAIL");
            if (strcmp(mode, "once") == 0) break;
            sleep(interval_sec);
            continue;
        }

        /* 2. POST to OPi5 */
        int offset_x = 0, offset_y = 0;
        double fire_score = 0;
        int fire_bbox[4] = {0, 0, 0, 0};

        int rc = post_track_frame(opi5_url, tmp_img, tmp_resp, tmp_http,
                                  connect_timeout, max_time,
                                  &offset_x, &offset_y, &fire_score, fire_bbox);

        if (rc != 0) {
            fprintf(stderr, "[tracker] POST failed rc=%d\n", rc);
            write_visual_state(visual_state, 0, "none", "AI_OFFLINE");
            if (strcmp(mode, "once") == 0) break;
            sleep(interval_sec);
            continue;
        }

        fprintf(stderr, "[tracker] offset=[%d,%d] fire_score=%.3f bbox=[%d,%d,%d,%d]\n",
                offset_x, offset_y, fire_score,
                fire_bbox[0], fire_bbox[1], fire_bbox[2], fire_bbox[3]);

        /* 3. Write visual state for imx_safetyd */
        const char *label = "none";
        const char *phase = "IDLE";
        if (fire_score > 0.5) {
            label = "fire";
            phase = "AIMING";
        }
        write_visual_state(visual_state, fire_score, label, phase);

        /* 4. Move gimbal toward target */
        if (fire_score > 0) {
            int mv = move_gimbal(offset_x, offset_y, (double)step_us, (double)deadband_px,
                                 &cur_pan, &cur_tilt, pan_min, pan_max, tilt_min, tilt_max);
            if (mv != 0) {
                fprintf(stderr, "[tracker] gimbal move failed\n");
            } else {
                fprintf(stderr, "[tracker] gimbal -> pan=%.0f tilt=%.0f\n", cur_pan, cur_tilt);
            }
        } else {
            fprintf(stderr, "[tracker] no target, gimbal held at pan=%.0f tilt=%.0f\n", cur_pan, cur_tilt);
        }

        long long elapsed = now_ms() - t0;
        fprintf(stderr, "[tracker] frame done in %lld ms\n", elapsed);

        if (strcmp(mode, "once") == 0) break;

        /* Wait for interval */
        int wait_sec = interval_sec - (int)(elapsed / 1000);
        if (wait_sec > 0 && keep_running) {
            sleep(wait_sec);
        }

    } while (keep_running && strcmp(mode, "loop") == 0);

    /* Cleanup temp files */
    (void)unlink(tmp_img);
    (void)unlink(tmp_resp);
    (void)unlink(tmp_http);

    fprintf(stderr, "[tracker] exiting\n");
    return 0;
}
