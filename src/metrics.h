#ifndef METRICS_H
#define METRICS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define MAX_DISKS 16
#define MAX_PATH_LEN 256

typedef struct {
    double user_percent;
    double system_percent;
    double idle_percent;
    double total_percent;  /* user + system */
} cpu_metrics_t;

typedef struct {
    uint64_t total_bytes;
    uint64_t used_bytes;
    uint64_t free_bytes;
    double used_percent;
} memory_metrics_t;

typedef struct {
    char mount_point[MAX_PATH_LEN];
    uint64_t total_bytes;
    uint64_t used_bytes;
    uint64_t free_bytes;
    double used_percent;
} disk_metrics_t;

typedef struct {
    disk_metrics_t disks[MAX_DISKS];
    int count;
} disk_metrics_list_t;

/* Initialize metrics subsystem (call once at startup) */
bool metrics_init(void);

/* Cleanup metrics subsystem (call once at shutdown) */
void metrics_cleanup(void);

/* Get current CPU usage (requires two calls with delay for delta) */
bool metrics_get_cpu(cpu_metrics_t *cpu);

/* Get current memory usage */
bool metrics_get_memory(memory_metrics_t *mem);

/* Get disk usage for a specific mount point */
bool metrics_get_disk(const char *mount_point, disk_metrics_t *disk);

/* Get disk usage for multiple mount points */
bool metrics_get_disks(const char **mount_points, int count, disk_metrics_list_t *disks);

/* Helper to format bytes as human-readable string */
void metrics_format_bytes(uint64_t bytes, char *buf, size_t buf_size);

#endif /* METRICS_H */
