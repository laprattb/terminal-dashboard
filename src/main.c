#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>

#include "config.h"
#include "metrics.h"
#include "render.h"

static volatile int running = 1;

static void signal_handler(int sig) {
    (void)sig;
    running = 0;
}

static void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS]\n\n", program_name);
    printf("Options:\n");
    printf("  -c, --config FILE    Path to configuration file\n");
    printf("  -h, --help           Show this help message\n");
    printf("  -v, --version        Show version information\n");
    printf("\n");
    printf("Default config path: %s\n", config_get_default_path());
}

static void print_version(void) {
    printf("Terminal Dashboard v1.0.0\n");
    printf("A native terminal system monitor\n");
}

int main(int argc, char *argv[]) {
    config_t cfg;
    const char *config_path = NULL;

    /* Parse command line arguments */
    static struct option long_options[] = {
        {"config",  required_argument, 0, 'c'},
        {"help",    no_argument,       0, 'h'},
        {"version", no_argument,       0, 'v'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "c:hv", long_options, NULL)) != -1) {
        switch (opt) {
            case 'c':
                config_path = optarg;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            case 'v':
                print_version();
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    /* Initialize config with defaults */
    config_init_defaults(&cfg);

    /* Try to load config file */
    if (config_path) {
        if (!config_load(config_path, &cfg)) {
            fprintf(stderr, "Warning: Could not load config file: %s\n", config_path);
            fprintf(stderr, "Using default settings.\n");
            sleep(2);
        }
    } else {
        /* Try default config path */
        const char *default_path = config_get_default_path();
        config_load(default_path, &cfg);  /* Silently ignore if not found */
    }

    /* Set up signal handlers */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /* Initialize subsystems */
    if (!metrics_init()) {
        fprintf(stderr, "Error: Failed to initialize metrics subsystem\n");
        return 1;
    }

    render_init();

    /* Prepare disk mount points array */
    const char *mount_points[MAX_DISK_PATHS];
    for (int i = 0; i < cfg.disk_path_count; i++) {
        mount_points[i] = cfg.disk_paths[i];
    }

    /* Main loop */
    cpu_metrics_t cpu;
    memory_metrics_t mem;
    disk_metrics_list_t disks;

    while (running) {
        /* Collect metrics */
        bool have_cpu = cfg.show_cpu && metrics_get_cpu(&cpu);
        bool have_mem = cfg.show_memory && metrics_get_memory(&mem);
        bool have_disks = cfg.show_disk &&
                          metrics_get_disks(mount_points, cfg.disk_path_count, &disks);

        /* Render dashboard */
        render_dashboard(&cfg,
                         have_cpu ? &cpu : NULL,
                         have_mem ? &mem : NULL,
                         have_disks ? &disks : NULL);

        /* Sleep for refresh interval */
        usleep(cfg.refresh_ms * 1000);
    }

    /* Cleanup */
    render_cleanup();
    metrics_cleanup();

    printf("\nDashboard stopped.\n");
    return 0;
}
