#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>

#define MAX_DISK_PATHS 16
#define MAX_PATH_LEN 256
#define MAX_TITLE_LEN 64

typedef enum {
    COLOR_DEFAULT = 0,
    COLOR_BLACK = 30,
    COLOR_RED = 31,
    COLOR_GREEN = 32,
    COLOR_YELLOW = 33,
    COLOR_BLUE = 34,
    COLOR_MAGENTA = 35,
    COLOR_CYAN = 36,
    COLOR_WHITE = 37
} color_t;

typedef struct {
    /* General settings */
    int refresh_ms;                     /* Refresh rate in milliseconds */
    char title[MAX_TITLE_LEN];          /* Dashboard title */

    /* Display toggles */
    bool show_cpu;
    bool show_memory;
    bool show_disk;

    /* Colors */
    color_t bar_color;
    color_t title_color;
    color_t label_color;
    color_t value_color;
    color_t warning_color;              /* Used when > 80% */
    color_t critical_color;             /* Used when > 90% */

    /* Thresholds */
    int warning_threshold;              /* Percentage to show warning color */
    int critical_threshold;             /* Percentage to show critical color */

    /* Disk paths to monitor */
    char disk_paths[MAX_DISK_PATHS][MAX_PATH_LEN];
    int disk_path_count;

    /* Bar style */
    char bar_fill_char;
    char bar_empty_char;
    int bar_width;
} config_t;

/* Initialize config with default values */
void config_init_defaults(config_t *cfg);

/* Load configuration from file, returns true on success */
bool config_load(const char *filename, config_t *cfg);

/* Get the default config file path */
const char* config_get_default_path(void);

#endif /* CONFIG_H */
