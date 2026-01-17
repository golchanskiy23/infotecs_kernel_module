#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>

#define MODULE_NAME "test_module"
#define SYSFS_BASE "/sys/module/" MODULE_NAME "/parameters"
#define PARAM_FILENAME SYSFS_BASE "/filename"
#define PARAM_TIMER_PERIOD SYSFS_BASE "/timer_period"
#define MAX_PERIOD 3600
#define MIN_PERIOD 1
#define MAX_FILENAME_LEN (PATH_MAX - 1)
#define PERIOD_STR_BUF_SIZE 32

typedef struct {
    const char *filename;
    unsigned int period;
} module_params_t;

static void params_init(module_params_t *params)
{
    if (!params) {
        return;
    }
    params->filename = NULL;
    params->period = 0;
}

void print_usage(const char *prog_name)
{
    if (!prog_name) {
        prog_name = "set_params";
    }
    printf("Usage: sudo %s [OPTIONS]\n", prog_name);
    printf("\nNote: This program requires root privileges (sudo) to modify module parameters.\n");
    printf("\nOptions:\n");
    printf("  -f, --filename PATH    Set the log file path\n");
    printf("  -p, --period SECONDS   Set the timer period in seconds (1-3600)\n");
    printf("\nExamples:\n");
    printf("  sudo %s -p 1                    # Change timer period to 1 second\n", prog_name);
    printf("  sudo %s -f /var/tmp/test_module/log.txt -p 5\n", prog_name);
    printf("  sudo %s -f /var/tmp/test_module/log.txt -p 10\n", prog_name);
}

int validate_filepath(const char *filename)
{    
    size_t len;

    if (!filename) {
        fprintf(stderr, "Error: NULL file path\n");
        return -1;
    }

    len = strlen(filename);
    if (len == 0) {
        fprintf(stderr, "Error: Empty file path\n");
        return -1;
    }

    if (len > MAX_FILENAME_LEN) {
        fprintf(stderr, "Error: File path too long (max %d characters)\n",
                MAX_FILENAME_LEN);
        return -1;
    }

    if (strstr(filename, "..") != NULL) {
        fprintf(stderr, "Error: Invalid characters in file path (contains '..')\n");
        return -1;
    }

    return 0;
}

int write_sysfs_param(const char *param_path, const char *value)
{
    int fd;
    ssize_t written;
    size_t len;

    if (!param_path || !value) {
        fprintf(stderr, "Error: NULL parameter in write_sysfs_param\n");
        return -1;
    }

    fd = open(param_path, O_WRONLY);
    if (fd < 0) {
        if (errno == EACCES || errno == EPERM) {
            fprintf(stderr, "Permission denied: Failed to open sysfs parameter file %s\n", param_path);
            fprintf(stderr, "This program requires root privileges. Please run with sudo.\n");
        } else {
            fprintf(stderr, "Failed to open sysfs parameter file %s: %s\n",
                    param_path, strerror(errno));
        }
        return -1;
    }

    len = strlen(value);
    if (len == 0) {
        fprintf(stderr, "Error: Empty value for parameter\n");
        close(fd);
        return -1;
    }

    written = write(fd, value, len);
    if (written < 0) {
        fprintf(stderr, "Failed to write to sysfs parameter file %s: %s\n",
                param_path, strerror(errno));
        close(fd);
        return -1;
    }

    if (close(fd) != 0) {
        fprintf(stderr, "Warning: Failed to close file descriptor: %s\n",
                strerror(errno));
    }

    if ((size_t)written != len) {
        fprintf(stderr, "Warning: Partial write to sysfs parameter file "
                "(%zd of %zu bytes)\n", written, len);
        return -1;
    }

    return 0;
}

static int parse_period(const char *str, unsigned int *period_out)
{
    char *endptr;
    long period_long;

    if (!str || !period_out) {
        fprintf(stderr, "Error: NULL parameter in parse_period\n");
        return -1;
    }

    errno = 0;
    period_long = strtol(str, &endptr, 10);

    if (*endptr != '\0') {
        fprintf(stderr, "Error: Invalid period value (non-numeric characters): %s\n", str);
        return -1;
    }

    if (endptr == str) {
        fprintf(stderr, "Error: Empty period value\n");
        return -1;
    }

    if (errno == ERANGE) {
        fprintf(stderr, "Error: Period value out of range: %s\n", str);
        return -1;
    }

    if (period_long < 0) {
        fprintf(stderr, "Error: Period cannot be negative: %ld\n", period_long);
        return -1;
    }

    if (period_long > (long)UINT_MAX) {
        fprintf(stderr, "Error: Period value too large for unsigned int: %ld\n", period_long);
        return -1;
    }

    if (period_long < MIN_PERIOD || period_long > MAX_PERIOD) {
        fprintf(stderr, "Error: Period must be between %d and %d seconds (got %ld)\n",
                MIN_PERIOD, MAX_PERIOD, period_long);
        return -1;
    }

    *period_out = (unsigned int)period_long;
    return 0;
}

int main(int argc, char *argv[])
{
    module_params_t params;
    int ret = 0;

    params_init(&params);

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--filename") == 0) {
            if (i + 1 < argc) {
                params.filename = argv[++i];
            } else {
                fprintf(stderr, "Error: -f requires a file path\n");
                return 1;
            }
        } else if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--period") == 0) {
            if (i + 1 < argc) {
                if (parse_period(argv[++i], &params.period) != 0) {
                    return 1;
                }
            } else {
                fprintf(stderr, "Error: -p requires a period value\n");
                return 1;
            }
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    if (!params.filename && params.period == 0) {
        fprintf(stderr, "Error: At least one parameter (filename or period) must be specified\n");
        print_usage(argv[0]);
        return 1;
    }

    if (params.filename) {
        if (validate_filepath(params.filename) != 0) {
            return 1;
        }
        printf("Setting filename parameter to: %s\n", params.filename);
        if (write_sysfs_param(PARAM_FILENAME, params.filename) != 0) {
            fprintf(stderr, "Failed to set filename parameter\n");
            ret = 1;
            goto cleanup;
        }
        printf("Filename parameter set successfully\n");
    }

    if (params.period > 0) {
        char period_str[PERIOD_STR_BUF_SIZE];
        int n = snprintf(period_str, sizeof(period_str), "%u", params.period);
        if (n < 0 || (size_t)n >= sizeof(period_str)) {
            fprintf(stderr, "Error: Failed to format period string\n");
            ret = 1;
            goto cleanup;
        }

        printf("Setting timer_period parameter to: %u seconds\n", params.period);
        if (write_sysfs_param(PARAM_TIMER_PERIOD, period_str) != 0) {
            fprintf(stderr, "Failed to set timer_period parameter\n");
            ret = 1;
            goto cleanup;
        }
        printf("Timer period parameter set successfully\n");
    }

cleanup:
    return ret;
}