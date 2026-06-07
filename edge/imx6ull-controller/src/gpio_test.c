#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define GPIO_SYSFS_DIR "/sys/class/gpio"

static volatile sig_atomic_t keep_running = 1;

static void handle_signal(int signum) {
    (void)signum;
    keep_running = 0;
}

static void usage(const char *prog) {
    fprintf(stderr,
            "Usage:\n"
            "  %s --sysfs <GPIO_NUM> [--watch] [--interval-ms <MS>]\n"
            "  %s --gpiochip <gpiochipN|N> --line <LINE> [--watch] [--interval-ms <MS>]\n"
            "\n"
            "Examples:\n"
            "  %s --sysfs 133\n"
            "  %s --sysfs 133 --watch\n"
            "  %s --gpiochip gpiochip128 --line 5 --watch\n"
            "\n"
            "Notes:\n"
            "  This program only configures the selected GPIO as input and reads value.\n"
            "  It does not drive output levels and does not unexport the GPIO on exit.\n",
            prog, prog, prog, prog, prog);
}

static int parse_long(const char *text, long *out) {
    char *end = NULL;
    errno = 0;
    long value = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value < 0) {
        return -1;
    }
    *out = value;
    return 0;
}

static int parse_gpiochip_base(const char *text, long *out) {
    const char *prefix = "gpiochip";
    size_t prefix_len = strlen(prefix);

    if (strncmp(text, prefix, prefix_len) == 0) {
        text += prefix_len;
    }

    return parse_long(text, out);
}

static int path_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

static int write_text(const char *path, const char *text) {
    FILE *fp = fopen(path, "w");
    if (fp == NULL) {
        fprintf(stderr, "open %s failed: %s\n", path, strerror(errno));
        return -1;
    }

    if (fputs(text, fp) == EOF) {
        fprintf(stderr, "write %s failed: %s\n", path, strerror(errno));
        fclose(fp);
        return -1;
    }

    if (fclose(fp) != 0) {
        fprintf(stderr, "close %s failed: %s\n", path, strerror(errno));
        return -1;
    }

    return 0;
}

static int ensure_gpio_exported(long gpio_num, char *gpio_dir, size_t gpio_dir_size) {
    snprintf(gpio_dir, gpio_dir_size, GPIO_SYSFS_DIR "/gpio%ld", gpio_num);
    if (path_exists(gpio_dir)) {
        return 0;
    }

    char export_value[32];
    snprintf(export_value, sizeof(export_value), "%ld", gpio_num);
    if (write_text(GPIO_SYSFS_DIR "/export", export_value) != 0) {
        return -1;
    }

    for (int i = 0; i < 20; i++) {
        if (path_exists(gpio_dir)) {
            return 0;
        }
        usleep(25000);
    }

    fprintf(stderr, "%s did not appear after export\n", gpio_dir);
    return -1;
}

static int configure_input(const char *gpio_dir) {
    char direction_path[256];
    snprintf(direction_path, sizeof(direction_path), "%s/direction", gpio_dir);
    return write_text(direction_path, "in");
}

static int read_gpio_value(const char *gpio_dir, char *value, size_t value_size) {
    char value_path[256];
    snprintf(value_path, sizeof(value_path), "%s/value", gpio_dir);

    FILE *fp = fopen(value_path, "r");
    if (fp == NULL) {
        fprintf(stderr, "open %s failed: %s\n", value_path, strerror(errno));
        return -1;
    }

    if (fgets(value, (int)value_size, fp) == NULL) {
        fprintf(stderr, "read %s failed: %s\n", value_path, strerror(errno));
        fclose(fp);
        return -1;
    }

    fclose(fp);
    value[strcspn(value, "\r\n")] = '\0';
    return 0;
}

static void sleep_ms(long ms) {
    struct timespec req;
    req.tv_sec = ms / 1000;
    req.tv_nsec = (ms % 1000) * 1000000L;
    while (nanosleep(&req, &req) != 0 && errno == EINTR && keep_running) {
    }
}

int main(int argc, char **argv) {
    long gpio_num = -1;
    long chip_base = -1;
    long line = -1;
    long interval_ms = 200;
    bool watch = false;

    if (argc < 3) {
        usage(argv[0]);
        return 2;
    }

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--sysfs") == 0) {
            if (++i >= argc || parse_long(argv[i], &gpio_num) != 0) {
                fprintf(stderr, "--sysfs requires a non-negative GPIO number\n");
                return 2;
            }
        } else if (strcmp(argv[i], "--gpiochip") == 0) {
            if (++i >= argc || parse_gpiochip_base(argv[i], &chip_base) != 0) {
                fprintf(stderr, "--gpiochip requires gpiochipN or a non-negative chip base\n");
                return 2;
            }
        } else if (strcmp(argv[i], "--line") == 0) {
            if (++i >= argc || parse_long(argv[i], &line) != 0) {
                fprintf(stderr, "--line requires a non-negative line number\n");
                return 2;
            }
        } else if (strcmp(argv[i], "--watch") == 0) {
            watch = true;
        } else if (strcmp(argv[i], "--interval-ms") == 0) {
            if (++i >= argc || parse_long(argv[i], &interval_ms) != 0 ||
                interval_ms < 10 || interval_ms > 10000) {
                fprintf(stderr, "--interval-ms requires 10..10000\n");
                return 2;
            }
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "unknown argument: %s\n", argv[i]);
            usage(argv[0]);
            return 2;
        }
    }

    if (gpio_num >= 0 && (chip_base >= 0 || line >= 0)) {
        fprintf(stderr, "use either --sysfs or --gpiochip/--line, not both\n");
        return 2;
    }

    if (gpio_num < 0) {
        if (chip_base < 0 || line < 0) {
            fprintf(stderr, "missing GPIO selection: use --sysfs N or --gpiochip N --line M\n");
            return 2;
        }
        gpio_num = chip_base + line;
    }

    if (!path_exists(GPIO_SYSFS_DIR)) {
        fprintf(stderr, GPIO_SYSFS_DIR " does not exist; sysfs GPIO is unavailable\n");
        return 1;
    }

    char gpio_dir[256];
    if (ensure_gpio_exported(gpio_num, gpio_dir, sizeof(gpio_dir)) != 0) {
        return 1;
    }

    if (configure_input(gpio_dir) != 0) {
        return 1;
    }

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    unsigned long seq = 0;
    do {
        char value[16];
        if (read_gpio_value(gpio_dir, value, sizeof(value)) != 0) {
            return 1;
        }

        if (watch) {
            printf("seq=%lu gpio=%ld value=%s\n", seq++, gpio_num, value);
            fflush(stdout);
            sleep_ms(interval_ms);
        } else {
            printf("gpio=%ld value=%s\n", gpio_num, value);
        }
    } while (watch && keep_running);

    if (watch) {
        printf("stopped\n");
    }

    return 0;
}
