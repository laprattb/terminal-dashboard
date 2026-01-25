#include "render.h"
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <unistd.h>
#endif

/* History tracking for line graphs */
#define MAX_HISTORY 128

/* Use internal typedef that maps to public enum */
typedef render_history_type_t history_type_t;
#define HISTORY_CPU RENDER_HISTORY_CPU
#define HISTORY_MEMORY RENDER_HISTORY_MEMORY
#define HISTORY_GPU RENDER_HISTORY_GPU
#define HISTORY_GPU_MEM RENDER_HISTORY_GPU_MEM
#define HISTORY_COUNT RENDER_HISTORY_COUNT

static double history_data[HISTORY_COUNT][MAX_HISTORY];
static int history_count_arr[HISTORY_COUNT] = {0};
static int history_index[HISTORY_COUNT] = {0};

static void history_add(history_type_t type, double value) {
    history_data[type][history_index[type]] = value;
    history_index[type] = (history_index[type] + 1) % MAX_HISTORY;
    if (history_count_arr[type] < MAX_HISTORY) {
        history_count_arr[type]++;
    }
}

static double history_get(history_type_t type, int samples_ago) {
    if (samples_ago >= history_count_arr[type]) {
        return 0.0;
    }
    int idx = (history_index[type] - 1 - samples_ago + MAX_HISTORY) % MAX_HISTORY;
    return history_data[type][idx];
}

/* Public wrappers for testing */
void render_history_add(render_history_type_t type, double value) {
    history_add(type, value);
}

double render_history_get(render_history_type_t type, int samples_ago) {
    return history_get(type, samples_ago);
}

int render_history_count(render_history_type_t type) {
    return history_count_arr[type];
}

void render_history_clear(render_history_type_t type) {
    history_count_arr[type] = 0;
    history_index[type] = 0;
}

/* Unicode sparkline characters (8 levels) */
static const char *sparkline_chars[] = {
    "\xe2\x96\x81",  /* ▁ U+2581 */
    "\xe2\x96\x82",  /* ▂ U+2582 */
    "\xe2\x96\x83",  /* ▃ U+2583 */
    "\xe2\x96\x84",  /* ▄ U+2584 */
    "\xe2\x96\x85",  /* ▅ U+2585 */
    "\xe2\x96\x86",  /* ▆ U+2586 */
    "\xe2\x96\x87",  /* ▇ U+2587 */
    "\xe2\x96\x88"   /* █ U+2588 */
};

/* ANSI escape sequences */
#define ESC "\033"
#define CLEAR_SCREEN ESC "[2J"
#define CURSOR_HOME ESC "[H"
#define CLEAR_LINE ESC "[K"
#define CURSOR_HIDE ESC "[?25l"
#define CURSOR_SHOW ESC "[?25h"
#define RESET_COLOR ESC "[0m"
#define BOLD ESC "[1m"

#define LABEL_WIDTH 9
#define MIN_BAR_WIDTH 10
/* Fixed overhead: label(9) + space(1) + brackets(2) + spacing(2) + percent(6) + suffix(~28) */
#define LINE_OVERHEAD 48

int render_calculate_bar_width(int terminal_width) {
    int bar_width = terminal_width - LINE_OVERHEAD;
    if (bar_width < MIN_BAR_WIDTH) bar_width = MIN_BAR_WIDTH;
    return bar_width;
}

static int calculate_bar_width(void) {
    int term_width, term_height;
    render_get_terminal_size(&term_width, &term_height);
    return render_calculate_bar_width(term_width);
}

static void set_color(color_t color) {
    if (color != COLOR_DEFAULT) {
        printf(ESC "[%dm", color);
    }
}

static void reset_style(void) {
    printf(RESET_COLOR);
}

void render_init(void) {
#ifdef _WIN32
    /* Enable Virtual Terminal Processing for ANSI escape sequences on Windows 10+ */
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    if (hOut != INVALID_HANDLE_VALUE && GetConsoleMode(hOut, &dwMode)) {
        dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(hOut, dwMode);
    }
#endif
    /* Hide cursor and clear screen once at startup */
    printf(CURSOR_HIDE CLEAR_SCREEN CURSOR_HOME);
    fflush(stdout);
}

void render_cleanup(void) {
    /* Show cursor again */
    printf(CURSOR_SHOW);
    reset_style();
    fflush(stdout);
}

void render_clear(void) {
    /* Only move cursor home - don't clear screen to avoid flickering */
    printf(CURSOR_HOME);
    fflush(stdout);
}

void render_get_terminal_size(int *width, int *height) {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo(hOut, &csbi)) {
        *width = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        *height = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    } else {
        /* Default fallback */
        *width = 80;
        *height = 24;
    }
#else
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
        *width = ws.ws_col;
        *height = ws.ws_row;
    } else {
        /* Default fallback */
        *width = 80;
        *height = 24;
    }
#endif
}

static color_t get_threshold_color(const config_t *cfg, double percent) {
    if (percent >= cfg->critical_threshold) {
        return cfg->critical_color;
    } else if (percent >= cfg->warning_threshold) {
        return cfg->warning_color;
    }
    return cfg->bar_color;
}

static color_t get_temp_color(const config_t *cfg, int temp_celsius) {
    /* Temperature thresholds: normal < 70, warning 70-85, critical > 85 */
    if (temp_celsius > 85) {
        return cfg->critical_color;
    } else if (temp_celsius >= 70) {
        return cfg->warning_color;
    }
    return cfg->bar_color;
}

static void render_bar(const config_t *cfg, double percent, color_t color, int bar_width) {
    int filled = (int)(percent / 100.0 * bar_width);
    if (filled > bar_width) filled = bar_width;
    if (filled < 0) filled = 0;

    printf("[");
    set_color(color);

    for (int i = 0; i < bar_width; i++) {
        if (i < filled) {
            putchar(cfg->bar_fill_char);
        } else {
            putchar(cfg->bar_empty_char);
        }
    }

    reset_style();
    printf("]");
}

static void render_sparkline(const config_t *cfg, history_type_t type, int graph_width) {
    int samples = graph_width;
    if (samples > history_count_arr[type]) {
        samples = history_count_arr[type];
    }

    printf("[");

    /* Pad with spaces if not enough history */
    int padding = graph_width - samples;
    for (int i = 0; i < padding; i++) {
        printf(" ");
    }

    /* Render sparkline from oldest to newest */
    for (int i = samples - 1; i >= 0; i--) {
        double value = history_get(type, i);
        color_t color = get_threshold_color(cfg, value);

        /* Map 0-100% to 0-7 for sparkline character index */
        int level = (int)(value / 100.0 * 7.99);
        if (level < 0) level = 0;
        if (level > 7) level = 7;

        set_color(color);
        printf("%s", sparkline_chars[level]);
        reset_style();
    }

    printf("]");
}

static void render_graph(const config_t *cfg, double percent, color_t color,
                         int bar_width, history_type_t history_type) {
    if (cfg->graph_style == GRAPH_STYLE_LINE) {
        history_add(history_type, percent);
        render_sparkline(cfg, history_type, bar_width);
    } else {
        render_bar(cfg, percent, color, bar_width);
    }
}

static void render_title(const config_t *cfg) {
    int term_width, term_height;
    render_get_terminal_size(&term_width, &term_height);

    /* Center the title */
    int title_len = (int)strlen(cfg->title);
    int padding = (term_width - title_len - 4) / 2;
    if (padding < 0) padding = 0;

    printf(BOLD);
    set_color(cfg->title_color);

    for (int i = 0; i < padding; i++) putchar(' ');
    printf("[ %s ]", cfg->title);

    reset_style();
    printf(CLEAR_LINE "\n" CLEAR_LINE "\n");
}

static void render_cpu(const config_t *cfg, const cpu_metrics_t *cpu, int bar_width) {
    set_color(cfg->label_color);
    printf(BOLD "%-*s" RESET_COLOR " ", LABEL_WIDTH, "CPU");

    color_t bar_color = get_threshold_color(cfg, cpu->total_percent);
    render_graph(cfg, cpu->total_percent, bar_color, bar_width, HISTORY_CPU);

    printf("  ");
    set_color(cfg->value_color);
    printf("%5.1f%%", cpu->total_percent);
    reset_style();

    printf("  (usr: %.1f%% sys: %.1f%%)", cpu->user_percent, cpu->system_percent);

    /* Show CPU temperature if available and enabled */
    if (cfg->show_temperature && cpu->temperature_celsius >= 0) {
        printf("  ");
        set_color(get_temp_color(cfg, cpu->temperature_celsius));
        printf("%d%cC", cpu->temperature_celsius, 0xB0);  /* degree symbol */
        reset_style();
    }

    printf(CLEAR_LINE "\n");
}

static void render_memory(const config_t *cfg, const memory_metrics_t *mem, int bar_width) {
    char used_str[32], total_str[32];
    metrics_format_bytes(mem->used_bytes, used_str, sizeof(used_str));
    metrics_format_bytes(mem->total_bytes, total_str, sizeof(total_str));

    set_color(cfg->label_color);
    printf(BOLD "%-*s" RESET_COLOR " ", LABEL_WIDTH, "Memory");

    color_t bar_color = get_threshold_color(cfg, mem->used_percent);
    render_graph(cfg, mem->used_percent, bar_color, bar_width, HISTORY_MEMORY);

    printf("  ");
    set_color(cfg->value_color);
    printf("%5.1f%%", mem->used_percent);
    reset_style();

    printf("  (%s / %s)" CLEAR_LINE "\n", used_str, total_str);
}

static void render_gpu(const config_t *cfg, const gpu_metrics_t *gpu, int bar_width) {
    char used_str[32], total_str[32];

    /* Truncate GPU name if too long */
    char name_display[LABEL_WIDTH + 2];
    if (strlen(gpu->name) > LABEL_WIDTH) {
        strncpy(name_display, gpu->name, LABEL_WIDTH - 1);
        name_display[LABEL_WIDTH - 1] = '\0';
        strcat(name_display, "~");
    } else {
        strncpy(name_display, gpu->name, LABEL_WIDTH);
        name_display[LABEL_WIDTH] = '\0';
    }

    /* GPU utilization line */
    set_color(cfg->label_color);
    printf(BOLD "%-*s" RESET_COLOR " ", LABEL_WIDTH, "GPU");

    color_t bar_color = get_threshold_color(cfg, (double)gpu->utilization_percent);
    render_graph(cfg, (double)gpu->utilization_percent, bar_color, bar_width, HISTORY_GPU);

    printf("  ");
    set_color(cfg->value_color);
    printf("%5.1f%%", (double)gpu->utilization_percent);
    reset_style();

    /* Show GPU temperature if available and enabled */
    if (cfg->show_temperature && gpu->temperature_celsius >= 0) {
        printf("  ");
        set_color(get_temp_color(cfg, gpu->temperature_celsius));
        printf("%d%cC", gpu->temperature_celsius, 0xB0);  /* degree symbol */
        reset_style();
    }

    /* Show power if available */
    if (gpu->power_watts >= 0) {
        printf("  %dW", gpu->power_watts);
    }

    printf(CLEAR_LINE "\n");

    /* VRAM line */
    metrics_format_bytes(gpu->memory_used, used_str, sizeof(used_str));
    metrics_format_bytes(gpu->memory_total, total_str, sizeof(total_str));

    set_color(cfg->label_color);
    printf(BOLD "%-*s" RESET_COLOR " ", LABEL_WIDTH, "VRAM");

    bar_color = get_threshold_color(cfg, gpu->memory_percent);
    render_graph(cfg, gpu->memory_percent, bar_color, bar_width, HISTORY_GPU_MEM);

    printf("  ");
    set_color(cfg->value_color);
    printf("%5.1f%%", gpu->memory_percent);
    reset_style();

    printf("  (%s / %s)" CLEAR_LINE "\n", used_str, total_str);
}

static void render_disk(const config_t *cfg, const disk_metrics_t *disk, int bar_width) {
    char used_str[32], total_str[32];
    metrics_format_bytes(disk->used_bytes, used_str, sizeof(used_str));
    metrics_format_bytes(disk->total_bytes, total_str, sizeof(total_str));

    /* Truncate mount point if too long */
    char mount_display[LABEL_WIDTH + 2];
    if (strlen(disk->mount_point) > LABEL_WIDTH) {
        strncpy(mount_display, disk->mount_point, LABEL_WIDTH - 1);
        mount_display[LABEL_WIDTH - 1] = '\0';
        strcat(mount_display, "~");
    } else {
        strncpy(mount_display, disk->mount_point, LABEL_WIDTH);
        mount_display[LABEL_WIDTH] = '\0';
    }

    set_color(cfg->label_color);
    printf(BOLD "%-*s" RESET_COLOR " ", LABEL_WIDTH, mount_display);

    color_t bar_color = get_threshold_color(cfg, disk->used_percent);
    render_bar(cfg, disk->used_percent, bar_color, bar_width);

    printf("  ");
    set_color(cfg->value_color);
    printf("%5.1f%%", disk->used_percent);
    reset_style();

    printf("  (%s / %s)" CLEAR_LINE "\n", used_str, total_str);
}

static void render_separator(void) {
    printf(CLEAR_LINE "\n");
}

static void render_footer(void) {
    printf(CLEAR_LINE "\n");
    set_color(COLOR_WHITE);
    printf("Press Ctrl+C to exit");
    reset_style();
    printf(CLEAR_LINE "\n");
}

void render_dashboard(const config_t *cfg,
                      const cpu_metrics_t *cpu,
                      const memory_metrics_t *mem,
                      const disk_metrics_list_t *disks,
                      const gpu_metrics_t *gpu) {
    render_clear();
    render_title(cfg);

    int bar_width = calculate_bar_width();

    if (cfg->show_cpu && cpu) {
        render_cpu(cfg, cpu, bar_width);
    }

    if (cfg->show_memory && mem) {
        render_memory(cfg, mem, bar_width);
    }

    /* GPU section */
    if (cfg->show_gpu && gpu && gpu->available) {
        if (cfg->show_cpu || cfg->show_memory) {
            render_separator();
        }
        render_gpu(cfg, gpu, bar_width);
    }

    /* Disk section */
    if (cfg->show_disk && disks && disks->count > 0) {
        if (cfg->show_cpu || cfg->show_memory || (cfg->show_gpu && gpu && gpu->available)) {
            render_separator();
        }
        for (int i = 0; i < disks->count; i++) {
            render_disk(cfg, &disks->disks[i], bar_width);
        }
    }

    render_footer();
    fflush(stdout);
}
