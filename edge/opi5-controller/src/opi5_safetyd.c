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
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include "bsp_oled_ssd1306.h"

#define PROGRAM_NAME "opi5_safetyd_c"
#define CONTRACT_VERSION "1.0"
#define DEFAULT_DEVICE_ID "labbox_001"
/* OPi5 RK3588S GPIO defaults — 2026-06-09 实测确认
 * Override via env vars or CLI args. */
#define DEFAULT_GPIO_PIR 139            /* pin 13, active HIGH, 已验证 */
#define DEFAULT_GPIO_ACTIVE_HIGH 1
#define DEFAULT_GPIO_FLAME 28           /* pin 15, active LOW, 已验证 */
#define DEFAULT_GPIO_FLAME_ACTIVE_HIGH 0
#define DEFAULT_GPIO_MQ2 49             /* pin 19, active LOW, 已验证 (3.3V+调灵敏度) */
#define DEFAULT_GPIO_MQ2_ACTIVE_HIGH 0
#define DEFAULT_GPIO_RGB_R 48           /* pin 21, active HIGH, 已验证 */
#define DEFAULT_GPIO_RGB_R_ACTIVE_HIGH 1
#define DEFAULT_GPIO_BUZZER 35          /* pin 26, active LOW, 已验证 */
#define DEFAULT_GPIO_BUZZER_ACTIVE_HIGH 0
#define DEFAULT_GPIO_RGB_G 50           /* pin 23, active HIGH, 已验证 */
#define DEFAULT_GPIO_RGB_G_ACTIVE_HIGH 1
#define DEFAULT_GPIO_RGB_B 52           /* pin 24, active HIGH, 已验证 */
#define DEFAULT_GPIO_RGB_B_ACTIVE_HIGH 1
#define DEFAULT_RGB_BACKEND "gpio"      /* PCA9685 不可用，GPIO 直控 */
#define DEFAULT_PCA9685_BUS "/dev/i2c-1"
#define DEFAULT_PCA9685_ADDR 0x40
#define DEFAULT_PCA9685_RGB_R_CH 2
#define DEFAULT_PCA9685_RGB_G_CH 3
#define DEFAULT_PCA9685_RGB_B_CH 4
#define DEFAULT_PCA9685_RGB_ON_DUTY 4095
#define DEFAULT_PCA9685_RGB_OFF_DUTY 0
/* Water gun via MOS on pin 11 (GPIO138), active HIGH, 已验证 */
#define DEFAULT_GPIO_WATER_GUN 138
#define DEFAULT_GPIO_WATER_GUN_ACTIVE_HIGH 1
/* Servo via OPi5 hardware PWM (pin 7 = PWM15, pwmchip4), 已验证 */
#define DEFAULT_SERVO_PWM_CHIP 4
#define DEFAULT_SERVO_PWM_CHANNEL 0
#define DEFAULT_SERVO_PERIOD_NS 20000000
#define DEFAULT_SERVO_PULSE_MIN_NS 500000
#define DEFAULT_SERVO_PULSE_CENTER_NS 1500000
#define DEFAULT_SERVO_PULSE_MAX_NS 2500000
#define DEFAULT_CAPTURE_DEVICE "/dev/video0"
#define DEFAULT_BASE_DIR "/opt/edge-ai-safety-monitor"
#define DEFAULT_INTERVAL_SEC 2
#define DEFAULT_MODE "once"
#define DEFAULT_OLED_ENABLE 0
#define DEFAULT_OLED_BUS "/dev/i2c-1"    /* I2C1 pin 16/18, 已验证 */
#define DEFAULT_OLED_ADDR 0x3C
#define DEFAULT_RELAY_ENABLE 0
#define DEFAULT_PCA9685_RELAY_CH 5
#define DEFAULT_PCA9685_RELAY_ACTIVE_HIGH 1
#define DEFAULT_SOC_TEMP_ENABLE 1
#define DEFAULT_SOC_TEMP_PATH "/sys/class/thermal/thermal_zone0/temp"
#define DEFAULT_SOC_TEMP_WARN_C 70
#define DEFAULT_PUMP_ENABLE 0
#define DEFAULT_PCA9685_PUMP_CH 6
#define DEFAULT_PCA9685_PUMP_ACTIVE_HIGH 1
#define DEFAULT_BACKEND_URL "http://127.0.0.1:5000"
#define DEFAULT_OPI5_URL "http://127.0.0.1:8080"
#define INITIAL_SEQ 9000

/* Patrol (servo sweep) defaults */
#define DEFAULT_PATROL_ENABLE 0
#define DEFAULT_PATROL_INTERVAL_SEC 30
#define DEFAULT_PATROL_ANGLES_STR "60,90,120"
#define DEFAULT_PATROL_SETTLE_MS 500
#define DEFAULT_PATROL_SETTLE_CAPTURE_MS 2000
#define DEFAULT_PATROL_AI_ENABLE 1
#define DEFAULT_PATROL_POST_EVENT 1
#define DEFAULT_PATROL_MAX_ANGLES 16
#define DEFAULT_PATROL_AI_TIMEOUT_SEC 10

/* Water gun on flame (reuse SPRAY_MAX_MS / SPRAY_COOLDOWN_MS) */
#define DEFAULT_WATER_GUN_ON_FLAME_ENABLE 0

/* Spray double-confirmation defaults */
#define DEFAULT_SPRAY_CONFIRM_ENABLE 0
#define DEFAULT_SPRAY_REQUIRE_VISION 1
#define DEFAULT_SPRAY_MIN_AI_SCORE 0.50
#define DEFAULT_VISUAL_STATE_PATH "/run/opi5_visual_state.json"
#define DEFAULT_VISUAL_CONFIRM_WINDOW_MS 1500
#define DEFAULT_VISION_OFFLINE_AUTOSPRAY 1
#define DEFAULT_SPRAY_MAX_MS 3000
#define DEFAULT_SPRAY_COOLDOWN_MS 2000
#define DEFAULT_AI_CONNECT_TIMEOUT_SEC 5
#define DEFAULT_AI_MAX_TIME_SEC 20
#define DEFAULT_PCA9685_LOCK_PATH "/run/edge-ai-safety-monitor/pca9685.lock"

typedef struct {
    char opi5_url[256];
    char backend_url[256];
    char device_id[64];
    int gpio_pir;
    int gpio_active_high;
    int gpio_flame;
    int gpio_flame_active_high;
    int gpio_mq2;
    int gpio_mq2_active_high;
    int gpio_rgb_r;
    int gpio_rgb_r_active_high;
    int gpio_buzzer;
    int gpio_buzzer_active_high;
    int gpio_rgb_g;
    int gpio_rgb_g_active_high;
    int gpio_rgb_b;
    int gpio_rgb_b_active_high;
    char rgb_backend[16];
    char pca9685_bus[64];
    int pca9685_addr;
    int pca9685_rgb_r_ch;
    int pca9685_rgb_g_ch;
    int pca9685_rgb_b_ch;
    int pca9685_rgb_on_duty;
    int pca9685_rgb_off_duty;
    char capture_device[64];
    char base_dir[256];
    int post_normal;
    int force_verify;
    int interval_sec;
    char mode[16];
    int oled_enable;
    char oled_bus[64];
    int oled_addr;
    int relay_enable;
    int pca9685_relay_ch;
    int pca9685_relay_active_high;
    int soc_temp_enable;
    char soc_temp_path[256];
    int soc_temp_warn_c;
    int pump_enable;
    int pca9685_pump_ch;
    int pca9685_pump_active_high;
    /* Spray double-confirmation */
    int spray_confirm_enable;
    int spray_require_vision;
    double spray_min_ai_score;
    char visual_state_path[256];
    int visual_confirm_window_ms;
    int vision_offline_autospray;
    int spray_max_ms;
    int spray_cooldown_ms;
    /* AI timeout config */
    int ai_connect_timeout_sec;
    int ai_max_time_sec;
    /* PCA9685 lock path */
    char pca9685_lock_path[256];
    /* Servo patrol */
    int patrol_enable;
    int patrol_interval_sec;
    int patrol_angles[DEFAULT_PATROL_MAX_ANGLES];
    int patrol_angle_count;
    int patrol_settle_ms;
    int patrol_settle_capture_ms;
    int patrol_ai_enable;
    int patrol_post_event;
    int patrol_ai_timeout_sec;
    /* Water gun on flame */
    int water_gun_on_flame_enable;
} AppConfig;

typedef struct {
    int raw_value;
    int pir;
    int door;
    int flame;
    int flame_raw;
    int mq2;
    int mq2_raw;
    double soc_temp_c;
    int soc_temp_valid;
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
    char device_health[16];
    int gpio_ms;
    int capture_ms;
    int ai_ms;
    int post_ms;
    int total_ms;
    /* Spray double-confirmation */
    char alarm_phase[16];
    int visual_fire_confirmed;
    double visual_fire_score;
    char visual_fire_label[32];
    long long visual_state_ts_ms;
    /* Actuator snapshot (actual physical state after spray gate) */
    int actuator_buzzer;
    int actuator_rgb_r;
    int actuator_rgb_g;
    int actuator_rgb_b;
    int actuator_relay;
    int actuator_pump;
    /* Spray decision snapshot (matches actual pump state) */
    char spray_decision[32];
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
static AppConfig g_cfg;
static volatile sig_atomic_t g_cfg_ready = 0;

/* Spray timing state (Hotfix D) */
static long long g_last_spray_start_ms = 0;
static long long g_last_spray_stop_ms = 0;
static int g_spray_active = 0;

/* Water gun on-flame timing (independent of spray state machine) */
static long long g_wg_last_trigger_ms = 0;
static long long g_wg_last_stop_ms = 0;
static int g_wg_active = 0;

/* Patrol state */
static long long g_last_patrol_ms = 0;
static int g_patrol_seq = 0;

static void all_off(const AppConfig *cfg);
static void shell_quote(const char *src, char *dst, size_t size);
static int run_shell_command(const char *cmd);
static void read_visual_state(const AppConfig *cfg, EventContext *ctx);
static void determine_alarm_phase(const AppConfig *cfg, const SensorState *sensors,
                                   const EventContext *ctx, char *phase, size_t phase_size);
static int should_pump_activate(const AppConfig *cfg, const SensorState *sensors,
                                 EventContext *ctx);
static long long extract_json_ll(const char *json, const char *key);
static void extract_json_string(const char *json, const char *key, char *out, size_t size);
/* Patrol */
static void patrol_round(const AppConfig *cfg, const AppPaths *paths);
/* PCA9685 I2C lock (Hotfix E) */
static int pca9685_lock_acquire(const char *lock_path);
static void pca9685_lock_release(int fd);

static void handle_signal(int signum) {
    (void)signum;
    keep_running = 0;
    if (g_cfg_ready) {
        all_off(&g_cfg);
    }
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
        "  --gpio-mq2 N                default/env GPIO_MQ2\n"
        "  --gpio-mq2-active-high 0|1  default/env GPIO_MQ2_ACTIVE_HIGH\n"
        "  --gpio-rgb-r N              default/env GPIO_RGB_R\n"
        "  --gpio-rgb-r-active-high 0|1 default/env GPIO_RGB_R_ACTIVE_HIGH\n"
        "  --gpio-buzzer N             default/env GPIO_BUZZER\n"
        "  --gpio-buzzer-active-high 0|1 default/env GPIO_BUZZER_ACTIVE_HIGH\n"
        "  --gpio-rgb-g N              default/env GPIO_RGB_G\n"
        "  --gpio-rgb-g-active-high 0|1 default/env GPIO_RGB_G_ACTIVE_HIGH\n"
        "  --gpio-rgb-b N              default/env GPIO_RGB_B\n"
        "  --gpio-rgb-b-active-high 0|1 default/env GPIO_RGB_B_ACTIVE_HIGH\n"
        "  --rgb-backend gpio|pca9685  default/env RGB_BACKEND\n"
        "  --pca9685-bus PATH          default/env PCA9685_BUS\n"
        "  --pca9685-addr HEX          default/env PCA9685_ADDR\n"
        "  --pca9685-rgb-r-ch N        default/env PCA9685_RGB_R_CH\n"
        "  --pca9685-rgb-g-ch N        default/env PCA9685_RGB_G_CH\n"
        "  --pca9685-rgb-b-ch N        default/env PCA9685_RGB_B_CH\n"
        "  --pca9685-rgb-on-duty N     default/env PCA9685_RGB_ON_DUTY\n"
        "  --pca9685-rgb-off-duty N    default/env PCA9685_RGB_OFF_DUTY\n"
        "  --capture-device PATH       default/env CAPTURE_DEVICE\n"
        "  --base-dir PATH             default/env BASE_DIR\n"
        "  --post-normal 0|1           default/env POST_NORMAL\n"
        "  --force-verify 0|1          default/env FORCE_VERIFY\n"
        "  --interval-sec N            default/env INTERVAL_SEC\n"
        "  --oled-enable 0|1           default/env OLED_ENABLE\n"
        "  --oled-bus PATH             default/env OLED_BUS\n"
        "  --oled-addr HEX             default/env OLED_ADDR\n"
        "  --relay-enable 0|1          default/env RELAY_ENABLE\n"
        "  --pca9685-relay-ch N        default/env PCA9685_RELAY_CH\n"
        "  --pca9685-relay-active-high 0|1 default/env PCA9685_RELAY_ACTIVE_HIGH\n"
        "  --soc-temp-enable 0|1       default/env SOC_TEMP_ENABLE\n"
        "  --soc-temp-path PATH        default/env SOC_TEMP_PATH\n"
        "  --soc-temp-warn-c N         default/env SOC_TEMP_WARN_C\n"
        "  --pump-enable 0|1           default/env PUMP_ENABLE\n"
        "  --pca9685-pump-ch N        default/env PCA9685_PUMP_CH\n"
        "  --pca9685-pump-active-high 0|1 default/env PCA9685_PUMP_ACTIVE_HIGH\n"
        "  --spray-confirm-enable 0|1  default/env SPRAY_CONFIRM_ENABLE\n"
        "  --spray-require-vision 0|1  default/env SPRAY_REQUIRE_VISION\n"
        "  --spray-min-ai-score F      default/env SPRAY_MIN_AI_SCORE\n"
        "  --visual-state-path PATH    default/env VISUAL_STATE_PATH\n"
        "  --visual-confirm-window-ms N default/env VISUAL_CONFIRM_WINDOW_MS\n"
        "  --vision-offline-autospray 0|1 default/env VISION_OFFLINE_AUTOSPRAY\n"
        "  --spray-max-ms N            default/env SPRAY_MAX_MS\n"
        "  --spray-cooldown-ms N       default/env SPRAY_COOLDOWN_MS\n"
        "  --ai-connect-timeout-sec N  default/env AI_CONNECT_TIMEOUT_SEC\n"
        "  --ai-max-time-sec N         default/env AI_MAX_TIME_SEC\n"
        "  --patrol-enable 0|1         default/env PATROL_ENABLE\n"
        "  --patrol-interval-sec N     default/env PATROL_INTERVAL_SEC\n"
        "  --patrol-angles A,B,C       default/env PATROL_ANGLES (comma-separated degrees)\n"
        "  --patrol-settle-ms N        default/env PATROL_SETTLE_MS\n"
        "  --patrol-ai-enable 0|1      default/env PATROL_AI_ENABLE\n"
        "  --patrol-post-event 0|1     default/env PATROL_POST_EVENT\n"
        "  --patrol-ai-timeout-sec N   default/env PATROL_AI_TIMEOUT_SEC\n"
        "  --water-gun-on-flame-enable 0|1 default/env WATER_GUN_ON_FLAME_ENABLE\n"
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
    cfg->gpio_mq2 = set_from_env_int("GPIO_MQ2", DEFAULT_GPIO_MQ2);
    cfg->gpio_mq2_active_high = set_from_env_int("GPIO_MQ2_ACTIVE_HIGH", DEFAULT_GPIO_MQ2_ACTIVE_HIGH);
    cfg->gpio_rgb_r = set_from_env_int("GPIO_RGB_R", DEFAULT_GPIO_RGB_R);
    cfg->gpio_rgb_r_active_high = set_from_env_int("GPIO_RGB_R_ACTIVE_HIGH", DEFAULT_GPIO_RGB_R_ACTIVE_HIGH);
    cfg->gpio_buzzer = set_from_env_int("GPIO_BUZZER", DEFAULT_GPIO_BUZZER);
    cfg->gpio_buzzer_active_high = set_from_env_int("GPIO_BUZZER_ACTIVE_HIGH", DEFAULT_GPIO_BUZZER_ACTIVE_HIGH);
    cfg->gpio_rgb_g = set_from_env_int("GPIO_RGB_G", DEFAULT_GPIO_RGB_G);
    cfg->gpio_rgb_g_active_high = set_from_env_int("GPIO_RGB_G_ACTIVE_HIGH", DEFAULT_GPIO_RGB_G_ACTIVE_HIGH);
    cfg->gpio_rgb_b = set_from_env_int("GPIO_RGB_B", DEFAULT_GPIO_RGB_B);
    cfg->gpio_rgb_b_active_high = set_from_env_int("GPIO_RGB_B_ACTIVE_HIGH", DEFAULT_GPIO_RGB_B_ACTIVE_HIGH);
    set_from_env_string(cfg->rgb_backend, sizeof(cfg->rgb_backend), "RGB_BACKEND", DEFAULT_RGB_BACKEND);
    set_from_env_string(cfg->pca9685_bus, sizeof(cfg->pca9685_bus), "PCA9685_BUS", DEFAULT_PCA9685_BUS);
    cfg->pca9685_addr = set_from_env_int("PCA9685_ADDR", DEFAULT_PCA9685_ADDR);
    cfg->pca9685_rgb_r_ch = set_from_env_int("PCA9685_RGB_R_CH", DEFAULT_PCA9685_RGB_R_CH);
    cfg->pca9685_rgb_g_ch = set_from_env_int("PCA9685_RGB_G_CH", DEFAULT_PCA9685_RGB_G_CH);
    cfg->pca9685_rgb_b_ch = set_from_env_int("PCA9685_RGB_B_CH", DEFAULT_PCA9685_RGB_B_CH);
    cfg->pca9685_rgb_on_duty = set_from_env_int("PCA9685_RGB_ON_DUTY", DEFAULT_PCA9685_RGB_ON_DUTY);
    cfg->pca9685_rgb_off_duty = set_from_env_int("PCA9685_RGB_OFF_DUTY", DEFAULT_PCA9685_RGB_OFF_DUTY);
    cfg->post_normal = set_from_env_int("POST_NORMAL", 0);
    cfg->force_verify = set_from_env_int("FORCE_VERIFY", 0);
    cfg->interval_sec = set_from_env_int("INTERVAL_SEC", DEFAULT_INTERVAL_SEC);
    cfg->oled_enable = set_from_env_int("OLED_ENABLE", DEFAULT_OLED_ENABLE);
    set_from_env_string(cfg->oled_bus, sizeof(cfg->oled_bus), "OLED_BUS", DEFAULT_OLED_BUS);
    cfg->oled_addr = set_from_env_int("OLED_ADDR", DEFAULT_OLED_ADDR);
    cfg->relay_enable = set_from_env_int("RELAY_ENABLE", DEFAULT_RELAY_ENABLE);
    cfg->pca9685_relay_ch = set_from_env_int("PCA9685_RELAY_CH", DEFAULT_PCA9685_RELAY_CH);
    cfg->pca9685_relay_active_high = set_from_env_int("PCA9685_RELAY_ACTIVE_HIGH", DEFAULT_PCA9685_RELAY_ACTIVE_HIGH);
    cfg->soc_temp_enable = set_from_env_int("SOC_TEMP_ENABLE", DEFAULT_SOC_TEMP_ENABLE);
    set_from_env_string(cfg->soc_temp_path, sizeof(cfg->soc_temp_path), "SOC_TEMP_PATH", DEFAULT_SOC_TEMP_PATH);
    cfg->soc_temp_warn_c = set_from_env_int("SOC_TEMP_WARN_C", DEFAULT_SOC_TEMP_WARN_C);
    cfg->pump_enable = set_from_env_int("PUMP_ENABLE", DEFAULT_PUMP_ENABLE);
    cfg->pca9685_pump_ch = set_from_env_int("PCA9685_PUMP_CH", DEFAULT_PCA9685_PUMP_CH);
    cfg->pca9685_pump_active_high = set_from_env_int("PCA9685_PUMP_ACTIVE_HIGH", DEFAULT_PCA9685_PUMP_ACTIVE_HIGH);
    /* Spray double-confirmation */
    cfg->spray_confirm_enable = set_from_env_int("SPRAY_CONFIRM_ENABLE", DEFAULT_SPRAY_CONFIRM_ENABLE);
    cfg->spray_require_vision = set_from_env_int("SPRAY_REQUIRE_VISION", DEFAULT_SPRAY_REQUIRE_VISION);
    {
        const char *score_env = getenv("SPRAY_MIN_AI_SCORE");
        cfg->spray_min_ai_score = (score_env && score_env[0]) ? atof(score_env) : DEFAULT_SPRAY_MIN_AI_SCORE;
    }
    set_from_env_string(cfg->visual_state_path, sizeof(cfg->visual_state_path), "VISUAL_STATE_PATH", DEFAULT_VISUAL_STATE_PATH);
    cfg->visual_confirm_window_ms = set_from_env_int("VISUAL_CONFIRM_WINDOW_MS", DEFAULT_VISUAL_CONFIRM_WINDOW_MS);
    cfg->vision_offline_autospray = set_from_env_int("VISION_OFFLINE_AUTOSPRAY", DEFAULT_VISION_OFFLINE_AUTOSPRAY);
    cfg->spray_max_ms = set_from_env_int("SPRAY_MAX_MS", DEFAULT_SPRAY_MAX_MS);
    cfg->spray_cooldown_ms = set_from_env_int("SPRAY_COOLDOWN_MS", DEFAULT_SPRAY_COOLDOWN_MS);
    cfg->ai_connect_timeout_sec = set_from_env_int("AI_CONNECT_TIMEOUT_SEC", DEFAULT_AI_CONNECT_TIMEOUT_SEC);
    cfg->ai_max_time_sec = set_from_env_int("AI_MAX_TIME_SEC", DEFAULT_AI_MAX_TIME_SEC);
    set_from_env_string(cfg->pca9685_lock_path, sizeof(cfg->pca9685_lock_path), "PCA9685_LOCK_PATH", DEFAULT_PCA9685_LOCK_PATH);

    /* Patrol config */
    cfg->patrol_enable = set_from_env_int("PATROL_ENABLE", DEFAULT_PATROL_ENABLE);
    cfg->patrol_interval_sec = set_from_env_int("PATROL_INTERVAL_SEC", DEFAULT_PATROL_INTERVAL_SEC);
    cfg->patrol_settle_ms = set_from_env_int("PATROL_SETTLE_MS", DEFAULT_PATROL_SETTLE_MS);
    cfg->patrol_settle_capture_ms = set_from_env_int("PATROL_SETTLE_CAPTURE_MS", DEFAULT_PATROL_SETTLE_CAPTURE_MS);
    cfg->patrol_ai_enable = set_from_env_int("PATROL_AI_ENABLE", DEFAULT_PATROL_AI_ENABLE);
    cfg->patrol_post_event = set_from_env_int("PATROL_POST_EVENT", DEFAULT_PATROL_POST_EVENT);
    cfg->patrol_ai_timeout_sec = set_from_env_int("PATROL_AI_TIMEOUT_SEC", DEFAULT_PATROL_AI_TIMEOUT_SEC);
    {
        const char *angles_env = getenv("PATROL_ANGLES");
        const char *angles_str = (angles_env && angles_env[0]) ? angles_env : DEFAULT_PATROL_ANGLES_STR;
        cfg->patrol_angle_count = 0;
        char abuf[128];
        snprintf(abuf, sizeof(abuf), "%s", angles_str);
        char *tok = strtok(abuf, ",; ");
        while (tok && cfg->patrol_angle_count < DEFAULT_PATROL_MAX_ANGLES) {
            int a = atoi(tok);
            if (a >= 0 && a <= 180) {
                cfg->patrol_angles[cfg->patrol_angle_count++] = a;
            }
            tok = strtok(NULL, ",; ");
        }
        if (cfg->patrol_angle_count == 0) {
            cfg->patrol_angles[0] = 90;
            cfg->patrol_angle_count = 1;
        }
    }

    /* Water gun on flame */
    cfg->water_gun_on_flame_enable = set_from_env_int("WATER_GUN_ON_FLAME_ENABLE", DEFAULT_WATER_GUN_ON_FLAME_ENABLE);

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
        } else if (strcmp(argv[i], "--gpio-mq2") == 0 && i + 1 < argc) {
            if (parse_int(argv[++i], &cfg->gpio_mq2) != 0) {
                fprintf(stderr, "--gpio-mq2 requires integer\n");
                return -1;
            }
        } else if (strcmp(argv[i], "--gpio-mq2-active-high") == 0 && i + 1 < argc) {
            if (parse_int(argv[++i], &cfg->gpio_mq2_active_high) != 0) {
                fprintf(stderr, "--gpio-mq2-active-high requires 0|1\n");
                return -1;
            }
        } else if (strcmp(argv[i], "--gpio-rgb-r") == 0 && i + 1 < argc) {
            if (parse_int(argv[++i], &cfg->gpio_rgb_r) != 0) {
                fprintf(stderr, "--gpio-rgb-r requires integer\n");
                return -1;
            }
        } else if (strcmp(argv[i], "--gpio-rgb-r-active-high") == 0 && i + 1 < argc) {
            if (parse_int(argv[++i], &cfg->gpio_rgb_r_active_high) != 0) {
                fprintf(stderr, "--gpio-rgb-r-active-high requires 0|1\n");
                return -1;
            }
        } else if (strcmp(argv[i], "--gpio-buzzer") == 0 && i + 1 < argc) {
            if (parse_int(argv[++i], &cfg->gpio_buzzer) != 0) {
                fprintf(stderr, "--gpio-buzzer requires integer\n");
                return -1;
            }
        } else if (strcmp(argv[i], "--gpio-buzzer-active-high") == 0 && i + 1 < argc) {
            if (parse_int(argv[++i], &cfg->gpio_buzzer_active_high) != 0) {
                fprintf(stderr, "--gpio-buzzer-active-high requires 0|1\n");
                return -1;
            }
        } else if (strcmp(argv[i], "--gpio-rgb-g") == 0 && i + 1 < argc) {
            if (parse_int(argv[++i], &cfg->gpio_rgb_g) != 0) {
                fprintf(stderr, "--gpio-rgb-g requires integer\n");
                return -1;
            }
        } else if (strcmp(argv[i], "--gpio-rgb-g-active-high") == 0 && i + 1 < argc) {
            if (parse_int(argv[++i], &cfg->gpio_rgb_g_active_high) != 0) {
                fprintf(stderr, "--gpio-rgb-g-active-high requires 0|1\n");
                return -1;
            }
        } else if (strcmp(argv[i], "--gpio-rgb-b") == 0 && i + 1 < argc) {
            if (parse_int(argv[++i], &cfg->gpio_rgb_b) != 0) {
                fprintf(stderr, "--gpio-rgb-b requires integer\n");
                return -1;
            }
        } else if (strcmp(argv[i], "--gpio-rgb-b-active-high") == 0 && i + 1 < argc) {
            if (parse_int(argv[++i], &cfg->gpio_rgb_b_active_high) != 0) {
                fprintf(stderr, "--gpio-rgb-b-active-high requires 0|1\n");
                return -1;
            }
        } else if (strcmp(argv[i], "--rgb-backend") == 0 && i + 1 < argc) {
            copy_string(cfg->rgb_backend, sizeof(cfg->rgb_backend), argv[++i]);
        } else if (strcmp(argv[i], "--pca9685-bus") == 0 && i + 1 < argc) {
            copy_string(cfg->pca9685_bus, sizeof(cfg->pca9685_bus), argv[++i]);
        } else if (strcmp(argv[i], "--pca9685-addr") == 0 && i + 1 < argc) {
            cfg->pca9685_addr = (int)strtol(argv[++i], NULL, 0);
        } else if (strcmp(argv[i], "--pca9685-rgb-r-ch") == 0 && i + 1 < argc) {
            if (parse_int(argv[++i], &cfg->pca9685_rgb_r_ch) != 0) { return -1; }
        } else if (strcmp(argv[i], "--pca9685-rgb-g-ch") == 0 && i + 1 < argc) {
            if (parse_int(argv[++i], &cfg->pca9685_rgb_g_ch) != 0) { return -1; }
        } else if (strcmp(argv[i], "--pca9685-rgb-b-ch") == 0 && i + 1 < argc) {
            if (parse_int(argv[++i], &cfg->pca9685_rgb_b_ch) != 0) { return -1; }
        } else if (strcmp(argv[i], "--pca9685-rgb-on-duty") == 0 && i + 1 < argc) {
            if (parse_int(argv[++i], &cfg->pca9685_rgb_on_duty) != 0) { return -1; }
        } else if (strcmp(argv[i], "--pca9685-rgb-off-duty") == 0 && i + 1 < argc) {
            if (parse_int(argv[++i], &cfg->pca9685_rgb_off_duty) != 0) { return -1; }
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
        } else if (strcmp(argv[i], "--oled-enable") == 0 && i + 1 < argc) {
            if (parse_int(argv[++i], &cfg->oled_enable) != 0) {
                fprintf(stderr, "--oled-enable requires 0|1\n");
                return -1;
            }
        } else if (strcmp(argv[i], "--oled-bus") == 0 && i + 1 < argc) {
            copy_string(cfg->oled_bus, sizeof(cfg->oled_bus), argv[++i]);
        } else if (strcmp(argv[i], "--oled-addr") == 0 && i + 1 < argc) {
            cfg->oled_addr = (int)strtol(argv[++i], NULL, 0);
        } else if (strcmp(argv[i], "--relay-enable") == 0 && i + 1 < argc) {
            if (parse_int(argv[++i], &cfg->relay_enable) != 0) {
                fprintf(stderr, "--relay-enable requires 0|1\n");
                return -1;
            }
        } else if (strcmp(argv[i], "--pca9685-relay-ch") == 0 && i + 1 < argc) {
            if (parse_int(argv[++i], &cfg->pca9685_relay_ch) != 0) { return -1; }
        } else if (strcmp(argv[i], "--pca9685-relay-active-high") == 0 && i + 1 < argc) {
            if (parse_int(argv[++i], &cfg->pca9685_relay_active_high) != 0) { return -1; }
        } else if (strcmp(argv[i], "--soc-temp-enable") == 0 && i + 1 < argc) {
            if (parse_int(argv[++i], &cfg->soc_temp_enable) != 0) {
                fprintf(stderr, "--soc-temp-enable requires 0|1\n");
                return -1;
            }
        } else if (strcmp(argv[i], "--soc-temp-path") == 0 && i + 1 < argc) {
            copy_string(cfg->soc_temp_path, sizeof(cfg->soc_temp_path), argv[++i]);
        } else if (strcmp(argv[i], "--soc-temp-warn-c") == 0 && i + 1 < argc) {
            if (parse_int(argv[++i], &cfg->soc_temp_warn_c) != 0) { return -1; }
        } else if (strcmp(argv[i], "--pump-enable") == 0 && i + 1 < argc) {
            if (parse_int(argv[++i], &cfg->pump_enable) != 0) {
                fprintf(stderr, "--pump-enable requires 0|1\n");
                return -1;
            }
        } else if (strcmp(argv[i], "--pca9685-pump-ch") == 0 && i + 1 < argc) {
            if (parse_int(argv[++i], &cfg->pca9685_pump_ch) != 0) { return -1; }
        } else if (strcmp(argv[i], "--pca9685-pump-active-high") == 0 && i + 1 < argc) {
            if (parse_int(argv[++i], &cfg->pca9685_pump_active_high) != 0) { return -1; }
        } else if (strcmp(argv[i], "--spray-confirm-enable") == 0 && i + 1 < argc) {
            if (parse_int(argv[++i], &cfg->spray_confirm_enable) != 0) { return -1; }
        } else if (strcmp(argv[i], "--spray-require-vision") == 0 && i + 1 < argc) {
            if (parse_int(argv[++i], &cfg->spray_require_vision) != 0) { return -1; }
        } else if (strcmp(argv[i], "--spray-min-ai-score") == 0 && i + 1 < argc) {
            cfg->spray_min_ai_score = atof(argv[++i]);
        } else if (strcmp(argv[i], "--visual-state-path") == 0 && i + 1 < argc) {
            copy_string(cfg->visual_state_path, sizeof(cfg->visual_state_path), argv[++i]);
        } else if (strcmp(argv[i], "--visual-confirm-window-ms") == 0 && i + 1 < argc) {
            if (parse_int(argv[++i], &cfg->visual_confirm_window_ms) != 0) { return -1; }
        } else if (strcmp(argv[i], "--vision-offline-autospray") == 0 && i + 1 < argc) {
            if (parse_int(argv[++i], &cfg->vision_offline_autospray) != 0) { return -1; }
        } else if (strcmp(argv[i], "--spray-max-ms") == 0 && i + 1 < argc) {
            if (parse_int(argv[++i], &cfg->spray_max_ms) != 0) { return -1; }
        } else if (strcmp(argv[i], "--spray-cooldown-ms") == 0 && i + 1 < argc) {
            if (parse_int(argv[++i], &cfg->spray_cooldown_ms) != 0) { return -1; }
        } else if (strcmp(argv[i], "--ai-connect-timeout-sec") == 0 && i + 1 < argc) {
            if (parse_int(argv[++i], &cfg->ai_connect_timeout_sec) != 0) { return -1; }
        } else if (strcmp(argv[i], "--ai-max-time-sec") == 0 && i + 1 < argc) {
            if (parse_int(argv[++i], &cfg->ai_max_time_sec) != 0) { return -1; }
        } else if (strcmp(argv[i], "--patrol-enable") == 0 && i + 1 < argc) {
            if (parse_int(argv[++i], &cfg->patrol_enable) != 0) { return -1; }
        } else if (strcmp(argv[i], "--patrol-interval-sec") == 0 && i + 1 < argc) {
            if (parse_int(argv[++i], &cfg->patrol_interval_sec) != 0) { return -1; }
        } else if (strcmp(argv[i], "--patrol-angles") == 0 && i + 1 < argc) {
            cfg->patrol_angle_count = 0;
            char abuf[128];
            snprintf(abuf, sizeof(abuf), "%s", argv[++i]);
            char *tok = strtok(abuf, ",; ");
            while (tok && cfg->patrol_angle_count < DEFAULT_PATROL_MAX_ANGLES) {
                int a = atoi(tok);
                if (a >= 0 && a <= 180) {
                    cfg->patrol_angles[cfg->patrol_angle_count++] = a;
                }
                tok = strtok(NULL, ",; ");
            }
        } else if (strcmp(argv[i], "--patrol-settle-ms") == 0 && i + 1 < argc) {
            if (parse_int(argv[++i], &cfg->patrol_settle_ms) != 0) { return -1; }
        } else if (strcmp(argv[i], "--patrol-ai-enable") == 0 && i + 1 < argc) {
            if (parse_int(argv[++i], &cfg->patrol_ai_enable) != 0) { return -1; }
        } else if (strcmp(argv[i], "--patrol-post-event") == 0 && i + 1 < argc) {
            if (parse_int(argv[++i], &cfg->patrol_post_event) != 0) { return -1; }
        } else if (strcmp(argv[i], "--patrol-ai-timeout-sec") == 0 && i + 1 < argc) {
            if (parse_int(argv[++i], &cfg->patrol_ai_timeout_sec) != 0) { return -1; }
        } else if (strcmp(argv[i], "--water-gun-on-flame-enable") == 0 && i + 1 < argc) {
            if (parse_int(argv[++i], &cfg->water_gun_on_flame_enable) != 0) { return -1; }
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
    if (strcmp(cfg->rgb_backend, "gpio") != 0 && strcmp(cfg->rgb_backend, "pca9685") != 0) {
        fprintf(stderr, "--rgb-backend must be gpio or pca9685\n");
        return -1;
    }
    if (cfg->gpio_pir < 0 || (cfg->gpio_active_high != 0 && cfg->gpio_active_high != 1) ||
        cfg->gpio_flame < 0 || (cfg->gpio_flame_active_high != 0 && cfg->gpio_flame_active_high != 1) ||
        cfg->gpio_mq2 < 0 || (cfg->gpio_mq2_active_high != 0 && cfg->gpio_mq2_active_high != 1) ||
        cfg->gpio_rgb_r < 0 || (cfg->gpio_rgb_r_active_high != 0 && cfg->gpio_rgb_r_active_high != 1) ||
        cfg->gpio_buzzer < 0 || (cfg->gpio_buzzer_active_high != 0 && cfg->gpio_buzzer_active_high != 1) ||
        cfg->gpio_rgb_g < 0 || (cfg->gpio_rgb_g_active_high != 0 && cfg->gpio_rgb_g_active_high != 1) ||
        cfg->gpio_rgb_b < 0 || (cfg->gpio_rgb_b_active_high != 0 && cfg->gpio_rgb_b_active_high != 1) ||
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
    snprintf(paths->capture_dir, sizeof(paths->capture_dir), "%s/captures/opi5-safetyd", cfg->base_dir);
    snprintf(paths->spool_dir, sizeof(paths->spool_dir), "%s/spool/opi5-safetyd", cfg->base_dir);
    snprintf(paths->pending_dir, sizeof(paths->pending_dir), "%s/pending", paths->spool_dir);
    snprintf(paths->sent_dir, sizeof(paths->sent_dir), "%s/sent", paths->spool_dir);
    snprintf(paths->run_dir, sizeof(paths->run_dir), "%s/run", cfg->base_dir);
    snprintf(paths->seq_file, sizeof(paths->seq_file), "%s/seq.txt", paths->spool_dir);
    snprintf(paths->log_file, sizeof(paths->log_file), "%s/opi5_safetyd.log", paths->run_dir);
    snprintf(paths->status_file, sizeof(paths->status_file), "%s/opi5_safetyd_status.json", paths->run_dir);

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

static int ensure_gpio_export_out(int gpio) {
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
        (void)write_gpio_sysfs(gpio, "direction", "out");
        return 0;
    }
    return -1;
}

static void write_gpio_output(int gpio, int active_high, int desired_on) {
    int raw = desired_on ? active_high : !active_high;
    char val[4];
    snprintf(val, sizeof(val), "%d", raw ? 1 : 0);
    if (write_gpio_sysfs(gpio, "value", val) != 0) {
        log_msg("[output] WARNING: gpio%d write failed", gpio);
    }
}

static void pca9685_set_rgb(const AppConfig *cfg, int r_on, int g_on, int b_on) {
    char cmd[768];
    int r_duty = r_on ? cfg->pca9685_rgb_on_duty : cfg->pca9685_rgb_off_duty;
    int g_duty = g_on ? cfg->pca9685_rgb_on_duty : cfg->pca9685_rgb_off_duty;
    int b_duty = b_on ? cfg->pca9685_rgb_on_duty : cfg->pca9685_rgb_off_duty;
    char tool_path[512];
    char q_bus[80], q_tool[600];
    snprintf(tool_path, sizeof(tool_path), "%s/pca9685_set_pwm", cfg->base_dir);
    shell_quote(cfg->pca9685_bus, q_bus, sizeof(q_bus));
    shell_quote(tool_path, q_tool, sizeof(q_tool));

    int lock_fd = pca9685_lock_acquire(cfg->pca9685_lock_path);
    if (lock_fd < 0) {
        log_msg("[pca9685] WARNING: lock acquire failed, skip RGB write");
        return;
    }

    snprintf(cmd, sizeof(cmd),
             "%s --bus %s --addr 0x%02x --channel %d --duty %d && "
             "%s --bus %s --addr 0x%02x --channel %d --duty %d && "
             "%s --bus %s --addr 0x%02x --channel %d --duty %d",
             q_tool, q_bus, cfg->pca9685_addr, cfg->pca9685_rgb_r_ch, r_duty,
             q_tool, q_bus, cfg->pca9685_addr, cfg->pca9685_rgb_g_ch, g_duty,
             q_tool, q_bus, cfg->pca9685_addr, cfg->pca9685_rgb_b_ch, b_duty);
    if (run_shell_command(cmd) != 0) {
        log_msg("[pca9685] WARNING: RGB PWM write failed");
    }

    pca9685_lock_release(lock_fd);
}

static void pca9685_set_relay(const AppConfig *cfg, int on) {
    if (!cfg->relay_enable) return;
    int duty;
    if (cfg->pca9685_relay_active_high) {
        duty = on ? cfg->pca9685_rgb_on_duty : cfg->pca9685_rgb_off_duty;
    } else {
        duty = on ? cfg->pca9685_rgb_off_duty : cfg->pca9685_rgb_on_duty;
    }
    char tool_path[512], q_bus[80], q_tool[600], cmd[768];
    snprintf(tool_path, sizeof(tool_path), "%s/pca9685_set_pwm", cfg->base_dir);
    shell_quote(cfg->pca9685_bus, q_bus, sizeof(q_bus));
    shell_quote(tool_path, q_tool, sizeof(q_tool));

    int lock_fd = pca9685_lock_acquire(cfg->pca9685_lock_path);
    if (lock_fd < 0) {
        log_msg("[pca9685] WARNING: lock acquire failed, skip relay write");
        return;
    }
    snprintf(cmd, sizeof(cmd), "%s --bus %s --addr 0x%02x --channel %d --duty %d",
             q_tool, q_bus, cfg->pca9685_addr, cfg->pca9685_relay_ch, duty);
    if (run_shell_command(cmd) != 0) {
        log_msg("[relay] WARNING: CH%d duty=%d write failed", cfg->pca9685_relay_ch, duty);
    }
    pca9685_lock_release(lock_fd);
}

static void pca9685_set_pump(const AppConfig *cfg, int on) {
    if (!cfg->pump_enable) return;
    int duty;
    if (cfg->pca9685_pump_active_high) {
        duty = on ? cfg->pca9685_rgb_on_duty : cfg->pca9685_rgb_off_duty;
    } else {
        duty = on ? cfg->pca9685_rgb_off_duty : cfg->pca9685_rgb_on_duty;
    }
    char tool_path[512], q_bus[80], q_tool[600], cmd[768];
    snprintf(tool_path, sizeof(tool_path), "%s/pca9685_set_pwm", cfg->base_dir);
    shell_quote(cfg->pca9685_bus, q_bus, sizeof(q_bus));
    shell_quote(tool_path, q_tool, sizeof(q_tool));

    int lock_fd = pca9685_lock_acquire(cfg->pca9685_lock_path);
    if (lock_fd < 0) {
        log_msg("[pca9685] WARNING: lock acquire failed, skip pump write");
        return;
    }
    snprintf(cmd, sizeof(cmd), "%s --bus %s --addr 0x%02x --channel %d --duty %d",
             q_tool, q_bus, cfg->pca9685_addr, cfg->pca9685_pump_ch, duty);
    if (run_shell_command(cmd) != 0) {
        log_msg("[pump] WARNING: CH%d duty=%d write failed", cfg->pca9685_pump_ch, duty);
    }
    pca9685_lock_release(lock_fd);
}

static void apply_actuators_by_state(const AppConfig *cfg, const char *state,
                                      const SensorState *sensors, EventContext *ctx) {
    int buzzer_on = 0, r_on = 0, g_on = 0, b_on = 0, relay_on = 0, pump_on = 0;
    int use_pca = (strcmp(cfg->rgb_backend, "pca9685") == 0);

    if (strcmp(state, "NORMAL") == 0) {
        g_on = 1;
    } else if (strcmp(state, "VERIFY") == 0) {
        b_on = 1;
    } else if (strcmp(state, "ALARM") == 0) {
        r_on = 1;
        buzzer_on = 1;
        relay_on = 1;
        /* Pump: double-confirmation gate or legacy ALARM behavior */
        pump_on = should_pump_activate(cfg, sensors, ctx);
    } else if (strcmp(state, "FAULT") == 0) {
        r_on = 1;
        b_on = 1;
        buzzer_on = 1;
        /* FAULT: relay/pump off to avoid false activation */
    }

    /* Save actuator snapshot for event JSON (Hotfix A) */
    ctx->actuator_buzzer = buzzer_on;
    ctx->actuator_rgb_r = r_on;
    ctx->actuator_rgb_g = g_on;
    ctx->actuator_rgb_b = b_on;
    ctx->actuator_relay = relay_on;
    ctx->actuator_pump = pump_on;

    /* Correct alarm_phase to match actual pump state */
    if (strcmp(state, "ALARM") == 0) {
        if (pump_on) {
            copy_string(ctx->alarm_phase, sizeof(ctx->alarm_phase), "SUPPRESSING");
        } else if (sensors->flame || sensors->mq2) {
            copy_string(ctx->alarm_phase, sizeof(ctx->alarm_phase), "AIMING");
        }
    }

    /* Buzzer always on GPIO */
    write_gpio_output(cfg->gpio_buzzer, cfg->gpio_buzzer_active_high, buzzer_on);

    /* RGB: PCA9685 or GPIO */
    if (use_pca) {
        pca9685_set_rgb(cfg, r_on, g_on, b_on);
    } else {
        write_gpio_output(cfg->gpio_rgb_r, cfg->gpio_rgb_r_active_high, r_on);
        write_gpio_output(cfg->gpio_rgb_g, cfg->gpio_rgb_g_active_high, g_on);
        write_gpio_output(cfg->gpio_rgb_b, cfg->gpio_rgb_b_active_high, b_on);
    }

    /* Relay: PCA9685 CH5 */
    pca9685_set_relay(cfg, relay_on);

    /* Pump: PCA9685 CH6 (dual-load: water pump + water gun emitter) */
    pca9685_set_pump(cfg, pump_on);

    log_msg("[actuators] state=%s buzzer=%d R=%d G=%d B=%d relay=%d pump=%d rgb_backend=%s",
            state, buzzer_on, r_on, g_on, b_on, relay_on, pump_on, cfg->rgb_backend);
}

static void all_off(const AppConfig *cfg) {
    int use_pca = (strcmp(cfg->rgb_backend, "pca9685") == 0);

    /* Buzzer always on GPIO */
    write_gpio_output(cfg->gpio_buzzer, cfg->gpio_buzzer_active_high, 0);

    /* RGB off */
    if (use_pca) {
        pca9685_set_rgb(cfg, 0, 0, 0);
    } else {
        write_gpio_output(cfg->gpio_rgb_r, cfg->gpio_rgb_r_active_high, 0);
        write_gpio_output(cfg->gpio_rgb_g, cfg->gpio_rgb_g_active_high, 0);
        write_gpio_output(cfg->gpio_rgb_b, cfg->gpio_rgb_b_active_high, 0);
    }

    /* Relay off */
    pca9685_set_relay(cfg, 0);

    /* Pump off */
    pca9685_set_pump(cfg, 0);

    /* Water gun GPIO off */
    write_gpio_output(DEFAULT_GPIO_WATER_GUN, DEFAULT_GPIO_WATER_GUN_ACTIVE_HIGH, 0);

    log_msg("[actuators] all_off (backend=%s)", cfg->rgb_backend);
}

static volatile int g_oled_fd = -1;

static int oled_open(const AppConfig *cfg) {
    if (!cfg->oled_enable) return -1;
    int fd = open(cfg->oled_bus, O_RDWR);
    if (fd < 0) {
        log_msg("[oled] open %s failed: %s", cfg->oled_bus, strerror(errno));
        return -1;
    }
    if (ioctl(fd, I2C_SLAVE, cfg->oled_addr) < 0) {
        log_msg("[oled] set addr 0x%02X failed: %s", cfg->oled_addr, strerror(errno));
        close(fd);
        return -1;
    }
    if (oled_init(fd) != 0) {
        log_msg("[oled] init failed");
        close(fd);
        return -1;
    }
    if (oled_clear(fd) != 0) {
        log_msg("[oled] clear failed");
        close(fd);
        return -1;
    }
    log_msg("[oled] initialized on %s addr=0x%02X", cfg->oled_bus, cfg->oled_addr);
    return fd;
}

static void oled_refresh(int fd, const AppConfig *cfg, const SensorState *sensors, const EventContext *ctx) {
    (void)cfg;
    if (fd < 0) return;

    char line0[32], line1[32], line2[32], line3[32];
    snprintf(line0, sizeof(line0), "State: %s", ctx->state[0] ? ctx->state : "N/A");
    snprintf(line1, sizeof(line1), "Risk:%d H:%s", ctx->risk_score,
             ctx->device_health[0] ? ctx->device_health : "N/A");
    if (sensors->soc_temp_valid) {
        snprintf(line2, sizeof(line2), "Temp:%.1fC", sensors->soc_temp_c);
    } else {
        snprintf(line2, sizeof(line2), "Temp: N/A");
    }
    snprintf(line3, sizeof(line3), "S:P%dF%dM%d B%dR%dP%d",
             sensors->pir, sensors->flame, sensors->mq2,
             (ctx->state[0] == 'A' || ctx->state[0] == 'F') ? 1 : 0,
             (ctx->state[0] == 'A') ? 1 : 0,
             (ctx->state[0] == 'A') ? 1 : 0);

    oled_draw_text(fd, 0, 0, line0);
    oled_draw_text(fd, 0, 1, line1);
    oled_draw_text(fd, 0, 2, line2);
    oled_draw_text(fd, 0, 3, line3);
}

static void read_gpio(const AppConfig *cfg, SensorState *sensors, EventContext *ctx) {
    long long t0 = now_ms();
    sensors->door = 0;
    sensors->flame = 0;
    sensors->flame_raw = -1;
    sensors->mq2 = 0;
    sensors->mq2_raw = -1;
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

    /* MQ-2 */
    if (ensure_gpio_export(cfg->gpio_mq2) == 0) {
        sensors->mq2_raw = read_gpio_value(cfg->gpio_mq2);
    }

    if (sensors->mq2_raw == 0 || sensors->mq2_raw == 1) {
        sensors->mq2 = cfg->gpio_mq2_active_high ? sensors->mq2_raw : !sensors->mq2_raw;
    } else {
        sensors->mq2 = 0;
        if (ctx->last_error[0] == '\0') {
            copy_string(ctx->last_error, sizeof(ctx->last_error), "mq2_read_failed");
        }
    }

    ctx->gpio_ms = (int)(now_ms() - t0);
}

static void read_soc_temp(const AppConfig *cfg, SensorState *sensors) {
    sensors->soc_temp_valid = 0;
    sensors->soc_temp_c = 0.0;

    if (!cfg->soc_temp_enable) return;

    char buf[64];
    if (read_text_file_limited(cfg->soc_temp_path, buf, sizeof(buf)) != 0) {
        return;
    }
    long milli = strtol(buf, NULL, 10);
    if (milli <= 0 || milli > 200000) return;

    sensors->soc_temp_c = (double)milli / 1000.0;
    sensors->soc_temp_valid = 1;
}

static void compute_state(const AppConfig *cfg, const SensorState *sensors, EventContext *ctx) {
    (void)cfg;
    int local_score = 0;

    /* Strong trigger: flame=1 or mq2=1 → ALARM directly, independent of AI/network */
    if (sensors->flame) {
        local_score += 6;
    }
    if (sensors->mq2) {
        local_score += 5;
    }
    if (sensors->pir) {
        local_score += 2;
    }
    /* door skipped for now (Task10-A deferred) */

    if (sensors->flame || sensors->mq2) {
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

/* PCA9685 I2C lock (fcntl-based, works on BusyBox) */
static int pca9685_lock_acquire(const char *lock_path) {
    char dir[256];
    snprintf(dir, sizeof(dir), "%s", lock_path);
    char *slash = strrchr(dir, '/');
    if (slash) {
        *slash = '\0';
        mkdir_p(dir);
    }

    int fd = open(lock_path, O_CREAT | O_RDWR, 0644);
    if (fd < 0) {
        log_msg("[lock] WARNING: open %s failed: %s", lock_path, strerror(errno));
        return -1;
    }

    struct flock fl;
    memset(&fl, 0, sizeof(fl));
    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;

    if (fcntl(fd, F_SETLKW, &fl) != 0) {
        log_msg("[lock] WARNING: flock %s failed: %s", lock_path, strerror(errno));
        close(fd);
        return -1;
    }
    return fd;
}

static void pca9685_lock_release(int fd) {
    if (fd >= 0) {
        struct flock fl;
        memset(&fl, 0, sizeof(fl));
        fl.l_type = F_UNLCK;
        fl.l_whence = SEEK_SET;
        fcntl(fd, F_SETLK, &fl);
        close(fd);
    }
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

static double extract_json_double(const char *json, const char *key) {
    char needle[128];
    snprintf(needle, sizeof(needle), "\"%s\"", key);
    const char *p = strstr(json, needle);
    if (p == NULL) return 0.0;
    p = strchr(p, ':');
    if (p == NULL) return 0.0;
    p++;
    while (*p && isspace((unsigned char)*p)) p++;
    return atof(p);
}

static long long extract_json_ll(const char *json, const char *key) {
    char needle[128];
    snprintf(needle, sizeof(needle), "\"%s\"", key);
    const char *p = strstr(json, needle);
    if (p == NULL) return 0;
    p = strchr(p, ':');
    if (p == NULL) return 0;
    p++;
    while (*p && isspace((unsigned char)*p)) p++;
    return strtoll(p, NULL, 10);
}

static void extract_json_string(const char *json, const char *key, char *out, size_t size) {
    char needle[128];
    snprintf(needle, sizeof(needle), "\"%s\"", key);
    const char *p = strstr(json, needle);
    if (p == NULL) { out[0] = '\0'; return; }
    p = strchr(p, ':');
    if (p == NULL) { out[0] = '\0'; return; }
    p++;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p == '"') {
        p++;
        const char *end = strchr(p, '"');
        if (end == NULL) { out[0] = '\0'; return; }
        size_t len = (size_t)(end - p);
        if (len >= size) len = size - 1;
        memcpy(out, p, len);
        out[len] = '\0';
    } else {
        out[0] = '\0';
    }
}

static void read_visual_state(const AppConfig *cfg, EventContext *ctx) {
    char buf[1024];
    ctx->visual_fire_confirmed = 0;
    ctx->visual_fire_score = 0.0;
    ctx->visual_fire_label[0] = '\0';
    ctx->visual_state_ts_ms = 0;

    if (!cfg->spray_confirm_enable) return;

    if (read_text_file_limited(cfg->visual_state_path, buf, sizeof(buf)) != 0) {
        return;
    }

    if (buf[0] == '\0') return;

    /* Freshness check via monotonic_ms (Hotfix C) */
    long long visual_ms = extract_json_ll(buf, "monotonic_ms");
    long long age_ms = now_ms() - visual_ms;

    if (visual_ms <= 0 || age_ms < 0 || age_ms > cfg->visual_confirm_window_ms) {
        ctx->visual_fire_confirmed = 0;
        ctx->visual_fire_score = 0.0;
        ctx->visual_fire_label[0] = '\0';
        ctx->visual_state_ts_ms = visual_ms;
        return;
    }

    ctx->visual_fire_confirmed = (strstr(buf, "\"fire_confirmed\": true") != NULL) ? 1 : 0;
    ctx->visual_fire_score = extract_json_double(buf, "score");

    char label[32];
    extract_json_string(buf, "label", label, sizeof(label));
    copy_string(ctx->visual_fire_label, sizeof(ctx->visual_fire_label), label);
}

static void determine_alarm_phase(const AppConfig *cfg, const SensorState *sensors,
                                   const EventContext *ctx, char *phase, size_t phase_size) {
    if (strcmp(ctx->state, "ALARM") != 0) {
        copy_string(phase, phase_size, "IDLE");
        return;
    }

    if (!cfg->spray_confirm_enable) {
        /* Old behavior: pump on with ALARM */
        copy_string(phase, phase_size, "SUPPRESSING");
        return;
    }

    int local_hazard = sensors->flame || sensors->mq2;
    int vision_hazard = ctx->visual_fire_confirmed &&
                        ctx->visual_fire_score >= cfg->spray_min_ai_score;

    if (local_hazard && vision_hazard) {
        copy_string(phase, phase_size, "SUPPRESSING");
    } else if (local_hazard && !vision_hazard) {
        copy_string(phase, phase_size, "AIMING");
    } else {
        copy_string(phase, phase_size, "IDLE");
    }
}

static int should_pump_activate(const AppConfig *cfg, const SensorState *sensors,
                                 EventContext *ctx) {
    int local_hazard = sensors->flame || sensors->mq2;

    /* If no ALARM or no local hazard, stop spray if active */
    if (strcmp(ctx->state, "ALARM") != 0 || !local_hazard) {
        if (g_spray_active) {
            g_spray_active = 0;
            g_last_spray_stop_ms = now_ms();
        }
        copy_string(ctx->spray_decision, sizeof(ctx->spray_decision), "idle");
        return 0;
    }

    /* Legacy behavior: pump follows ALARM state (still subject to max/cooldown) */
    if (!cfg->spray_confirm_enable) {
        /* Apply cooldown */
        if (!g_spray_active && g_last_spray_stop_ms > 0) {
            long long since_stop = now_ms() - g_last_spray_stop_ms;
            if (since_stop >= 0 && since_stop < cfg->spray_cooldown_ms) {
                copy_string(ctx->spray_decision, sizeof(ctx->spray_decision), "cooldown");
                return 0;
            }
        }
        /* Apply max duration */
        if (g_spray_active) {
            long long spray_dur = now_ms() - g_last_spray_start_ms;
            if (spray_dur >= cfg->spray_max_ms) {
                g_spray_active = 0;
                g_last_spray_stop_ms = now_ms();
                log_msg("[spray] max duration %dms reached, stopping pump", cfg->spray_max_ms);
                copy_string(ctx->spray_decision, sizeof(ctx->spray_decision), "max_duration_stop");
                return 0;
            }
            copy_string(ctx->spray_decision, sizeof(ctx->spray_decision), "legacy_pump_on");
            return 1;
        }
        /* Start spray */
        g_spray_active = 1;
        g_last_spray_start_ms = now_ms();
        copy_string(ctx->spray_decision, sizeof(ctx->spray_decision), "legacy_pump_on");
        return 1;
    }

    /* Double-confirmation mode */
    int vision_hazard = ctx->visual_fire_confirmed &&
                        ctx->visual_fire_score >= cfg->spray_min_ai_score;

    if (cfg->spray_require_vision && !vision_hazard) {
        if (g_spray_active) {
            g_spray_active = 0;
            g_last_spray_stop_ms = now_ms();
        }
        copy_string(ctx->spray_decision, sizeof(ctx->spray_decision), "hold_pump");
        return 0;
    }

    /* Apply cooldown */
    if (!g_spray_active && g_last_spray_stop_ms > 0) {
        long long since_stop = now_ms() - g_last_spray_stop_ms;
        if (since_stop >= 0 && since_stop < cfg->spray_cooldown_ms) {
            copy_string(ctx->spray_decision, sizeof(ctx->spray_decision), "cooldown");
            return 0;
        }
    }

    /* Apply max duration */
    if (g_spray_active) {
        long long spray_dur = now_ms() - g_last_spray_start_ms;
        if (spray_dur >= cfg->spray_max_ms) {
            g_spray_active = 0;
            g_last_spray_stop_ms = now_ms();
            log_msg("[spray] max duration %dms reached, stopping pump", cfg->spray_max_ms);
            copy_string(ctx->spray_decision, sizeof(ctx->spray_decision), "max_duration_stop");
            return 0;
        }
        copy_string(ctx->spray_decision, sizeof(ctx->spray_decision), "pump_on");
        return 1;
    }

    /* Start spray */
    g_spray_active = 1;
    g_last_spray_start_ms = now_ms();
    copy_string(ctx->spray_decision, sizeof(ctx->spray_decision), "pump_on");
    return 1;
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
            "  \"source\": \"opi5_safetyd_c\",\n"
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
             "--connect-timeout %d --max-time %d > %s 2>/dev/null",
             q_image, q_meta, q_url, q_ai,
             cfg->ai_connect_timeout_sec, cfg->ai_max_time_sec, q_http);

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

    /* Use actuator snapshot from apply_actuators_by_state (Hotfix A) */
    int buzzer_out = ctx->actuator_buzzer;
    int rgb_r_out = ctx->actuator_rgb_r;
    int rgb_g_out = ctx->actuator_rgb_g;
    int rgb_b_out = ctx->actuator_rgb_b;
    int relay_out = ctx->actuator_relay;
    int pump_out = ctx->actuator_pump;

    /* Build soc_temp part of sensors JSON */
    char soc_temp_json[64];
    if (sensors->soc_temp_valid) {
        snprintf(soc_temp_json, sizeof(soc_temp_json), ", \"soc_temp\": %.1f", sensors->soc_temp_c);
    } else if (cfg->soc_temp_enable) {
        snprintf(soc_temp_json, sizeof(soc_temp_json), ", \"soc_temp\": null");
    } else {
        soc_temp_json[0] = '\0';
    }

    /* Build spray_confirm score string */
    char spray_score_str[32];
    if (ctx->visual_fire_score > 0) {
        snprintf(spray_score_str, sizeof(spray_score_str), "%.3f", ctx->visual_fire_score);
    } else {
        snprintf(spray_score_str, sizeof(spray_score_str), "null");
    }

    /* Build matched_label as valid JSON string or null (Hotfix B) */
    char matched_label_json[80];
    if (ctx->visual_fire_label[0]) {
        snprintf(matched_label_json, sizeof(matched_label_json), "\"%s\"", ctx->visual_fire_label);
    } else {
        snprintf(matched_label_json, sizeof(matched_label_json), "null");
    }

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
            "  \"sensors\": {\"door\": %d, \"pir\": %d, \"flame\": %d, \"mq2\": %d%s},\n"
            "  \"actuators\": {\"relay\": %d, \"pump\": %d, \"buzzer\": %d, \"rgb_r\": %d, \"rgb_g\": %d, \"rgb_b\": %d},\n"
            "  \"device_health\": \"%s\",\n"
            "  \"vision\": {\n"
            "    \"frame_id\": %d,\n"
            "    \"image_url\": null,\n"
            "    \"capture_ok\": %s,\n"
            "    \"pan_tilt\": {\"pan_deg\": 90, \"tilt_deg\": 90}\n"
            "  },\n"
            "  \"ai_result\": %s,\n"
            "  \"latency_ms\": {\"gpio\": %d, \"capture\": %d, \"ai\": %d, \"post\": %d, \"total\": %d},\n"
            "  \"diagnostics\": {\n"
            "    \"program\": \"opi5_safetyd_c\",\n"
            "    \"gpio\": {\"pir_gpio\": %d, \"raw_value\": %d, \"active_high\": %s, \"flame_gpio\": %d, \"flame_raw\": %d, \"flame_active_high\": %s, \"mq2_gpio\": %d, \"mq2_raw\": %d, \"mq2_active_high\": %s},\n"
            "    \"ai_status\": \"%s\",\n"
            "    \"backend_status\": \"%s\",\n"
            "    \"spool_path\": %s,\n"
            "    \"force_verify\": %s\n"
            "  },\n"
            "  \"alarm_phase\": \"%s\",\n"
            "  \"spray_confirm\": {\n"
            "    \"enabled\": %s,\n"
            "    \"local_hazard\": %s,\n"
            "    \"vision_hazard\": %s,\n"
            "    \"matched_label\": %s,\n"
            "    \"matched_score\": %s,\n"
            "    \"threshold\": %.2f,\n"
            "    \"decision\": \"%s\"\n"
            "  }\n"
            "}\n",
            cfg->device_id, ctx->seq, ctx->state, ctx->risk_score, ctx->need_snap ? "true" : "false",
            sensors->door, sensors->pir, sensors->flame, sensors->mq2, soc_temp_json,
            relay_out, pump_out, buzzer_out, rgb_r_out, rgb_g_out, rgb_b_out,
            ctx->device_health[0] ? ctx->device_health : "UNKNOWN",
            ctx->seq, ctx->capture_ok ? "true" : "false", ai_json,
            ctx->gpio_ms, ctx->capture_ms, ctx->ai_ms, post_ms, total_ms,
            cfg->gpio_pir, sensors->raw_value, cfg->gpio_active_high ? "true" : "false",
            cfg->gpio_flame, sensors->flame_raw, cfg->gpio_flame_active_high ? "true" : "false",
            cfg->gpio_mq2, sensors->mq2_raw, cfg->gpio_mq2_active_high ? "true" : "false",
            ctx->ai_status, backend_status, spool_path ? spool_path : "null", cfg->force_verify ? "true" : "false",
            /* alarm_phase */
            ctx->alarm_phase[0] ? ctx->alarm_phase : "IDLE",
            /* spray_confirm */
            cfg->spray_confirm_enable ? "true" : "false",
            (sensors->flame || sensors->mq2) ? "true" : "false",
            ctx->visual_fire_confirmed ? "true" : "false",
            matched_label_json,
            spray_score_str,
            cfg->spray_min_ai_score,
            /* decision */
            ctx->spray_decision[0] ? ctx->spray_decision : "idle");
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

    /* Build soc_temp part */
    char soc_temp_line[80];
    if (sensors && sensors->soc_temp_valid) {
        snprintf(soc_temp_line, sizeof(soc_temp_line), "  \"soc_temp\": %.1f,\n", sensors->soc_temp_c);
    } else {
        snprintf(soc_temp_line, sizeof(soc_temp_line), "  \"soc_temp\": null,\n");
    }

    fprintf(fp,
            "{\n"
            "  \"program\": \"opi5_safetyd_c\",\n"
            "  \"last_seq\": %d,\n"
            "  \"last_state\": \"%s\",\n"
            "  \"last_pir\": %d,\n"
            "  \"last_flame\": %d,\n"
            "  \"last_mq2\": %d,\n"
            "  \"last_risk_score\": %d,\n"
            "%s"
            "  \"device_health\": \"%s\",\n"
            "  \"last_ai_status\": \"%s\",\n"
            "  \"last_backend_status\": \"%s\",\n"
            "  \"last_error\": ",
            ctx->seq, ctx->state[0] ? ctx->state : "NONE", sensors ? sensors->pir : 0, sensors ? sensors->flame : 0, sensors ? sensors->mq2 : 0, ctx->risk_score,
            soc_temp_line,
            ctx->device_health[0] ? ctx->device_health : "UNKNOWN",
            ctx->ai_status[0] ? ctx->ai_status : "skipped",
            ctx->backend_status[0] ? ctx->backend_status : "none");
    if (ctx->last_error[0]) {
        fprintf(fp, "\"%s\"", ctx->last_error);
    } else {
        fprintf(fp, "null");
    }

    /* Water gun status */
    const char *wg_decision = "idle";
    if (g_wg_active) wg_decision = "active";
    else if (g_wg_last_stop_ms > 0 && (now_ms() - g_wg_last_stop_ms) < 2000) wg_decision = "cooldown";

    fprintf(fp,
            ",\n"
            "  \"water_gun\": %d,\n"
            "  \"water_gun_decision\": \"%s\",\n"
            "  \"water_gun_last_trigger_at\": %lld,\n"
            "  \"water_gun_cooldown_ms\": %d,\n"
            "  \"updated_at\": \"%s\"\n"
            "}\n",
            g_wg_active, wg_decision,
            g_wg_last_trigger_ms, 0,
            ts);
    fclose(fp);
}

/* ---- Servo PWM control via sysfs ---- */
static char g_servo_pwm_dir[256];
static int g_servo_pwm_ready = 0;

static int servo_init(void) {
    snprintf(g_servo_pwm_dir, sizeof(g_servo_pwm_dir),
             "/sys/class/pwm/pwmchip%d/pwm%d",
             DEFAULT_SERVO_PWM_CHIP, DEFAULT_SERVO_PWM_CHANNEL);

    char export_path[256], enable_path[280], period_path[280];
    snprintf(export_path, sizeof(export_path),
             "/sys/class/pwm/pwmchip%d/export", DEFAULT_SERVO_PWM_CHIP);
    snprintf(enable_path, sizeof(enable_path), "%s/enable", g_servo_pwm_dir);
    snprintf(period_path, sizeof(period_path), "%s/period", g_servo_pwm_dir);

    /* Export PWM channel if needed */
    if (!path_exists(g_servo_pwm_dir)) {
        char ch_str[8];
        snprintf(ch_str, sizeof(ch_str), "%d", DEFAULT_SERVO_PWM_CHANNEL);
        if (write_text_file(export_path, ch_str) != 0) {
            log_msg("[servo] export pwmchip%d ch%d failed: %s",
                    DEFAULT_SERVO_PWM_CHIP, DEFAULT_SERVO_PWM_CHANNEL, strerror(errno));
            return -1;
        }
        usleep(100000);
    }

    /* Set period ONCE, then enable — never rewrite period to avoid glitches */
    char val[32];
    snprintf(val, sizeof(val), "%d", DEFAULT_SERVO_PERIOD_NS);
    if (write_text_file(period_path, val) != 0) {
        log_msg("[servo] set period failed");
        return -1;
    }
    write_text_file(enable_path, "1");

    g_servo_pwm_ready = 1;
    log_msg("[servo] PWM initialized: pwmchip%d ch%d period=%dns",
            DEFAULT_SERVO_PWM_CHIP, DEFAULT_SERVO_PWM_CHANNEL, DEFAULT_SERVO_PERIOD_NS);
    return 0;
}

static int servo_set_angle(int angle_deg) {
    if (!g_servo_pwm_ready) {
        if (servo_init() != 0) return -1;
    }

    /* Clamp angle */
    if (angle_deg < 0) angle_deg = 0;
    if (angle_deg > 180) angle_deg = 180;

    /* angle → pulse width: 0°=min, 90°=center, 180°=max */
    int pulse_ns = DEFAULT_SERVO_PULSE_MIN_NS +
        (int)((long)(DEFAULT_SERVO_PULSE_MAX_NS - DEFAULT_SERVO_PULSE_MIN_NS) * angle_deg / 180);

    /* Only write duty_cycle — period is already set, no rewrite = no glitch */
    char duty_path[280], val[32];
    snprintf(duty_path, sizeof(duty_path), "%s/duty_cycle", g_servo_pwm_dir);
    snprintf(val, sizeof(val), "%d", pulse_ns);
    if (write_text_file(duty_path, val) != 0) {
        log_msg("[servo] set duty_cycle failed");
        return -1;
    }

    return 0;
}

static void servo_disable(void) {
    if (!g_servo_pwm_ready) return;
    char enable_path[280];
    snprintf(enable_path, sizeof(enable_path), "%s/enable", g_servo_pwm_dir);
    write_text_file(enable_path, "0");
    g_servo_pwm_ready = 0;
}

/* ---- Patrol round ---- */
static void patrol_round(const AppConfig *cfg, const AppPaths *paths) {
    long long t_start = now_ms();

    /* Generate patrol ID */
    g_patrol_seq++;
    char ts_tag[32];
    {
        time_t t = time(NULL);
        struct tm tm_utc;
        gmtime_r(&t, &tm_utc);
        strftime(ts_tag, sizeof(ts_tag), "%Y%m%d_%H%M%S", &tm_utc);
    }
    char patrol_id[64];
    snprintf(patrol_id, sizeof(patrol_id), "patrol_%s_%03d", ts_tag, g_patrol_seq);

    log_msg("[patrol] START patrol_id=%s angles=%d interval=%ds",
            patrol_id, cfg->patrol_angle_count, cfg->patrol_interval_sec);

    /* Read sensors once for this patrol round */
    SensorState sensors;
    EventContext ctx;
    memset(&sensors, 0, sizeof(sensors));
    memset(&ctx, 0, sizeof(ctx));
    copy_string(ctx.ai_status, sizeof(ctx.ai_status), "skipped");

    read_gpio(cfg, &sensors, &ctx);
    read_soc_temp(cfg, &sensors);

    /* Safety: apply basic actuators for current sensor state */
    compute_state(cfg, &sensors, &ctx);
    read_visual_state(cfg, &ctx);
    determine_alarm_phase(cfg, &sensors, &ctx, ctx.alarm_phase, sizeof(ctx.alarm_phase));
    apply_actuators_by_state(cfg, ctx.state, &sensors, &ctx);
    oled_refresh(g_oled_fd, cfg, &sensors, &ctx);

    /* Water gun on flame during patrol */
    if (cfg->water_gun_on_flame_enable && sensors.flame) {
        long long now = now_ms();
        int should_fire = 1;
        if (!g_wg_active && g_wg_last_stop_ms > 0) {
            long long since_stop = now - g_wg_last_stop_ms;
            if (since_stop >= 0 && since_stop < cfg->spray_cooldown_ms) {
                should_fire = 0;
                log_msg("[water_gun] cooldown active, %lldms remaining",
                        cfg->spray_cooldown_ms - since_stop);
            }
        }
        if (should_fire && !g_wg_active) {
            g_wg_active = 1;
            g_wg_last_trigger_ms = now;
            ensure_gpio_export_out(DEFAULT_GPIO_WATER_GUN);
            write_gpio_output(DEFAULT_GPIO_WATER_GUN, DEFAULT_GPIO_WATER_GUN_ACTIVE_HIGH, 1);
            log_msg("[water_gun] trigger reason=flame_sensor max_ms=%d cooldown_ms=%d",
                    cfg->spray_max_ms, cfg->spray_cooldown_ms);
        }
    }
    if (g_wg_active) {
        long long dur = now_ms() - g_wg_last_trigger_ms;
        if (dur >= cfg->spray_max_ms) {
            write_gpio_output(DEFAULT_GPIO_WATER_GUN, DEFAULT_GPIO_WATER_GUN_ACTIVE_HIGH, 0);
            g_wg_active = 0;
            g_wg_last_stop_ms = now_ms();
            log_msg("[water_gun] stop reason=max_duration dur=%lldms", dur);
        }
    }

    /* Ensure water gun GPIO off if not active */
    if (!g_wg_active) {
        ensure_gpio_export_out(DEFAULT_GPIO_WATER_GUN);
        write_gpio_output(DEFAULT_GPIO_WATER_GUN, DEFAULT_GPIO_WATER_GUN_ACTIVE_HIGH, 0);
    }

    /* Ensure patrol capture directory exists */
    char patrol_cap_dir[512];
    snprintf(patrol_cap_dir, sizeof(patrol_cap_dir), "%s/captures/patrol", cfg->base_dir);
    mkdir_p(patrol_cap_dir);

    int max_risk = 0;
    int angles_ok = 0;
    char overall_summary[512];
    overall_summary[0] = '\0';

    /* Build sweep sequence: center → left → center → right → center
     * Use first angle as "left", middle as "center", last as "right" */
    int left_a  = cfg->patrol_angles[0];
    int center_a = (cfg->patrol_angle_count >= 2) ? cfg->patrol_angles[1] : 90;
    int right_a = (cfg->patrol_angle_count >= 3) ? cfg->patrol_angles[2] : 120;
    /* Ensure center is actually the middle value */
    if (cfg->patrol_angle_count >= 3) {
        int sorted[3] = {left_a, center_a, right_a};
        /* Simple sort for 3 elements */
        if (sorted[0] > sorted[1]) { int t=sorted[0]; sorted[0]=sorted[1]; sorted[1]=t; }
        if (sorted[1] > sorted[2]) { int t=sorted[1]; sorted[1]=sorted[2]; sorted[2]=t; }
        if (sorted[0] > sorted[1]) { int t=sorted[0]; sorted[0]=sorted[1]; sorted[1]=t; }
        left_a = sorted[0]; center_a = sorted[1]; right_a = sorted[2];
    }

    /* sweep_steps: {angle, should_capture} */
    struct { int angle; int capture; } sweep[] = {
        { center_a, 1 },   /* 1. 起始中心，抓拍 */
        { left_a,   1 },   /* 2. 转左，抓拍 */
        { center_a, 0 },   /* 3. 回中，只停顿不拍 */
        { right_a,  1 },   /* 4. 转右，抓拍 */
        { center_a, 0 },   /* 5. 回中，结束 */
    };
    int sweep_count = sizeof(sweep) / sizeof(sweep[0]);

    /* Track captured images for batch AI */
    char captured_paths[16][512];
    int captured_angles[16];
    int capture_count = 0;

    log_msg("[patrol] sweep: %d°→%d°→%d°→%d°→%d° (capture at center/left/right)",
            center_a, left_a, center_a, right_a, center_a);

    for (int si = 0; si < sweep_count && keep_running; si++) {
        int angle = sweep[si].angle;
        int do_capture = sweep[si].capture;
        log_msg("[patrol] step %d/%d: %d° %s", si + 1, sweep_count, angle,
                do_capture ? "[抓拍]" : "[停顿]");

        /* 1. Move servo */
        if (servo_set_angle(angle) != 0) {
            log_msg("[patrol] servo move to %d° failed", angle);
            continue;
        }

        /* 2. Settle: longer for capture positions */
        int settle_ms = do_capture ? cfg->patrol_settle_capture_ms : cfg->patrol_settle_ms;
        log_msg("[patrol] settling %dms...", settle_ms);
        usleep((unsigned long)settle_ms * 1000);

        if (!do_capture) {
            angles_ok++;
            continue;
        }

        /* 3. Capture frame */
        char capture_path[512];
        snprintf(capture_path, sizeof(capture_path),
                 "%s/%s_angle_%d.jpg", patrol_cap_dir, patrol_id, angle);

        char q_dev[160], q_out[640], cap_cmd[2048];
        shell_quote(cfg->capture_device, q_dev, sizeof(q_dev));
        shell_quote(capture_path, q_out, sizeof(q_out));
        snprintf(cap_cmd, sizeof(cap_cmd),
                 "v4l2-ctl -d %s --set-fmt-video=width=640,height=480,pixelformat=MJPG "
                 "--stream-mmap=3 --stream-count=1 --stream-to=%s >/dev/null 2>&1",
                 q_dev, q_out);

        if (run_shell_command(cap_cmd) == 0 && path_exists(capture_path)) {
            log_msg("[patrol] capture ok: %s", capture_path);
            if (capture_count < 16) {
                snprintf(captured_paths[capture_count], 512, "%s", capture_path);
                captured_angles[capture_count] = angle;
                capture_count++;
            }
        } else {
            log_msg("[patrol] capture failed at %d°", angle);
        }
        angles_ok++;
    }

    /* 4. Batch AI: send all captured images + sensor summary in ONE call */
    int risk_hint = 0;
    char ai_summary[256] = "";
    char ai_status_str[32] = "skipped";

    if (cfg->patrol_ai_enable && capture_count > 0) {
        /* Build combined metadata with sensor summary */
        char meta_path[512];
        snprintf(meta_path, sizeof(meta_path), "%s/%s_meta.json",
                 paths->spool_dir, patrol_id);

        FILE *mfp = fopen(meta_path, "w");
        if (mfp) {
            char iso_ts[32];
            iso_time_utc(iso_ts, sizeof(iso_ts));
            fprintf(mfp,
                    "{\n"
                    "  \"contract_version\": \"1.0\",\n"
                    "  \"device_id\": \"%s\",\n"
                    "  \"source\": \"patrol\",\n"
                    "  \"patrol_id\": \"%s\",\n"
                    "  \"timestamp\": \"%s\",\n"
                    "  \"angles\": [%d, %d, %d],\n"
                    "  \"capture_count\": %d,\n"
                    "  \"sensors\": {\"pir\": %d, \"flame\": %d, \"mq2\": %d, \"soc_temp\": %.1f},\n"
                    "  \"sensor_summary\": \"PIR=%d flame=%d MQ2=%d temperature=%.1fC\",\n"
                    "  \"image_format\": \"jpeg\"\n"
                    "}\n",
                    cfg->device_id, patrol_id, iso_ts,
                    left_a, center_a, right_a, capture_count,
                    sensors.pir, sensors.flame, sensors.mq2,
                    sensors.soc_temp_valid ? sensors.soc_temp_c : 0.0,
                    sensors.pir, sensors.flame, sensors.mq2,
                    sensors.soc_temp_valid ? sensors.soc_temp_c : 0.0);
            fclose(mfp);
        }

        /* Build curl command with multiple -F image=@... */
        char ai_out[512];
        snprintf(ai_out, sizeof(ai_out), "%s/%s_ai.json",
                 paths->spool_dir, patrol_id);
        char ai_http[512];
        snprintf(ai_http, sizeof(ai_http), "%s/%s_ai.http",
                 paths->spool_dir, patrol_id);
        (void)unlink(ai_out);
        (void)unlink(ai_http);

        char url[320];
        snprintf(url, sizeof(url), "%s/api/infer/vision", cfg->opi5_url);

        /* Build multi-image curl: -F image=@path1 -F image=@path2 ... */
        char img_parts[4096];
        img_parts[0] = '\0';
        for (int ci = 0; ci < capture_count; ci++) {
            char q_img[900];
            char part[900];
            snprintf(part, sizeof(part), "image=@%s", captured_paths[ci]);
            shell_quote(part, q_img, sizeof(q_img));
            if (ci > 0) strncat(img_parts, " ", sizeof(img_parts) - strlen(img_parts) - 1);
            strncat(img_parts, q_img, sizeof(img_parts) - strlen(img_parts) - 1);
        }

        char q_meta[900], q_url[400], q_ai_out[700], q_http_out[700];
        char meta_part[900];
        snprintf(meta_part, sizeof(meta_part), "metadata=<%s", meta_path);
        shell_quote(meta_part, q_meta, sizeof(q_meta));
        shell_quote(url, q_url, sizeof(q_url));
        shell_quote(ai_out, q_ai_out, sizeof(q_ai_out));
        shell_quote(ai_http, q_http_out, sizeof(q_http_out));

        char ai_cmd[8192];
        snprintf(ai_cmd, sizeof(ai_cmd),
                 "curl -sS -X POST %s -F %s %s -o %s -w '%%{http_code}' "
                 "--connect-timeout %d --max-time %d > %s 2>/dev/null",
                 img_parts, q_meta, q_url, q_ai_out,
                 cfg->patrol_ai_timeout_sec, cfg->patrol_ai_timeout_sec * 2, q_http_out);

        log_msg("[patrol] batch AI: %d images + sensor summary → %s", capture_count, cfg->opi5_url);
        int ai_code = 0;
        if (run_shell_command(ai_cmd) == 0) {
            ai_code = read_http_code_file(ai_http);
        }

        if (ai_code == 200) {
            char ai_buf[16384];
            if (read_text_file_limited(ai_out, ai_buf, sizeof(ai_buf)) == 0) {
                risk_hint = extract_risk_hint(ai_buf);
                copy_string(ai_status_str, sizeof(ai_status_str), "ok");
                char summary_raw[512];
                extract_json_string(ai_buf, "summary", summary_raw, sizeof(summary_raw));
                if (summary_raw[0]) {
                    copy_string(ai_summary, sizeof(ai_summary), summary_raw);
                }
            }
        } else {
            copy_string(ai_status_str, sizeof(ai_status_str), "offline");
        }
        max_risk = risk_hint;
        log_msg("[patrol] batch AI result: status=%s risk_hint=%d summary=%.80s",
                ai_status_str, risk_hint, ai_summary);
    }

    /* Return servo to center */
    servo_set_angle(90);

    /* Build patrol status JSON */
    long long duration_ms = now_ms() - t_start;
    char patrol_status_path[512];
    snprintf(patrol_status_path, sizeof(patrol_status_path),
             "%s/opi5_patrol_status.json", paths->run_dir);

    char iso_ts[32];
    iso_time_utc(iso_ts, sizeof(iso_ts));

    FILE *pfp = fopen(patrol_status_path, "w");
    if (pfp) {
        fprintf(pfp,
                "{\n"
                "  \"ok\": true,\n"
                "  \"patrol_id\": \"%s\",\n"
                "  \"timestamp\": \"%s\",\n"
                "  \"interval_sec\": %d,\n"
                "  \"angles\": [",
                patrol_id, iso_ts, cfg->patrol_interval_sec);
        for (int i = 0; i < cfg->patrol_angle_count; i++) {
            if (i > 0) fprintf(pfp, ", ");
            fprintf(pfp, "%d", cfg->patrol_angles[i]);
        }
        fprintf(pfp,
                "],\n"
                "  \"results\": [\n");
        /* Re-read AI results for each angle to build results array */
        for (int i = 0; i < cfg->patrol_angle_count; i++) {
            int angle = cfg->patrol_angles[i];
            char ai_path[512], cap_path[512];
            snprintf(ai_path, sizeof(ai_path), "%s/%s_ai_%d.json",
                     paths->spool_dir, patrol_id, angle);
            snprintf(cap_path, sizeof(cap_path), "%s/%s_angle_%d.jpg",
                     patrol_cap_dir, patrol_id, angle);

            int r_cap_ok = path_exists(cap_path) ? 1 : 0;
            int r_risk = 0;
            char r_summary[256] = "not processed";
            char r_status[32] = "skipped";

            if (cfg->patrol_ai_enable && r_cap_ok) {
                char ai_buf[16384];
                if (read_text_file_limited(ai_path, ai_buf, sizeof(ai_buf)) == 0 && ai_buf[0] == '{') {
                    r_risk = extract_risk_hint(ai_buf);
                    copy_string(r_status, sizeof(r_status), "ok");
                    char s[256];
                    extract_json_string(ai_buf, "summary", s, sizeof(s));
                    if (s[0]) copy_string(r_summary, sizeof(r_summary), s);
                } else {
                    copy_string(r_status, sizeof(r_status), "offline");
                }
            }

            fprintf(pfp,
                    "    %s{\n"
                    "      \"angle_deg\": %d,\n"
                    "      \"capture_ok\": %s,\n"
                    "      \"ai_status\": \"%s\",\n"
                    "      \"risk_hint\": %d,\n"
                    "      \"summary\": \"%s\"\n"
                    "    }",
                    i > 0 ? "," : "", angle,
                    r_cap_ok ? "true" : "false",
                    r_status, r_risk, r_summary);
            fprintf(pfp, "\n");
        }

        /* Generate overall summary */
        if (max_risk == 0) {
            snprintf(overall_summary, sizeof(overall_summary),
                     "本轮 %d 角度巡检未发现火焰或烟雾异常", cfg->patrol_angle_count);
        } else if (max_risk <= 3) {
            snprintf(overall_summary, sizeof(overall_summary),
                     "本轮 %d 角度巡检发现低风险信号 (risk=%d)", cfg->patrol_angle_count, max_risk);
        } else {
            snprintf(overall_summary, sizeof(overall_summary),
                     "本轮 %d 角度巡检发现风险信号 (risk=%d)，请人工确认", cfg->patrol_angle_count, max_risk);
        }

        fprintf(pfp,
                "  ],\n"
                "  \"max_risk_hint\": %d,\n"
                "  \"overall_summary\": \"%s\",\n"
                "  \"duration_ms\": %lld\n"
                "}\n",
                max_risk, overall_summary, duration_ms);
        fclose(pfp);
        log_msg("[patrol] status written: %s", patrol_status_path);
    }

    /* Post patrol event to backend */
    if (cfg->patrol_post_event) {
        char event_json_path[512];
        snprintf(event_json_path, sizeof(event_json_path),
                 "%s/patrol_event_%s.json", paths->spool_dir, patrol_id);

        const char *level = (max_risk >= 6) ? "danger" : (max_risk >= 3) ? "warn" : "info";
        char message[512];
        snprintf(message, sizeof(message),
                 "舵机巡检完成：%d 个角度，最高风险 %d，%s",
                 cfg->patrol_angle_count, max_risk,
                 max_risk == 0 ? "未发现明显异常" : "存在风险信号");

        FILE *efp = fopen(event_json_path, "w");
        if (efp) {
            fprintf(efp,
                    "{\n"
                    "  \"type\": \"event\",\n"
                    "  \"device_id\": \"%s\",\n"
                    "  \"state\": \"PATROL\",\n"
                    "  \"risk_score\": %d,\n"
                    "  \"need_snap\": false,\n"
                    "  \"sensors\": {\"pir\": %d, \"flame\": %d, \"mq2\": %d},\n"
                    "  \"actuators\": {},\n"
                    "  \"source\": \"patrol\",\n"
                    "  \"level\": \"%s\",\n"
                    "  \"message\": \"%s\",\n"
                    "  \"patrol\": {\n"
                    "    \"patrol_id\": \"%s\",\n"
                    "    \"angles\": [",
                    cfg->device_id, max_risk,
                    sensors.pir, sensors.flame, sensors.mq2,
                    level, message, patrol_id);
            for (int i = 0; i < cfg->patrol_angle_count; i++) {
                if (i > 0) fprintf(efp, ", ");
                fprintf(efp, "%d", cfg->patrol_angles[i]);
            }
            fprintf(efp,
                    "],\n"
                    "    \"max_risk_hint\": %d,\n"
                    "    \"overall_summary\": \"%s\",\n"
                    "    \"duration_ms\": %lld\n"
                    "  }\n"
                    "}\n",
                    max_risk, overall_summary, duration_ms);
            fclose(efp);
        }

        /* POST to backend */
        char post_cmd[4096];
        char q_url[400], q_event[700], q_resp[700], q_http[700];
        char url[320], resp_path[512], http_path[512];
        snprintf(url, sizeof(url), "%s/api/events", cfg->backend_url);
        snprintf(resp_path, sizeof(resp_path), "%s.response.txt", event_json_path);
        snprintf(http_path, sizeof(http_path), "%s.http", event_json_path);
        shell_quote(url, q_url, sizeof(q_url));
        shell_quote(event_json_path, q_event, sizeof(q_event));
        shell_quote(resp_path, q_resp, sizeof(q_resp));
        shell_quote(http_path, q_http, sizeof(q_http));
        snprintf(post_cmd, sizeof(post_cmd),
                 "curl -sS -X POST %s -H 'Content-Type: application/json' -d @%s -o %s "
                 "-w '%%{http_code}' --connect-timeout 5 --max-time 10 > %s 2>/dev/null",
                 q_url, q_event, q_resp, q_http);

        int post_code = 0;
        if (run_shell_command(post_cmd) == 0) {
            post_code = read_http_code_file(http_path);
        }
        log_msg("[patrol] backend POST: HTTP %03d", post_code);
    }

    /* Keep servo at center with PWM active (prevents drift) */
    servo_set_angle(90);

    log_msg("[patrol] DONE patrol_id=%s angles_ok=%d max_risk=%d duration=%lldms",
            patrol_id, angles_ok, max_risk, duration_ms);
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
    read_soc_temp(cfg, &sensors);
    compute_state(cfg, &sensors, &ctx);

    /* Compute device_health (independent of state machine) */
    if (sensors.soc_temp_valid && sensors.soc_temp_c >= cfg->soc_temp_warn_c) {
        copy_string(ctx.device_health, sizeof(ctx.device_health), "WARN");
    } else if (sensors.soc_temp_valid) {
        copy_string(ctx.device_health, sizeof(ctx.device_health), "NORMAL");
    } else {
        copy_string(ctx.device_health, sizeof(ctx.device_health), "UNKNOWN");
    }

    log_msg("[gpio] gpio%d raw=%d pir=%d | gpio%d flame_raw=%d flame=%d | gpio%d mq2_raw=%d mq2=%d",
            cfg->gpio_pir, sensors.raw_value, sensors.pir,
            cfg->gpio_flame, sensors.flame_raw, sensors.flame,
            cfg->gpio_mq2, sensors.mq2_raw, sensors.mq2);
    if (cfg->soc_temp_enable) {
        log_msg("[soc_temp] valid=%d temp=%.1fC warn=%dC device_health=%s",
                sensors.soc_temp_valid, sensors.soc_temp_c, cfg->soc_temp_warn_c, ctx.device_health);
    }
    log_msg("[state] %s risk_before_ai=%d need_snap=%s",
            ctx.state, ctx.risk_score_before_ai, ctx.need_snap ? "true" : "false");

    /* Initialize output GPIOs and apply actuators */
    ensure_gpio_export_out(cfg->gpio_buzzer);
    if (strcmp(cfg->rgb_backend, "gpio") == 0) {
        ensure_gpio_export_out(cfg->gpio_rgb_r);
        ensure_gpio_export_out(cfg->gpio_rgb_g);
        ensure_gpio_export_out(cfg->gpio_rgb_b);
    }

    /* Read visual state from tracker (for double-confirmation) */
    read_visual_state(cfg, &ctx);
    determine_alarm_phase(cfg, &sensors, &ctx, ctx.alarm_phase, sizeof(ctx.alarm_phase));

    if (cfg->spray_confirm_enable) {
        log_msg("[spray] alarm_phase=%s visual_fire=%d score=%.3f label=%s",
                ctx.alarm_phase, ctx.visual_fire_confirmed, ctx.visual_fire_score,
                ctx.visual_fire_label[0] ? ctx.visual_fire_label : "none");
    }

    apply_actuators_by_state(cfg, ctx.state, &sensors, &ctx);

    /* Water gun on flame: check every cycle (not just patrol) */
    if (cfg->water_gun_on_flame_enable) {
        long long now = now_ms();
        if (sensors.flame) {
            if (!g_wg_active) {
                /* Check cooldown */
                int can_fire = 1;
                if (g_wg_last_stop_ms > 0) {
                    long long since_stop = now - g_wg_last_stop_ms;
                    if (since_stop >= 0 && since_stop < cfg->spray_cooldown_ms) {
                        can_fire = 0;
                        log_msg("[water_gun] flame=1 but cooldown (%lldms/%dms remaining)",
                                cfg->spray_cooldown_ms - since_stop, cfg->spray_cooldown_ms);
                    }
                }
                if (can_fire) {
                    g_wg_active = 1;
                    g_wg_last_trigger_ms = now;
                    ensure_gpio_export_out(DEFAULT_GPIO_WATER_GUN);
                    write_gpio_output(DEFAULT_GPIO_WATER_GUN, DEFAULT_GPIO_WATER_GUN_ACTIVE_HIGH, 1);
                    log_msg("[water_gun] TRIGGER reason=flame_sensor max_ms=%d cooldown_ms=%d",
                            cfg->spray_max_ms, cfg->spray_cooldown_ms);
                }
            }
        }
        /* Check max duration */
        if (g_wg_active) {
            long long dur = now - g_wg_last_trigger_ms;
            if (dur >= cfg->spray_max_ms) {
                write_gpio_output(DEFAULT_GPIO_WATER_GUN, DEFAULT_GPIO_WATER_GUN_ACTIVE_HIGH, 0);
                g_wg_active = 0;
                g_wg_last_stop_ms = now;
                log_msg("[water_gun] STOP reason=max_duration dur=%lldms", dur);
            }
        }
        /* Ensure off when not active and no flame */
        if (!g_wg_active && !sensors.flame) {
            write_gpio_output(DEFAULT_GPIO_WATER_GUN, DEFAULT_GPIO_WATER_GUN_ACTIVE_HIGH, 0);
        }
    }

    /* Refresh OLED display (non-fatal) */
    oled_refresh(g_oled_fd, cfg, &sensors, &ctx);

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
    copy_string(status_ctx.device_health, sizeof(status_ctx.device_health), "UNKNOWN");

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
    log_msg("[loop] start interval_sec=%d patrol=%s",
            cfg->interval_sec, cfg->patrol_enable ? "enabled" : "disabled");
    if (cfg->patrol_enable) {
        log_msg("[loop] patrol_interval=%ds angles=%d settle=%dms ai=%s",
                cfg->patrol_interval_sec, cfg->patrol_angle_count,
                cfg->patrol_settle_ms, cfg->patrol_ai_enable ? "on" : "off");
        g_last_patrol_ms = now_ms();
    }
    while (keep_running) {
        long long now = now_ms();

        /* Check if patrol is due (patrol replaces one normal cycle) */
        if (cfg->patrol_enable) {
            long long since = now - g_last_patrol_ms;
            if (since >= (long long)cfg->patrol_interval_sec * 1000LL) {
                g_last_patrol_ms = now;
                patrol_round(cfg, paths);
                if (!keep_running) break;
                /* After patrol, sleep normal interval before next cycle */
                sleep_seconds_interruptible(cfg->interval_sec);
                continue;
            }
        }

        (void)run_once(cfg, paths);
        if (!keep_running) {
            break;
        }
        sleep_seconds_interruptible(cfg->interval_sec);
    }
    /* Ensure servo centered and water gun off on exit */
    servo_set_angle(90);
    servo_disable();
    write_gpio_output(DEFAULT_GPIO_WATER_GUN, DEFAULT_GPIO_WATER_GUN_ACTIVE_HIGH, 0);
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

    /* Make config available to signal handler for all_off cleanup */
    memcpy(&g_cfg, &cfg, sizeof(AppConfig));
    g_cfg_ready = 1;

    /* Initialize output GPIOs to OFF at startup */
    ensure_gpio_export_out(cfg.gpio_buzzer);
    if (strcmp(cfg.rgb_backend, "gpio") == 0) {
        ensure_gpio_export_out(cfg.gpio_rgb_r);
        ensure_gpio_export_out(cfg.gpio_rgb_g);
        ensure_gpio_export_out(cfg.gpio_rgb_b);
    }
    /* Water gun GPIO init */
    ensure_gpio_export_out(DEFAULT_GPIO_WATER_GUN);
    all_off(&cfg);

    /* Open OLED if enabled (non-fatal on failure) */
    g_oled_fd = oled_open(&cfg);

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    log_msg("[start] mode=%s device_id=%s base_dir=%s oled=%s patrol=%s wg_flame=%s",
            cfg.mode, cfg.device_id, cfg.base_dir,
            cfg.oled_enable ? "enabled" : "disabled",
            cfg.patrol_enable ? "enabled" : "disabled",
            cfg.water_gun_on_flame_enable ? "enabled" : "disabled");
    int rc = 0;
    if (strcmp(cfg.mode, "flush") == 0) {
        rc = flush_spool(&cfg, &paths);
    } else if (strcmp(cfg.mode, "loop") == 0) {
        rc = run_loop(&cfg, &paths);
    } else {
        rc = run_once(&cfg, &paths);
    }
    all_off(&cfg);
    if (g_oled_fd >= 0) {
        close(g_oled_fd);
        g_oled_fd = -1;
    }
    return rc;
}
