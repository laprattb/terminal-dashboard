#ifndef METRICS_GPU_H
#define METRICS_GPU_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    char name[128];
    int utilization_percent;      /* GPU core usage */
    uint64_t memory_total;
    uint64_t memory_used;
    double memory_percent;
    int temperature_celsius;      /* GPU temp */
    int power_watts;              /* Current power draw */
    bool available;               /* Whether GPU was detected */
} gpu_metrics_t;

/* Initialize GPU metrics subsystem (call once at startup) */
bool gpu_metrics_init(void);

/* Cleanup GPU metrics subsystem (call once at shutdown) */
void gpu_metrics_cleanup(void);

/* Get current GPU metrics, returns true if GPU available */
bool gpu_metrics_get(gpu_metrics_t *gpu);

#endif /* METRICS_GPU_H */
