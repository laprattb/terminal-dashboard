#ifndef RENDER_H
#define RENDER_H

#include "config.h"
#include "metrics.h"
#include "metrics_gpu.h"

/* Initialize the terminal for dashboard rendering */
void render_init(void);

/* Cleanup terminal state (restore cursor, etc.) */
void render_cleanup(void);

/* Clear screen and move cursor to home */
void render_clear(void);

/* Render the complete dashboard */
void render_dashboard(const config_t *cfg,
                      const cpu_metrics_t *cpu,
                      const memory_metrics_t *mem,
                      const disk_metrics_list_t *disks,
                      const gpu_metrics_t *gpu);

/* Get terminal dimensions */
void render_get_terminal_size(int *width, int *height);

/* Calculate dynamic bar width based on terminal size (exposed for testing) */
int render_calculate_bar_width(int terminal_width);

/* History types for line graph (exposed for testing) */
typedef enum {
    RENDER_HISTORY_CPU = 0,
    RENDER_HISTORY_MEMORY,
    RENDER_HISTORY_GPU,
    RENDER_HISTORY_GPU_MEM,
    RENDER_HISTORY_COUNT
} render_history_type_t;

/* History functions (exposed for testing) */
void render_history_add(render_history_type_t type, double value);
double render_history_get(render_history_type_t type, int samples_ago);
int render_history_count(render_history_type_t type);
void render_history_clear(render_history_type_t type);

#endif /* RENDER_H */
