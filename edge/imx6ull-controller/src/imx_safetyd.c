#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define PROGRAM_NAME "imx_safetyd_c"
#define CONTRACT_VERSION "1.0"
#define DEFAULT_DEVICE_ID "labbox_001"
#define DEFAULT_GPIO_PIR 117
#define DEFAULT_GPIO_ACTIVE_HIGH 1
#define DEFAULT_GPIO_FLAME 119
#define DEFAULT_GPIO_FLAME_ACTIVE_HIGH 1
#define DEFAULT_CAPTURE_DEVICE "/dev/video1"
#define DEFAULT_BASE_DIR "/opt/edge-ai-safety-monitor"
#define DEFAULT_INTERVAL_SEC 2
#define DEFAULT_MODE "once"
#define DEFAULT_BACKEND_URL "http://127.0.0.1:5000"
#define DEFAULT_OPI5_URL "http://127.0.0.1:8080"
#define INITIAL_SEQ 9000

typedef struct {
    char opi5_url[256];
    char backend_url[256];
    char device_id[64];
    int gpio_pir;
    int gpio_active_high;
    int gpio_flame;
    int gpio_flame_active_high;
    char capture_device[64];
    char base_dir[256];
    int post_normal;
    int force_verify;
    int interval_sec;
    char mode[16];
} AppConfig;

typedef struct {
    int raw_value;
    int pir;
    int door;
    int flame;
    int flame_raw;
    int mq2;
} SensorState;

typedef struct {
    int seq;
    char state[16];
    int risk_score_before_ai;
    int risk_score;
    bool need_snap;
    bool capture_ok;
    char capture_file[512];
    char metadata_file[512];
    char ai_file[512];
    char ai_http_file[512];
    char event_file[512];
    char backend_response_file[512];
    char backend_http_file[512];
    char spool_path[512];
    char ai_status[32];
    char backend_status[32];
    char last_error[256];
    int gpio_ms;
    int capture_ms;
    int ai_ms;
    int post_ms;
    int total_ms;
} EventContext;

typedef struct {
    char capture_dir[512];
    char spool_dir[512];
    char pending_dir[512];
    char sent_dir[512];
    char run_dir[512];
    char seq_file[512];
    char log_file[512];
    char status_file[512];
} AppPaths;

static volatile sig_atomic_t keep_running = 1;
static char g_log_file[512];

static void handle_signal(int signum) {
    (void)signum;
    keep_running = 0;
}

static long long now_ms(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0;
    }
    return ((long long)ts.tv_sec * 1000LL) + (ts.tv_nsec / 1000000LL);
}

static void iso_time_utc(char *buf, size_t size) {
    time_t t = time(NULL);
    struct tm tm_utc;
    if (gmtime_r(&t, &tm_utc) == NULL) {
        snprintf(buf, size, "1970-01-01T00:00:00Z");
        return;
    }
    strftime(buf, size, "%Y-%m-%dT%H:%M:%SZ", &tm_utc);
}

static void sleep_seconds_interruptible(int seconds) {
    struct timespec req;
    req.tv_sec = seconds;
    req.tv_nsec = 0;
    while (keep_running && nanosleep(&req, &req) != 0 && errno == EINTR) {
    }
}

static void log_msg(const char *fmt, ...) {
    char ts[32];
    iso_time_utc(ts, sizeof(ts));

    va_list ap;
    va_start(ap, fmt);
    printf("[%s] ", ts);
    vprintf(fmt, ap);
    printf("\n");
    fflush(stdout);
    va_end(ap);

    if (g_log_file[0] == '\0') {
        return;
    }

    FILE *fp = fopen(g_log_file, "a");
    if (fp == NULL) {
        return;
    }
    va_start(ap, fmt);
    fprintf(fp, "[%s] ", ts);
    vfprintf(fp, fmt, ap);
    fprintf(fp, "\n");
    fclose(fp);
    va_end(ap);
}

static void usage(const char *prog) {
    printf(
        "Usage:\n"
        "  %s --mode once|loop|flush [options]\n"
        "\n"
        "Options:\n"
        "  --mode once|loop|flush\n"
        "  --opi5-url URL              default/env OPI5_URL\n"
        "  --backend-url URL           default/env BACKEND_URL\n"
        "  --device-id ID              default/env DEVICE_ID\n"
        "  --gpio-pir N                default/env GPIO_PIR\n"
        "  --gpio-active-high 0|1      default/env GPIO_ACTIVE_HIGH\n"
        "  --gpio-flame N              default/env GPIO_FLAME\n"
        "  --gpio-flame-active-high 0|1 default/env GPIO_FLAME_ACTIVE_HIGH\n"
        "  --capture-device PATH       default/env CAPTURE_DEVICE\n"
        "  --base-dir PATH             default/env BASE_DIR\n"
        "  --post-normal 0|1           default/env POST_NORMAL\n"
        "  --force-verify 0|1          default/env FORCE_VERIFY\n"
        "  --interval-sec N            default/env INTERVAL_SEC\n"
        "  --help\n"
        "\n"
        "Notes:\n"
        "  AI/OPi5/Flask never control actuators; event control_allowed must remain false.\n"
        "  This MVP calls v4l2-ctl and curl child processes.\n",
        prog);
}

static int parse_int(const char *text, int *out) {
    char *end = NULL;
    long v;
    errno = 0;
    v = strtol(text, &end, 10);
    if (errno != 0 || end == text || v < INT_MIN || v > INT_MAX) {
        return -1;
    }
    while (*end && isspace((unsigned char)*end)) {
        end++;
    }
    if (*end != '\0') {
        return -1;
    }
    *out = (int)v;
    return 0;
}

static void copy_string(char *dst, size_t size, const char *src) {
    if (size == 0) {
        return;
    }
    snprintf(dst, size, "%s", src ? src : "");
}

static void set_from_env_string(char *dst, size_t size, const char *env_name, const char *fallback) {
    const char *value = getenv(env_name);
    copy_string(dst, size, (value && value[0] != '\0') ? value : fallback);
}

static int set_from_env_int(const char *env_name, int fallback) {
    const char *value = getenv(env_name);
    int out = fallback;
    if (value && value[0] != '\0' && parse_int(value, &out) == 0) {
        return out;
    }
    return fallback;
}

static int load_config(AppConfig *cfg, int argc, char **argv) {
    memset(cfg, 0, sizeof(*cfg));
    set_from_env_string(cfg->opi5_url, sizeof(cfg->opi5_url), "OPI5_URL", DEFAULT_OPI5_URL);
    set_from_env_string(cfg->backend_url, sizeof(cfg->backend_url), "BACKEND_URL", DEFAULT_BACKEND_URL);
    set_from_env_string(cfg->device_id, sizeof(cfg->device_id), "DEVICE_ID", DEFAULT_DEVICE_ID);
    set_from_env_string(cfg->capture_device, sizeof(cfg->capture_device), "CAPTURE_DEVICE", DEFAULT_CAPTURE_DEVICE);
    set_from_env_string(cfg->base_dir, sizeof(cfg->base_dir), "BASE_DIR", DEFAULT_BASE_DIR);
    set_from_env_string(cfg->mode, sizeof(cfg->mode), "MODE", DEFAULT_MODE);
    cfg->gpio_pir = set_from_env_int("GPIO_PIR", DEFAULT_GPIO_PIR);
    cfg->gpio_active_high = set_from_env_int("GPIO_ACTIVE_HIGH", DEFAULT_GPIO_ACTIVE_HIGH);
    cfg->gpio_flame = set_from_env_int("GPIO_FLAME", DEFAULT_GPIO_FLAME);
    cfg->gpio_flame_active_high = set_from_env_int("GPIO_FLAME_ACTIVE_HIGH", DEFAULT_GPIO_FLAME_ACTIVE_HIGH);
    cfg->post_normal = set_from_env_int("POST_NORMAL", 0);
    cfg->force_verify = set_from_env_int("FORCE_VERIFY", 0);
    cfg->interval_sec = set_from_env_int("INTERVAL_SEC", DEFAULT_INTERVAL_SEC);

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            usage(argv[0]);
            exit(0);
        } else if (strcmp(argv[i], "--mode") == 0 && i + 1 < argc) {
            copy_string(cfg->mode, sizeof(cfg->mode), argv[++i]);
        } else if (strcmp(argv[i], "--opi5-url") == 0 && i + 1 < argc) {
            copy_string(cfg->opi5_url, sizeof(cfg->opi5_url), argv[++i]);
        } else if (strcmp(argv[i], "--backend-url") == 0 && i + 1 < argc) {
            copy_string(cfg->backend_url, sizeof(cfg->backend_url), argv[++i]);
        } else if (strcmp(argv[i], "--device-id") == 0 && i + 1 < argc) {
            copy_string(cfg->device_id, sizeof(cfg->device_id), argv[++i]);
        } else if (strcmp(argv[i], "--gpio-pir") == 0 && i + 1 < argc) {
            if (parse_int(argv[++i], &cfg->gpio_pir) != 0) {
                fprintf(stderr, "--gpio-pir requires integer\n");
                return -1;
            }
        } else if (strcmp(argv[i], "--gpio-active-high") == 0 && i + 1 < argc) {
            if (parse_int(argv[++i], &cfg->gpio_active_high) != 0) {
                fprintf(stderr, "--gpio-active-high requires 0|1\n");
                return -1;
            }
        } else if (strcmp(argv[i], "--gpio-flame") == 0 && i + 1 < argc) {
            if (parse_int(argv[++i], &cfg->gpio_flame) != 0) {
                fprintf(stderr, "--gpio-flame requires integer\n");
                return -1;
            }
        } else if (strcmp(argv[i], "--gpio-flame-active-high") == 0 && i + 1 < argc) {
            if (parse_int(argv[++i], &cfg->gpio_flame_active_high) != 0) {
                fprintf(stderr, "--gpio-flame-active-high requires 0|1\n");
                return -1;
            }
        } else if (strcmp(argv[i], "--capture-device") == 0 && i + 1 < argc) {
            copy_string(cfg->capture_device, sizeof(cfg->capture_device), argv[++i]);
        } else if (strcmp(argv[i], "--base-dir") == 0 && i + 1 < argc) {
            copy_string(cfg->base_dir, sizeof(cfg->base_dir), argv[++i]);
        } else if (strcmp(argv[i], "--post-normal") == 0 && i + 1 < argc) {
            if (parse_int(argv[++i], &cfg->post_normal) != 0) {
                fprintf(stderr, "--post-normal requires 0|1\n");
                return -1;
            }
        } else if (strcmp(argv[i], "--force-verify") == 0 && i + 1 < argc) {
            if (parse_int(argv[++i], &cfg->force_verify) != 0) {
                fprintf(stderr, "--force-verify requires 0|1\n");
                return -1;
            }
        } else if (strcmp(argv[i], "--interval-sec") == 0 && i + 1 < argc) {
            if (parse_int(argv[++i], &cfg->interval_sec) != 0) {
                fprintf(stderr, "--interval-sec requires integer\n");
                return -1;
            }
        } else {
            fprintf(stderr, "unknown or incomplete argument: %s\n", argv[i]);
            usage(argv[0]);
            return -1;
        }
    }

    if (strcmp(cfg->mode, "once") != 0 && strcmp(cfg->mode, "loop") != 0 && strcmp(cfg->mode, "flush") != 0) {
        fprintf(stderr, "--mode must be once, loop, or flush\n");
        return -1;
    }
    if (cfg->gpio_pir < 0 || (cfg->gpio_active_high != 0 && cfg->gpio_active_high != 1) ||
        cfg->gpio_flame < 0 || (cfg->gpio_flame_active_high != 0 && cfg->gpio_flame_active_high != 1) ||
        (cfg->post_normal != 0 && cfg->post_normal != 1) ||
        (cfg->force_verify != 0 && cfg->force_verify != 1) ||
        cfg->interval_sec < 1 || cfg->interval_sec > 3600) {
        fprintf(stderr, "invalid numeric option\n");
        return -1;
    }
    return 0;
}

static int path_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

static int mkdir_p(const char *path) {
    char tmp[PATH_MAX];
    size_t len;

    if (path == NULL || path[0] == '\0') {
        return -1;
    }
    copy_string(tmp, sizeof(tmp), path);
    len = strlen(tmp);
    if (len == 0) {
        return -1;
    }
    if (tmp[len - 1] == '/') {
        tmp[len - 1] = '\0';
    }
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
                return -1;
            }
            *p = '/';
        }
    }
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
        return -1;
    }
    return 0;
}

static int write_text_file(const char *path, const char *text) {
    FILE *fp = fopen(path, "w");
    if (fp == NULL) {
        return -1;
    }
    if (fputs(text, fp) == EOF) {
        fclose(fp);
        return -1;
    }
    return fclose(fp);
}

static int read_text_file_limited(const char *path, char *buf, size_t size) {
    FILE *fp;
    size_t n;
    if (size == 0) {
        return -1;
    }
    fp = fopen(path, "r");
    if (fp == NULL) {
        buf[0] = '\0';
        return -1;
    }
    n = fread(buf, 1, size - 1, fp);
    buf[n] = '\0';
    fclose(fp);
    return 0;
}

static int copy_file(const char *src, const char *dst) {
    FILE *in = fopen(src, "rb");
    if (in == NULL) {
        return -1;
    }
    FILE *out = fopen(dst, "wb");
    if (out == NULL) {
        fclose(in);
        return -1;
    }
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) {
            fclose(in);
            fclose(out);
            return -1;
        }
    }
    fclose(in);
    return fclose(out);
}

static int ensure_dirs(const AppConfig *cfg, AppPaths *paths) {
    memset(paths, 0, sizeof(*paths));
    snprintf(paths->capture_dir, sizeof(paths->capture_dir), "%s/captures/imx-safetyd", cfg->base_dir);
    snprintf(paths->spool_dir, sizeof(paths->spool_dir), "%s/spool/imx-safetyd", cfg->base_dir);
    snprintf(paths->pending_dir, sizeof(paths->pending_dir), "%s/pending", paths->spool_dir);
    snprintf(paths->sent_dir, sizeof(paths->sent_dir), "%s/sent", paths->spool_dir);
    snprintf(paths->run_dir, sizeof(paths->run_dir), "%s/run", cfg->base_dir);
    snprintf(paths->seq_file, sizeof(paths->seq_file), "%s/seq.txt", paths->spool_dir);
    snprintf(paths->log_file, sizeof(paths->log_file), "%s/imx_safetyd.log", paths->run_dir);
    snprintf(paths->status_file, sizeof(paths->status_file), "%s/imx_safetyd_status.json", paths->run_dir);

    if (mkdir_p(paths->capture_dir) != 0 || mkdir_p(paths->pending_dir) != 0 ||
        mkdir_p(paths->sent_dir) != 0 || mkdir_p(paths->run_dir) != 0) {
        fprintf(stderr, "mkdir under %s failed: %s\n", cfg->base_dir, strerror(errno));
        return -1;
    }

    copy_string(g_log_file, sizeof(g_log_file), paths->log_file);
    return 0;
}

static int read_int_file_default(const char *path, int fallback) {
    char buf[64];
    int v = fallback;
    if (read_text_file_limited(path, buf, sizeof(buf)) != 0) {
        return fallback;
    }
    if (parse_int(buf, &v) != 0) {
        return fallback;
    }
    return v;
}

static int next_seq(const AppPaths *paths) {
    int seq = read_int_file_default(paths->seq_file, INITIAL_SEQ);
    char buf[64];
    seq++;
    snprintf(buf, sizeof(buf), "%d\n", seq);
    if (write_text_file(paths->seq_file, buf) != 0) {
        log_msg("[seq] WARNING: failed to update %s: %s", paths->seq_file, strerror(errno));
    }
    return seq;
}

static int write_gpio_sysfs(int gpio, const char *name, const char *value) {
    char path[256];
    if (name == NULL) {
        snprintf(path, sizeof(path), "/sys/class/gpio/export");
    } else {
        snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/%s", gpio, name);
    }
    return write_text_file(path, value);
}

static int ensure_gpio_export(int gpio) {
    char gpio_dir[256];
    snprintf(gpio_dir, sizeof(gpio_dir), "/sys/class/gpio/gpio%d", gpio);
    if (!path_exists("/sys/class/gpio")) {
        return -1;
    }
    if (!path_exists(gpio_dir)) {
        char num[32];
        snprintf(num, sizeof(num), "%d", gpio);
        if (write_gpio_sysfs(gpio, NULL, num) != 0) {
            return -1;
        }
        usleep(100000);
    }
    if (path_exists(gpio_dir)) {
        (void)write_gpio_sysfs(gpio, "direction", "in");
        return 0;
    }
    return -1;
}

static int read_gpio_value(int gpio) {
    char path[256];
    char buf[32];
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", gpio);
    if (read_text_file_limited(path, buf, sizeof(buf)) != 0) {
        return -1;
    }
    if (buf[0] == '0') {
        return 0;
    }
    if (buf[0] == '1') {
        return 1;
    }
    return -1;
}

static void read_gpio(const AppConfig *cfg, SensorState *sensors, EventContext *ctx) {
    long long t0 = now_ms();
    sensors->door = 0;
    sensors->flame = 0;
    sensors->flame_raw = -1;
    sensors->mq2 = 0;
    sensors->raw_value = -1;
    sensors->pir = 0;

    /* PIR */
    if (ensure_gpio_export(cfg->gpio_pir) == 0) {
        sensors->raw_value = read_gpio_value(cfg->gpio_pir);
    }

    if (sensors->raw_value == 0 || sensors->raw_value == 1) {
        sensors->pir = cfg->gpio_active_high ? sensors->raw_value : !sensors->raw_value;
    } else {
        sensors->pir = 0;
        copy_string(ctx->last_error, sizeof(ctx->last_error), "pir_read_failed");
    }

    if (cfg->force_verify) {
        sensors->pir = 1;
    }

    /* Flame */
    if (ensure_gpio_export(cfg->gpio_flame) == 0) {
        sensors->flame_raw = read_gpio_value(cfg->gpio_flame);
    }

    if (sensors->flame_raw == 0 || sensors->flame_raw == 1) {
        sensors->flame = cfg->gpio_flame_active_high ? sensors->flame_raw : !sensors->flame_raw;
    } else {
        sensors->flame = 0;
        if (ctx->last_error[0] == '\0') {
            copy_string(ctx->last_error, sizeof(ctx->last_error), "flame_read_failed");
        }
    }

    ctx->gpio_ms = (int)(now_ms() - t0);
}

static void compute_state(const AppConfig *cfg, const SensorState *sensors, EventContext *ctx) {
    (void)cfg;
    int local_score = 0;

    /* Strong trigger: flame=1 → ALARM directly, independent of AI/network */
    if (sensors->flame) {
        local_score += 6;
    }
    if (sensors->pir) {
        local_score += 2;
    }
    /* door skipped for now (Task10-A deferred) */

    if (sensors->flame) {
        copy_string(ctx->state, sizeof(ctx->state), "ALARM");
        ctx->risk_score_before_ai = local_score;
        ctx->risk_score = local_score;
        ctx->need_snap = true;
    } else if (sensors->pir) {
        copy_string(ctx->state, sizeof(ctx->state), "VERIFY");
        ctx->risk_score_before_ai = local_score;
        ctx->risk_score = local_score;
        ctx->need_snap = true;
    } else if (sensors->raw_value < 0) {
        copy_string(ctx->state, sizeof(ctx->state), "FAULT");
        ctx->risk_score_before_ai = 1;
        ctx->risk_score = 1;
        ctx->need_snap = false;
    } else {
        copy_string(ctx->state, sizeof(ctx->state), "NORMAL");
        ctx->risk_score_before_ai = 0;
        ctx->risk_score = 0;
        ctx->need_snap = false;
    }
}

static void shell_quote(const char *src, char *dst, size_t size) {
    size_t pos = 0;
    if (size == 0) {
        return;
    }
    dst[pos++] = '\'';
    for (const char *p = src; *p && pos + 5 < size; p++) {
        if (*p == '\'') {
            const char *q = "'\\''";
            for (int i = 0; q[i] && pos + 1 < size; i++) {
                dst[pos++] = q[i];
            }
        } else {
            dst[pos++] = *p;
        }
    }
    if (pos + 1 < size) {
        dst[pos++] = '\'';
    }
    dst[pos] = '\0';
}

static int run_shell_command(const char *cmd) {
    int rc = system(cmd);
    if (rc == -1) {
        return -1;
    }
    if (WIFEXITED(rc)) {
        return WEXITSTATUS(rc);
    }
    return 128;
}

static void capture_frame(const AppConfig *cfg, const AppPaths *paths, EventContext *ctx) {
    long long t0 = now_ms();
    ctx->capture_ok = false;

    if (!ctx->need_snap) {
        ctx->capture_ms = 0;
        return;
    }

    char q_dev[160], q_out[640], cmd[2048];
    shell_quote(cfg->capture_device, q_dev, sizeof(q_dev));
    shell_quote(ctx->capture_file, q_out, sizeof(q_out));
    snprintf(cmd, sizeof(cmd),
             "v4l2-ctl -d %s --set-fmt-video=width=640,height=480,pixelformat=MJPG "
             "--stream-mmap=3 --stream-count=1 --stream-to=%s >/dev/null 2>&1",
             q_dev, q_out);

    log_msg("[capture] %s -> %s", cfg->capture_device, ctx->capture_file);
    if (run_shell_command(cmd) == 0 && path_exists(ctx->capture_file)) {
        ctx->capture_ok = true;
    } else {
        char fallback[512];
        snprintf(fallback, sizeof(fallback), "%s/captures/task04_latest.jpg", cfg->base_dir);
        if (path_exists(fallback) && copy_file(fallback, ctx->capture_file) == 0) {
            ctx->capture_ok = true;
            log_msg("[capture] using fallback image %s", fallback);
        } else {
            copy_string(ctx->last_error, sizeof(ctx->last_error), "capture_failed");
            log_msg("[capture] failed and no fallback image is available");
        }
    }

    (void)paths;
    ctx->capture_ms = (int)(now_ms() - t0);
}

static void write_fallback_ai(const char *out_file, const char *error) {
    FILE *fp = fopen(out_file, "w");
    if (fp == NULL) {
        return;
    }
    fprintf(fp,
            "{\n"
            "  \"ok\": false,\n"
            "  \"contract_version\": \"1.0\",\n"
            "  \"model_ready\": false,\n"
            "  \"mode\": \"offline\",\n"
            "  \"models\": [],\n"
            "  \"objects\": [],\n"
            "  \"faces\": [],\n"
            "  \"risk_hint\": 0,\n"
            "  \"summary\": \"OPi5 AI service unavailable; local safety logic only\",\n"
            "  \"action_hint\": \"local_only\",\n"
            "  \"control_allowed\": false,\n"
            "  \"error\": \"%s\"\n"
            "}\n",
            error ? error : "opi5_unreachable");
    fclose(fp);
}

static int extract_risk_hint(const char *json) {
    const char *p = strstr(json, "\"risk_hint\"");
    if (p == NULL) {
        return 0;
    }
    p = strchr(p, ':');
    if (p == NULL) {
        return 0;
    }
    p++;
    while (*p && isspace((unsigned char)*p)) {
        p++;
    }
    int v = atoi(p);
    if (v < 0) {
        return 0;
    }
    if (v > 10) {
        return 10;
    }
    return v;
}

static bool has_control_allowed_false(const char *json) {
    const char *p = strstr(json, "\"control_allowed\"");
    if (p == NULL) {
        return false;
    }
    p = strchr(p, ':');
    if (p == NULL) {
        return false;
    }
    p++;
    while (*p && isspace((unsigned char)*p)) {
        p++;
    }
    return strncmp(p, "false", 5) == 0;
}

static void build_metadata_json(const AppConfig *cfg, const SensorState *sensors, const EventContext *ctx) {
    char ts[32];
    iso_time_utc(ts, sizeof(ts));
    FILE *fp = fopen(ctx->metadata_file, "w");
    if (fp == NULL) {
        return;
    }
    fprintf(fp,
            "{\n"
            "  \"contract_version\": \"1.0\",\n"
            "  \"device_id\": \"%s\",\n"
            "  \"frame_id\": %d,\n"
            "  \"timestamp\": \"%s\",\n"
            "  \"source\": \"imx_safetyd_c\",\n"
            "  \"image_format\": \"jpeg\",\n"
            "  \"resolution\": {\"width\": 640, \"height\": 480},\n"
            "  \"sensors\": {\"door\": %d, \"pir\": %d, \"flame\": %d, \"mq2\": %d},\n"
            "  \"state_before_ai\": \"%s\",\n"
            "  \"risk_score_before_ai\": %d,\n"
            "  \"pan_tilt\": {\"pan_deg\": 90, \"tilt_deg\": 90}\n"
            "}\n",
            cfg->device_id, ctx->seq, ts, sensors->door, sensors->pir, sensors->flame, sensors->mq2,
            ctx->state, ctx->risk_score_before_ai);
    fclose(fp);
}

static int read_http_code_file(const char *path) {
    char buf[32];
    if (read_text_file_limited(path, buf, sizeof(buf)) != 0) {
        return 0;
    }
    return atoi(buf);
}

static void post_opi5_ai(const AppConfig *cfg, const SensorState *sensors, EventContext *ctx) {
    long long t0 = now_ms();
    char ai_buf[65536];
    int ai_code = 0;

    copy_string(ctx->ai_status, sizeof(ctx->ai_status), "skipped");
    ctx->ai_ms = 0;

    (void)unlink(ctx->ai_file);
    (void)unlink(ctx->ai_http_file);

    if (!ctx->need_snap) {
        return;
    }

    if (!ctx->capture_ok || !path_exists(ctx->capture_file)) {
        write_fallback_ai(ctx->ai_file, "capture_unavailable");
        copy_string(ctx->ai_status, sizeof(ctx->ai_status), "offline");
        /* Keep local risk_score; AI offline should not lower local evidence */
        ctx->ai_ms = (int)(now_ms() - t0);
        return;
    }

    build_metadata_json(cfg, sensors, ctx);

    char image_part[700], meta_part[700], url[320];
    char q_image[900], q_meta[900], q_url[400], q_ai[700], q_http[700];
    char cmd[4096];

    snprintf(image_part, sizeof(image_part), "image=@%s", ctx->capture_file);
    snprintf(meta_part, sizeof(meta_part), "metadata=<%s", ctx->metadata_file);
    snprintf(url, sizeof(url), "%s/api/infer/vision", cfg->opi5_url);
    shell_quote(image_part, q_image, sizeof(q_image));
    shell_quote(meta_part, q_meta, sizeof(q_meta));
    shell_quote(url, q_url, sizeof(q_url));
    shell_quote(ctx->ai_file, q_ai, sizeof(q_ai));
    shell_quote(ctx->ai_http_file, q_http, sizeof(q_http));

    snprintf(cmd, sizeof(cmd),
             "curl -sS -X POST -F %s -F %s %s -o %s -w '%%{http_code}' "
             "--connect-timeout 5 --max-time 10 > %s 2>/dev/null",
             q_image, q_meta, q_url, q_ai, q_http);

    log_msg("[ai] POST %s/api/infer/vision", cfg->opi5_url);
    if (run_shell_command(cmd) != 0) {
        ai_code = 0;
    } else {
        ai_code = read_http_code_file(ctx->ai_http_file);
    }
    ctx->ai_ms = (int)(now_ms() - t0);

    if (ai_code == 200 && read_text_file_limited(ctx->ai_file, ai_buf, sizeof(ai_buf)) == 0 &&
        has_control_allowed_false(ai_buf)) {
        int risk_hint = extract_risk_hint(ai_buf);
        copy_string(ctx->ai_status, sizeof(ctx->ai_status), "ok");
        ctx->risk_score = ctx->risk_score_before_ai + risk_hint;
        if (ctx->risk_score > 10) {
            ctx->risk_score = 10;
        }
        log_msg("[ai] HTTP 200, risk_hint=%d, control_allowed=false", risk_hint);
    } else {
        const char *reason = (ai_code == 200) ? "opi5_protocol_error" : "opi5_unreachable";
        write_fallback_ai(ctx->ai_file, reason);
        copy_string(ctx->ai_status, sizeof(ctx->ai_status), (ai_code == 200) ? "protocol_error" : "offline");
        /* Keep local risk_score; AI offline should not lower local evidence */
        log_msg("[ai] degraded: HTTP %03d, ai_status=%s", ai_code, ctx->ai_status);
    }
}

static void build_event_json(const AppConfig *cfg, const SensorState *sensors, const EventContext *ctx,
                             const char *backend_status, const char *spool_path, int post_ms, int total_ms,
                             const char *out_file) {
    char ai_buf[65536];
    const char *ai_json = "{}";
    if (ctx->need_snap && read_text_file_limited(ctx->ai_file, ai_buf, sizeof(ai_buf)) == 0) {
        ai_json = ai_buf;
    }

    int rgb_b = strcmp(ctx->state, "VERIFY") == 0 ? 1 : 0;
    FILE *fp = fopen(out_file, "w");
    if (fp == NULL) {
        return;
    }
    fprintf(fp,
            "{\n"
            "  \"type\": \"event\",\n"
            "  \"contract_version\": \"1.0\",\n"
            "  \"device_id\": \"%s\",\n"
            "  \"seq\": %d,\n"
            "  \"state\": \"%s\",\n"
            "  \"risk_score\": %d,\n"
            "  \"need_snap\": %s,\n"
            "  \"sensors\": {\"door\": %d, \"pir\": %d, \"flame\": %d, \"mq2\": %d},\n"
            "  \"actuators\": {\"relay\": 0, \"fan\": 0, \"pump\": 0, \"buzzer\": 0, \"rgb_r\": 0, \"rgb_g\": 0, \"rgb_b\": %d},\n"
            "  \"vision\": {\n"
            "    \"frame_id\": %d,\n"
            "    \"image_url\": null,\n"
            "    \"capture_ok\": %s,\n"
            "    \"pan_tilt\": {\"pan_deg\": 90, \"tilt_deg\": 90}\n"
            "  },\n"
            "  \"ai_result\": %s,\n"
            "  \"latency_ms\": {\"gpio\": %d, \"capture\": %d, \"ai\": %d, \"post\": %d, \"total\": %d},\n"
            "  \"diagnostics\": {\n"
            "    \"program\": \"imx_safetyd_c\",\n"
            "    \"gpio\": {\"pir_gpio\": %d, \"raw_value\": %d, \"active_high\": %s, \"flame_gpio\": %d, \"flame_raw\": %d, \"flame_active_high\": %s},\n"
            "    \"ai_status\": \"%s\",\n"
            "    \"backend_status\": \"%s\",\n"
            "    \"spool_path\": %s,\n"
            "    \"force_verify\": %s\n"
            "  }\n"
            "}\n",
            cfg->device_id, ctx->seq, ctx->state, ctx->risk_score, ctx->need_snap ? "true" : "false",
            sensors->door, sensors->pir, sensors->flame, sensors->mq2, rgb_b, ctx->seq,
            ctx->capture_ok ? "true" : "false", ai_json, ctx->gpio_ms, ctx->capture_ms, ctx->ai_ms,
            post_ms, total_ms, cfg->gpio_pir, sensors->raw_value, cfg->gpio_active_high ? "true" : "false",
            cfg->gpio_flame, sensors->flame_raw, cfg->gpio_flame_active_high ? "true" : "false",
            ctx->ai_status, backend_status, spool_path ? spool_path : "null", cfg->force_verify ? "true" : "false");
    fclose(fp);
}

static int post_backend(const AppConfig *cfg, const EventContext *ctx) {
    char url[320], q_url[400], q_event[700], q_resp[700], q_http[700], cmd[4096];
    snprintf(url, sizeof(url), "%s/api/events", cfg->backend_url);
    shell_quote(url, q_url, sizeof(q_url));
    shell_quote(ctx->event_file, q_event, sizeof(q_event));
    shell_quote(ctx->backend_response_file, q_resp, sizeof(q_resp));
    shell_quote(ctx->backend_http_file, q_http, sizeof(q_http));
    (void)unlink(ctx->backend_response_file);
    (void)unlink(ctx->backend_http_file);

    snprintf(cmd, sizeof(cmd),
             "curl -sS -X POST %s -H 'Content-Type: application/json' -d @%s -o %s "
             "-w '%%{http_code}' --connect-timeout 5 --max-time 10 > %s 2>/dev/null",
             q_url, q_event, q_resp, q_http);
    if (run_shell_command(cmd) != 0) {
        return 0;
    }
    return read_http_code_file(ctx->backend_http_file);
}

static int spool_event(const AppPaths *paths, EventContext *ctx) {
    char response_dst[512];
    snprintf(ctx->spool_path, sizeof(ctx->spool_path), "%s/event_%d.json", paths->pending_dir, ctx->seq);
    snprintf(response_dst, sizeof(response_dst), "%s/event_%d.response.txt", paths->pending_dir, ctx->seq);
    if (copy_file(ctx->event_file, ctx->spool_path) != 0) {
        return -1;
    }
    if (path_exists(ctx->backend_response_file)) {
        (void)copy_file(ctx->backend_response_file, response_dst);
    }
    return 0;
}

static void write_status_json(const AppPaths *paths, const EventContext *ctx, const SensorState *sensors) {
    char ts[32];
    iso_time_utc(ts, sizeof(ts));
    FILE *fp = fopen(paths->status_file, "w");
    if (fp == NULL) {
        return;
    }
    fprintf(fp,
            "{\n"
            "  \"program\": \"imx_safetyd_c\",\n"
            "  \"last_seq\": %d,\n"
            "  \"last_state\": \"%s\",\n"
            "  \"last_pir\": %d,\n"
            "  \"last_flame\": %d,\n"
            "  \"last_risk_score\": %d,\n"
            "  \"last_ai_status\": \"%s\",\n"
            "  \"last_backend_status\": \"%s\",\n"
            "  \"last_error\": ",
            ctx->seq, ctx->state[0] ? ctx->state : "NONE", sensors ? sensors->pir : 0, sensors ? sensors->flame : 0, ctx->risk_score,
            ctx->ai_status[0] ? ctx->ai_status : "skipped",
            ctx->backend_status[0] ? ctx->backend_status : "none");
    if (ctx->last_error[0]) {
        fprintf(fp, "\"%s\"", ctx->last_error);
    } else {
        fprintf(fp, "null");
    }
    fprintf(fp,
            ",\n"
            "  \"updated_at\": \"%s\"\n"
            "}\n",
            ts);
    fclose(fp);
}

static int run_once(const AppConfig *cfg, const AppPaths *paths) {
    long long t_start = now_ms();
    SensorState sensors;
    EventContext ctx;
    int backend_code;

    memset(&ctx, 0, sizeof(ctx));
    copy_string(ctx.ai_status, sizeof(ctx.ai_status), "skipped");
    copy_string(ctx.backend_status, sizeof(ctx.backend_status), "skipped");

    read_gpio(cfg, &sensors, &ctx);
    compute_state(cfg, &sensors, &ctx);
    log_msg("[gpio] gpio%d raw=%d pir=%d active_high=%d force_verify=%d | gpio%d flame_raw=%d flame=%d flame_ah=%d",
            cfg->gpio_pir, sensors.raw_value, sensors.pir, cfg->gpio_active_high, cfg->force_verify,
            cfg->gpio_flame, sensors.flame_raw, sensors.flame, cfg->gpio_flame_active_high);
    log_msg("[state] %s risk_before_ai=%d need_snap=%s",
            ctx.state, ctx.risk_score_before_ai, ctx.need_snap ? "true" : "false");

    if (strcmp(ctx.state, "NORMAL") == 0 && cfg->post_normal == 0) {
        copy_string(ctx.backend_status, sizeof(ctx.backend_status), "skipped");
        ctx.total_ms = (int)(now_ms() - t_start);
        write_status_json(paths, &ctx, &sensors);
        log_msg("[skip] NORMAL and POST_NORMAL=0");
        return 0;
    }

    ctx.seq = next_seq(paths);
    snprintf(ctx.capture_file, sizeof(ctx.capture_file), "%s/safetyd_%d.jpg", paths->capture_dir, ctx.seq);
    snprintf(ctx.metadata_file, sizeof(ctx.metadata_file), "%s/meta_%d.json", paths->spool_dir, ctx.seq);
    snprintf(ctx.ai_file, sizeof(ctx.ai_file), "%s/ai_%d.json", paths->spool_dir, ctx.seq);
    snprintf(ctx.ai_http_file, sizeof(ctx.ai_http_file), "%s/ai_%d.http", paths->spool_dir, ctx.seq);
    snprintf(ctx.event_file, sizeof(ctx.event_file), "%s/event_%d.json", paths->spool_dir, ctx.seq);
    snprintf(ctx.backend_response_file, sizeof(ctx.backend_response_file), "%s/backend_%d.response.txt", paths->spool_dir, ctx.seq);
    snprintf(ctx.backend_http_file, sizeof(ctx.backend_http_file), "%s/backend_%d.http", paths->spool_dir, ctx.seq);

    capture_frame(cfg, paths, &ctx);
    post_opi5_ai(cfg, &sensors, &ctx);

    build_event_json(cfg, &sensors, &ctx, "posted", "null", 0, 0, ctx.event_file);

    long long t_post = now_ms();
    backend_code = post_backend(cfg, &ctx);
    ctx.post_ms = (int)(now_ms() - t_post);
    ctx.total_ms = (int)(now_ms() - t_start);

    if (backend_code == 201) {
        copy_string(ctx.backend_status, sizeof(ctx.backend_status), "posted");
        log_msg("[flask] HTTP 201 posted");
        build_event_json(cfg, &sensors, &ctx, "posted", "null", ctx.post_ms, ctx.total_ms, ctx.event_file);
    } else {
        copy_string(ctx.backend_status, sizeof(ctx.backend_status), "spooled");
        snprintf(ctx.spool_path, sizeof(ctx.spool_path), "\"%s/event_%d.json\"", paths->pending_dir, ctx.seq);
        build_event_json(cfg, &sensors, &ctx, "spooled", ctx.spool_path, ctx.post_ms, ctx.total_ms, ctx.event_file);
        if (spool_event(paths, &ctx) != 0) {
            copy_string(ctx.last_error, sizeof(ctx.last_error), "spool_write_failed");
        }
        log_msg("[flask] HTTP %03d spooled to %s/event_%d.json", backend_code, paths->pending_dir, ctx.seq);
    }

    write_status_json(paths, &ctx, &sensors);

    log_msg("[summary] seq=%d state=%s risk_score=%d ai_status=%s backend_status=%s gpio=%d capture=%d ai=%d post=%d total=%d",
            ctx.seq, ctx.state, ctx.risk_score, ctx.ai_status, ctx.backend_status, ctx.gpio_ms,
            ctx.capture_ms, ctx.ai_ms, ctx.post_ms, ctx.total_ms);
    return 0;
}

static int has_json_suffix(const char *name) {
    size_t len = strlen(name);
    return len > 5 && strcmp(name + len - 5, ".json") == 0;
}

static int flush_spool(const AppConfig *cfg, const AppPaths *paths) {
    DIR *dir = opendir(paths->pending_dir);
    struct dirent *ent;
    int total = 0;
    int sent = 0;
    EventContext status_ctx;
    SensorState sensors;

    memset(&status_ctx, 0, sizeof(status_ctx));
    memset(&sensors, 0, sizeof(sensors));
    copy_string(status_ctx.state, sizeof(status_ctx.state), "FLUSH");
    copy_string(status_ctx.ai_status, sizeof(status_ctx.ai_status), "skipped");
    copy_string(status_ctx.backend_status, sizeof(status_ctx.backend_status), "none");

    if (dir == NULL) {
        log_msg("[flush] open %s failed: %s", paths->pending_dir, strerror(errno));
        copy_string(status_ctx.last_error, sizeof(status_ctx.last_error), "pending_dir_open_failed");
        write_status_json(paths, &status_ctx, &sensors);
        return 1;
    }

    while ((ent = readdir(dir)) != NULL) {
        if (strncmp(ent->d_name, "event_", 6) != 0 || !has_json_suffix(ent->d_name)) {
            continue;
        }
        total++;
        char stem[256];
        copy_string(stem, sizeof(stem), ent->d_name);
        char *dot = strrchr(stem, '.');
        if (dot != NULL) {
            *dot = '\0';
        }
        snprintf(status_ctx.event_file, sizeof(status_ctx.event_file), "%s/%s", paths->pending_dir, ent->d_name);
        snprintf(status_ctx.backend_response_file, sizeof(status_ctx.backend_response_file), "%s/%s.response.txt",
                 paths->pending_dir, stem);
        snprintf(status_ctx.backend_http_file, sizeof(status_ctx.backend_http_file), "%s/%s.http",
                 paths->pending_dir, stem);
        int code = post_backend(cfg, &status_ctx);
        if (code == 201) {
            char sent_event[512], sent_resp[512], sent_http[512];
            snprintf(sent_event, sizeof(sent_event), "%s/%s", paths->sent_dir, ent->d_name);
            snprintf(sent_resp, sizeof(sent_resp), "%s/%s.response.txt", paths->sent_dir, stem);
            snprintf(sent_http, sizeof(sent_http), "%s/%s.http", paths->sent_dir, stem);
            if (rename(status_ctx.event_file, sent_event) == 0) {
                if (path_exists(status_ctx.backend_response_file)) {
                    (void)rename(status_ctx.backend_response_file, sent_resp);
                }
                if (path_exists(status_ctx.backend_http_file)) {
                    (void)rename(status_ctx.backend_http_file, sent_http);
                }
                sent++;
                log_msg("[flush] %s -> sent (HTTP 201)", ent->d_name);
            } else {
                log_msg("[flush] %s posted but move failed: %s", ent->d_name, strerror(errno));
            }
        } else {
            log_msg("[flush] %s kept pending (HTTP %03d)", ent->d_name, code);
        }
    }
    closedir(dir);

    copy_string(status_ctx.backend_status, sizeof(status_ctx.backend_status), sent == total ? "flushed" : "partial");
    status_ctx.risk_score = 0;
    write_status_json(paths, &status_ctx, &sensors);
    log_msg("[flush] done: %d/%d sent", sent, total);
    return sent == total ? 0 : 1;
}

static int run_loop(const AppConfig *cfg, const AppPaths *paths) {
    log_msg("[loop] start interval_sec=%d", cfg->interval_sec);
    while (keep_running) {
        (void)run_once(cfg, paths);
        if (!keep_running) {
            break;
        }
        sleep_seconds_interruptible(cfg->interval_sec);
    }
    log_msg("[loop] stopped");
    return 0;
}

int main(int argc, char **argv) {
    AppConfig cfg;
    AppPaths paths;

    if (load_config(&cfg, argc, argv) != 0) {
        return 2;
    }
    if (ensure_dirs(&cfg, &paths) != 0) {
        return 1;
    }

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    log_msg("[start] mode=%s device_id=%s base_dir=%s", cfg.mode, cfg.device_id, cfg.base_dir);
    if (strcmp(cfg.mode, "flush") == 0) {
        return flush_spool(&cfg, &paths);
    }
    if (strcmp(cfg.mode, "loop") == 0) {
        return run_loop(&cfg, &paths);
    }
    return run_once(&cfg, &paths);
}
