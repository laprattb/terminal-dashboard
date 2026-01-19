#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#ifdef _MSC_VER
/* MSVC-specific CRT debug heap */
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#pragma comment(lib, "psapi.lib")
#endif
#endif

#include "../src/config.h"
#include "../src/metrics.h"
#include "../src/render.h"

/* Number of iterations to run each test */
#define LEAK_TEST_ITERATIONS 100

/* Simple test framework */
static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) static void name(void)
#define RUN_TEST(name) do { \
    printf("  Running %s... ", #name); \
    fflush(stdout); \
    tests_run++; \
    name(); \
    tests_passed++; \
    printf("PASSED\n"); \
} while(0)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf("FAILED\n    Assertion failed: %s\n    at %s:%d\n", #cond, __FILE__, __LINE__); \
        exit(1); \
    } \
} while(0)

#ifdef _WIN32
/* Get current memory usage on Windows */
static SIZE_T get_current_memory_usage(void) {
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return pmc.WorkingSetSize;
    }
    return 0;
}
#else
/* Placeholder for non-Windows platforms */
static size_t get_current_memory_usage(void) {
    return 0;
}
#endif

/* Test: config functions don't leak memory */
TEST(test_config_no_leaks) {
    config_t cfg;
    SIZE_T initial_mem = get_current_memory_usage();

    for (int i = 0; i < LEAK_TEST_ITERATIONS; i++) {
        config_init_defaults(&cfg);

        /* Verify defaults are set correctly */
        ASSERT(cfg.refresh_ms == 1000);
        ASSERT(cfg.show_cpu == true);
        ASSERT(cfg.show_memory == true);
        ASSERT(cfg.show_disk == true);
    }

    SIZE_T final_mem = get_current_memory_usage();

    /* Allow for some variance (up to 1MB) as working set can fluctuate */
    SIZE_T max_allowed_growth = 1024 * 1024;
    ASSERT(final_mem < initial_mem + max_allowed_growth);
}

/* Test: config_load doesn't leak on nonexistent file */
TEST(test_config_load_nonexistent_no_leaks) {
    config_t cfg;
    SIZE_T initial_mem = get_current_memory_usage();

    for (int i = 0; i < LEAK_TEST_ITERATIONS; i++) {
        config_init_defaults(&cfg);
        /* Load from a file that doesn't exist - should return false, not leak */
        bool result = config_load("nonexistent_config_file_12345.ini", &cfg);
        ASSERT(result == false);
    }

    SIZE_T final_mem = get_current_memory_usage();
    SIZE_T max_allowed_growth = 1024 * 1024;
    ASSERT(final_mem < initial_mem + max_allowed_growth);
}

/* Test: metrics init/cleanup cycle doesn't leak */
TEST(test_metrics_init_cleanup_no_leaks) {
    SIZE_T initial_mem = get_current_memory_usage();

    for (int i = 0; i < LEAK_TEST_ITERATIONS; i++) {
        bool init_ok = metrics_init();
        ASSERT(init_ok == true);
        metrics_cleanup();
    }

    SIZE_T final_mem = get_current_memory_usage();
    SIZE_T max_allowed_growth = 1024 * 1024;
    ASSERT(final_mem < initial_mem + max_allowed_growth);
}

/* Test: repeated CPU metrics collection doesn't leak */
TEST(test_cpu_metrics_no_leaks) {
    cpu_metrics_t cpu;
    SIZE_T initial_mem = get_current_memory_usage();

    ASSERT(metrics_init() == true);

    for (int i = 0; i < LEAK_TEST_ITERATIONS; i++) {
        bool result = metrics_get_cpu(&cpu);
        ASSERT(result == true);

        /* Sanity check: percentages should be in valid range */
        ASSERT(cpu.total_percent >= 0.0 && cpu.total_percent <= 100.0);
        ASSERT(cpu.user_percent >= 0.0 && cpu.user_percent <= 100.0);
        ASSERT(cpu.system_percent >= 0.0 && cpu.system_percent <= 100.0);
    }

    metrics_cleanup();

    SIZE_T final_mem = get_current_memory_usage();
    SIZE_T max_allowed_growth = 1024 * 1024;
    ASSERT(final_mem < initial_mem + max_allowed_growth);
}

/* Test: repeated memory metrics collection doesn't leak */
TEST(test_memory_metrics_no_leaks) {
    memory_metrics_t mem;
    SIZE_T initial_mem = get_current_memory_usage();

    ASSERT(metrics_init() == true);

    for (int i = 0; i < LEAK_TEST_ITERATIONS; i++) {
        bool result = metrics_get_memory(&mem);
        ASSERT(result == true);

        /* Sanity check: values should be positive */
        ASSERT(mem.total_bytes > 0);
        ASSERT(mem.used_percent >= 0.0 && mem.used_percent <= 100.0);
    }

    metrics_cleanup();

    SIZE_T final_mem = get_current_memory_usage();
    SIZE_T max_allowed_growth = 1024 * 1024;
    ASSERT(final_mem < initial_mem + max_allowed_growth);
}

/* Test: repeated disk metrics collection doesn't leak */
TEST(test_disk_metrics_no_leaks) {
    disk_metrics_t disk;
    SIZE_T initial_mem = get_current_memory_usage();

    ASSERT(metrics_init() == true);

#ifdef _WIN32
    const char *test_path = "C:\\";
#else
    const char *test_path = "/";
#endif

    for (int i = 0; i < LEAK_TEST_ITERATIONS; i++) {
        bool result = metrics_get_disk(test_path, &disk);
        ASSERT(result == true);

        /* Sanity check: values should be positive */
        ASSERT(disk.total_bytes > 0);
        ASSERT(disk.used_percent >= 0.0 && disk.used_percent <= 100.0);
    }

    metrics_cleanup();

    SIZE_T final_mem = get_current_memory_usage();
    SIZE_T max_allowed_growth = 1024 * 1024;
    ASSERT(final_mem < initial_mem + max_allowed_growth);
}

/* Test: repeated multi-disk metrics collection doesn't leak */
TEST(test_multi_disk_metrics_no_leaks) {
    disk_metrics_list_t disks;
    SIZE_T initial_mem = get_current_memory_usage();

    ASSERT(metrics_init() == true);

#ifdef _WIN32
    const char *paths[] = {"C:\\"};
#else
    const char *paths[] = {"/"};
#endif
    int path_count = 1;

    for (int i = 0; i < LEAK_TEST_ITERATIONS; i++) {
        bool result = metrics_get_disks(paths, path_count, &disks);
        ASSERT(result == true);
        ASSERT(disks.count > 0);
    }

    metrics_cleanup();

    SIZE_T final_mem = get_current_memory_usage();
    SIZE_T max_allowed_growth = 1024 * 1024;
    ASSERT(final_mem < initial_mem + max_allowed_growth);
}

/* Test: metrics_format_bytes doesn't leak */
TEST(test_format_bytes_no_leaks) {
    char buf[64];
    SIZE_T initial_mem = get_current_memory_usage();

    uint64_t test_values[] = {
        0, 512, 1024, 1024 * 1024,
        1024ULL * 1024 * 1024,
        1024ULL * 1024 * 1024 * 1024
    };
    int num_values = sizeof(test_values) / sizeof(test_values[0]);

    for (int i = 0; i < LEAK_TEST_ITERATIONS; i++) {
        for (int j = 0; j < num_values; j++) {
            metrics_format_bytes(test_values[j], buf, sizeof(buf));
            ASSERT(strlen(buf) > 0);
        }
    }

    SIZE_T final_mem = get_current_memory_usage();
    SIZE_T max_allowed_growth = 1024 * 1024;
    ASSERT(final_mem < initial_mem + max_allowed_growth);
}

/* Test: full metrics cycle (init -> collect all -> cleanup) doesn't leak */
TEST(test_full_metrics_cycle_no_leaks) {
    SIZE_T initial_mem = get_current_memory_usage();

    for (int i = 0; i < LEAK_TEST_ITERATIONS / 10; i++) {
        cpu_metrics_t cpu;
        memory_metrics_t mem;
        disk_metrics_list_t disks;

        ASSERT(metrics_init() == true);

#ifdef _WIN32
        const char *paths[] = {"C:\\"};
#else
        const char *paths[] = {"/"};
#endif

        /* Collect all metrics types */
        metrics_get_cpu(&cpu);
        metrics_get_memory(&mem);
        metrics_get_disks(paths, 1, &disks);

        metrics_cleanup();
    }

    SIZE_T final_mem = get_current_memory_usage();
    SIZE_T max_allowed_growth = 2 * 1024 * 1024; /* Allow 2MB for full cycle */
    ASSERT(final_mem < initial_mem + max_allowed_growth);
}

#ifdef _MSC_VER
/* MSVC-specific: Use CRT debug heap to detect leaks */
static int check_crt_memory_leaks(void) {
    _CrtMemState state1, state2, diff;

    _CrtMemCheckpoint(&state1);

    /* Run a sample of operations that might leak */
    config_t cfg;
    config_init_defaults(&cfg);

    metrics_init();
    cpu_metrics_t cpu;
    memory_metrics_t mem;
    disk_metrics_t disk;

    metrics_get_cpu(&cpu);
    metrics_get_memory(&mem);
    metrics_get_disk("C:\\", &disk);

    metrics_cleanup();

    _CrtMemCheckpoint(&state2);

    if (_CrtMemDifference(&diff, &state1, &state2)) {
        printf("  CRT Memory difference detected:\n");
        _CrtMemDumpStatistics(&diff);
        return 1; /* Leak detected */
    }

    return 0; /* No leak */
}

TEST(test_crt_heap_no_leaks) {
    int leak_detected = check_crt_memory_leaks();
    ASSERT(leak_detected == 0);
}
#endif

int main(void) {
#ifdef _MSC_VER
    /* Enable CRT debug heap (MSVC only) */
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    printf("Running memory leak tests...\n\n");

    printf("Config memory tests:\n");
    RUN_TEST(test_config_no_leaks);
    RUN_TEST(test_config_load_nonexistent_no_leaks);

    printf("\nMetrics memory tests:\n");
    RUN_TEST(test_metrics_init_cleanup_no_leaks);
    RUN_TEST(test_cpu_metrics_no_leaks);
    RUN_TEST(test_memory_metrics_no_leaks);
    RUN_TEST(test_disk_metrics_no_leaks);
    RUN_TEST(test_multi_disk_metrics_no_leaks);
    RUN_TEST(test_format_bytes_no_leaks);
    RUN_TEST(test_full_metrics_cycle_no_leaks);

#ifdef _MSC_VER
    printf("\nCRT heap leak detection:\n");
    RUN_TEST(test_crt_heap_no_leaks);
#endif

    printf("\n========================================\n");
    printf("Memory leak tests: %d passed, %d total\n", tests_passed, tests_run);
    printf("========================================\n");

#ifdef _MSC_VER
    /* Final leak check - will output to debug console if leaks found */
    if (_CrtDumpMemoryLeaks()) {
        printf("\nWARNING: CRT detected memory leaks! Check debug output.\n");
        return 1;
    }
#endif

    return 0;
}
