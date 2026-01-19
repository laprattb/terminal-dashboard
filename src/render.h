#ifndef RENDER_H
#define RENDER_H

#include "config.h"
#include "metrics.h"

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
                      const disk_metrics_list_t *disks);

/* Get terminal dimensions */
void render_get_terminal_size(int *width, int *height);

#endif /* RENDER_H */
