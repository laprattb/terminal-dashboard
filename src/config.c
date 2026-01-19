#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static char default_config_path[MAX_PATH_LEN] = "";

void config_init_defaults(config_t *cfg) {
    cfg->refresh_ms = 1000;
    strncpy(cfg->title, "System Dashboard", MAX_TITLE_LEN - 1);
    cfg->title[MAX_TITLE_LEN - 1] = '\0';

    cfg->show_cpu = true;
    cfg->show_memory = true;
    cfg->show_disk = true;

    cfg->bar_color = COLOR_GREEN;
    cfg->title_color = COLOR_CYAN;
    cfg->label_color = COLOR_WHITE;
    cfg->value_color = COLOR_DEFAULT;
    cfg->warning_color = COLOR_YELLOW;
    cfg->critical_color = COLOR_RED;

    cfg->warning_threshold = 80;
    cfg->critical_threshold = 90;

    /* Default disk path */
    strncpy(cfg->disk_paths[0], "/", MAX_PATH_LEN - 1);
    cfg->disk_paths[0][MAX_PATH_LEN - 1] = '\0';
    cfg->disk_path_count = 1;

    cfg->bar_fill_char = '#';
    cfg->bar_empty_char = '-';
    cfg->bar_width = 30;
}

/* Trim leading and trailing whitespace in place */
static char* trim(char *str) {
    char *end;

    /* Trim leading space */
    while (isspace((unsigned char)*str)) str++;

    if (*str == 0) return str;

    /* Trim trailing space */
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;

    /* Write new null terminator */
    end[1] = '\0';

    return str;
}

/* Parse a color name to color_t */
static color_t parse_color(const char *value) {
    if (strcmp(value, "black") == 0) return COLOR_BLACK;
    if (strcmp(value, "red") == 0) return COLOR_RED;
    if (strcmp(value, "green") == 0) return COLOR_GREEN;
    if (strcmp(value, "yellow") == 0) return COLOR_YELLOW;
    if (strcmp(value, "blue") == 0) return COLOR_BLUE;
    if (strcmp(value, "magenta") == 0) return COLOR_MAGENTA;
    if (strcmp(value, "cyan") == 0) return COLOR_CYAN;
    if (strcmp(value, "white") == 0) return COLOR_WHITE;
    return COLOR_DEFAULT;
}

/* Parse a boolean value */
static bool parse_bool(const char *value) {
    return (strcmp(value, "true") == 0 ||
            strcmp(value, "yes") == 0 ||
            strcmp(value, "1") == 0);
}

bool config_load(const char *filename, config_t *cfg) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        return false;
    }

    char line[512];
    char current_section[64] = "";

    while (fgets(line, sizeof(line), fp)) {
        char *trimmed = trim(line);

        /* Skip empty lines and comments */
        if (trimmed[0] == '\0' || trimmed[0] == '#' || trimmed[0] == ';') {
            continue;
        }

        /* Check for section header */
        if (trimmed[0] == '[') {
            char *end = strchr(trimmed, ']');
            if (end) {
                *end = '\0';
                strncpy(current_section, trimmed + 1, sizeof(current_section) - 1);
                current_section[sizeof(current_section) - 1] = '\0';
            }
            continue;
        }

        /* Parse key=value */
        char *equals = strchr(trimmed, '=');
        if (!equals) continue;

        *equals = '\0';
        char *key = trim(trimmed);
        char *value = trim(equals + 1);

        /* Remove quotes from value if present */
        size_t vlen = strlen(value);
        if (vlen >= 2 && ((value[0] == '"' && value[vlen-1] == '"') ||
                          (value[0] == '\'' && value[vlen-1] == '\''))) {
            value[vlen-1] = '\0';
            value++;
        }

        /* Process based on section */
        if (strcmp(current_section, "general") == 0) {
            if (strcmp(key, "refresh_ms") == 0) {
                cfg->refresh_ms = atoi(value);
                if (cfg->refresh_ms < 100) cfg->refresh_ms = 100;
            } else if (strcmp(key, "title") == 0) {
                strncpy(cfg->title, value, MAX_TITLE_LEN - 1);
                cfg->title[MAX_TITLE_LEN - 1] = '\0';
            }
        } else if (strcmp(current_section, "display") == 0) {
            if (strcmp(key, "show_cpu") == 0) {
                cfg->show_cpu = parse_bool(value);
            } else if (strcmp(key, "show_memory") == 0) {
                cfg->show_memory = parse_bool(value);
            } else if (strcmp(key, "show_disk") == 0) {
                cfg->show_disk = parse_bool(value);
            }
        } else if (strcmp(current_section, "colors") == 0) {
            if (strcmp(key, "bar") == 0) {
                cfg->bar_color = parse_color(value);
            } else if (strcmp(key, "title") == 0) {
                cfg->title_color = parse_color(value);
            } else if (strcmp(key, "label") == 0) {
                cfg->label_color = parse_color(value);
            } else if (strcmp(key, "value") == 0) {
                cfg->value_color = parse_color(value);
            } else if (strcmp(key, "warning") == 0) {
                cfg->warning_color = parse_color(value);
            } else if (strcmp(key, "critical") == 0) {
                cfg->critical_color = parse_color(value);
            }
        } else if (strcmp(current_section, "thresholds") == 0) {
            if (strcmp(key, "warning") == 0) {
                cfg->warning_threshold = atoi(value);
            } else if (strcmp(key, "critical") == 0) {
                cfg->critical_threshold = atoi(value);
            }
        } else if (strcmp(current_section, "disks") == 0) {
            if (strcmp(key, "path") == 0 && cfg->disk_path_count < MAX_DISK_PATHS) {
                strncpy(cfg->disk_paths[cfg->disk_path_count], value, MAX_PATH_LEN - 1);
                cfg->disk_paths[cfg->disk_path_count][MAX_PATH_LEN - 1] = '\0';
                cfg->disk_path_count++;
            }
        } else if (strcmp(current_section, "style") == 0) {
            if (strcmp(key, "bar_fill") == 0 && strlen(value) > 0) {
                cfg->bar_fill_char = value[0];
            } else if (strcmp(key, "bar_empty") == 0 && strlen(value) > 0) {
                cfg->bar_empty_char = value[0];
            } else if (strcmp(key, "bar_width") == 0) {
                cfg->bar_width = atoi(value);
                if (cfg->bar_width < 10) cfg->bar_width = 10;
                if (cfg->bar_width > 80) cfg->bar_width = 80;
            }
        }
    }

    fclose(fp);

    /* If disks were specified in config, reset the count properly */
    /* The default "/" is at index 0, config adds starting at that index */
    /* So we need to handle this - if config specified paths, they override default */
    if (cfg->disk_path_count > 1) {
        /* Shift paths down, removing the default "/" */
        for (int i = 0; i < cfg->disk_path_count - 1; i++) {
            strncpy(cfg->disk_paths[i], cfg->disk_paths[i + 1], MAX_PATH_LEN);
        }
        cfg->disk_path_count--;
    }

    return true;
}

const char* config_get_default_path(void) {
    if (default_config_path[0] == '\0') {
        const char *home = getenv("HOME");
        if (home) {
            snprintf(default_config_path, MAX_PATH_LEN,
                     "%s/.config/dashboard/config.ini", home);
        } else {
            strncpy(default_config_path, "./dashboard.ini", MAX_PATH_LEN - 1);
        }
    }
    return default_config_path;
}
