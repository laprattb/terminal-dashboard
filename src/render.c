#include "render.h"
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

/* ANSI escape sequences */
#define ESC "\033"
#define CLEAR_SCREEN ESC "[2J"
#define CURSOR_HOME ESC "[H"
#define CURSOR_HIDE ESC "[?25l"
#define CURSOR_SHOW ESC "[?25h"
#define RESET_COLOR ESC "[0m"
#define BOLD ESC "[1m"

static void set_color(color_t color) {
    if (color != COLOR_DEFAULT) {
        printf(ESC "[%dm", color);
    }
}

static void reset_style(void) {
    printf(RESET_COLOR);
}

void render_init(void) {
    /* Hide cursor for cleaner display */
    printf(CURSOR_HIDE);
    fflush(stdout);
}

void render_cleanup(void) {
    /* Show cursor again */
    printf(CURSOR_SHOW);
    reset_style();
    fflush(stdout);
}

void render_clear(void) {
    printf(CLEAR_SCREEN CURSOR_HOME);
    fflush(stdout);
}

void render_get_terminal_size(int *width, int *height) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
        *width = ws.ws_col;
        *height = ws.ws_row;
    } else {
        /* Default fallback */
        *width = 80;
        *height = 24;
    }
}

static color_t get_threshold_color(const config_t *cfg, double percent) {
    if (percent >= cfg->critical_threshold) {
        return cfg->critical_color;
    } else if (percent >= cfg->warning_threshold) {
        return cfg->warning_color;
    }
    return cfg->bar_color;
}

static void render_bar(const config_t *cfg, double percent, color_t color) {
    int filled = (int)(percent / 100.0 * cfg->bar_width);
    if (filled > cfg->bar_width) filled = cfg->bar_width;
    if (filled < 0) filled = 0;

    printf("[");
    set_color(color);

    for (int i = 0; i < cfg->bar_width; i++) {
        if (i < filled) {
            putchar(cfg->bar_fill_char);
        } else {
            putchar(cfg->bar_empty_char);
        }
    }

    reset_style();
    printf("]");
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
    printf("\n\n");
}

static void render_cpu(const config_t *cfg, const cpu_metrics_t *cpu) {
    set_color(cfg->label_color);
    printf(BOLD "CPU" RESET_COLOR "      ");

    color_t bar_color = get_threshold_color(cfg, cpu->total_percent);
    render_bar(cfg, cpu->total_percent, bar_color);

    printf("  ");
    set_color(cfg->value_color);
    printf("%5.1f%%", cpu->total_percent);
    reset_style();

    printf("  (usr: %.1f%% sys: %.1f%%)\n",
           cpu->user_percent, cpu->system_percent);
}

static void render_memory(const config_t *cfg, const memory_metrics_t *mem) {
    char used_str[32], total_str[32];
    metrics_format_bytes(mem->used_bytes, used_str, sizeof(used_str));
    metrics_format_bytes(mem->total_bytes, total_str, sizeof(total_str));

    set_color(cfg->label_color);
    printf(BOLD "Memory" RESET_COLOR "   ");

    color_t bar_color = get_threshold_color(cfg, mem->used_percent);
    render_bar(cfg, mem->used_percent, bar_color);

    printf("  ");
    set_color(cfg->value_color);
    printf("%5.1f%%", mem->used_percent);
    reset_style();

    printf("  (%s / %s)\n", used_str, total_str);
}

static void render_disk(const config_t *cfg, const disk_metrics_t *disk) {
    char used_str[32], total_str[32];
    metrics_format_bytes(disk->used_bytes, used_str, sizeof(used_str));
    metrics_format_bytes(disk->total_bytes, total_str, sizeof(total_str));

    /* Truncate mount point if too long */
    char mount_display[12];
    if (strlen(disk->mount_point) > 10) {
        strncpy(mount_display, disk->mount_point, 9);
        mount_display[9] = '\0';
        strcat(mount_display, "~");
    } else {
        strncpy(mount_display, disk->mount_point, sizeof(mount_display) - 1);
        mount_display[sizeof(mount_display) - 1] = '\0';
    }

    set_color(cfg->label_color);
    printf(BOLD "%-10s" RESET_COLOR " ", mount_display);

    color_t bar_color = get_threshold_color(cfg, disk->used_percent);
    render_bar(cfg, disk->used_percent, bar_color);

    printf("  ");
    set_color(cfg->value_color);
    printf("%5.1f%%", disk->used_percent);
    reset_style();

    printf("  (%s / %s)\n", used_str, total_str);
}

static void render_separator(void) {
    printf("\n");
}

static void render_footer(void) {
    printf("\n");
    set_color(COLOR_WHITE);
    printf("Press Ctrl+C to exit");
    reset_style();
    printf("\n");
}

void render_dashboard(const config_t *cfg,
                      const cpu_metrics_t *cpu,
                      const memory_metrics_t *mem,
                      const disk_metrics_list_t *disks) {
    render_clear();
    render_title(cfg);

    if (cfg->show_cpu && cpu) {
        render_cpu(cfg, cpu);
    }

    if (cfg->show_memory && mem) {
        render_memory(cfg, mem);
    }

    if (cfg->show_disk && disks && disks->count > 0) {
        if (cfg->show_cpu || cfg->show_memory) {
            render_separator();
        }
        for (int i = 0; i < disks->count; i++) {
            render_disk(cfg, &disks->disks[i]);
        }
    }

    render_footer();
    fflush(stdout);
}
