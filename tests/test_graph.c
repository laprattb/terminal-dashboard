#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../src/config.h"
#include "../src/render.h"

/* Simple test framework */
static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) static void name(void)
#define RUN_TEST(name) do { \
    printf("  Running %s... ", #name); \
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

#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        printf("FAILED\n    Expected %d but got %d\n    at %s:%d\n", (int)(b), (int)(a), __FILE__, __LINE__); \
        exit(1); \
    } \
} while(0)

#define ASSERT_DOUBLE_EQ(a, b) do { \
    if (fabs((a) - (b)) > 0.001) { \
        printf("FAILED\n    Expected %.3f but got %.3f\n    at %s:%d\n", (double)(b), (double)(a), __FILE__, __LINE__); \
        exit(1); \
    } \
} while(0)

/* ==================== Config Tests ==================== */

/* Test: default graph style is bar */
TEST(test_config_default_graph_style) {
    config_t cfg;
    config_init_defaults(&cfg);
    ASSERT_EQ(cfg.graph_style, GRAPH_STYLE_BAR);
}

/* Test: graph style enum values */
TEST(test_graph_style_enum_values) {
    ASSERT_EQ(GRAPH_STYLE_BAR, 0);
    ASSERT_EQ(GRAPH_STYLE_LINE, 1);
}

/* ==================== History Tests ==================== */

/* Test: history starts empty */
TEST(test_history_starts_empty) {
    render_history_clear(RENDER_HISTORY_CPU);
    ASSERT_EQ(render_history_count(RENDER_HISTORY_CPU), 0);
}

/* Test: adding one value to history */
TEST(test_history_add_one) {
    render_history_clear(RENDER_HISTORY_CPU);
    render_history_add(RENDER_HISTORY_CPU, 50.0);
    ASSERT_EQ(render_history_count(RENDER_HISTORY_CPU), 1);
    ASSERT_DOUBLE_EQ(render_history_get(RENDER_HISTORY_CPU, 0), 50.0);
}

/* Test: adding multiple values to history */
TEST(test_history_add_multiple) {
    render_history_clear(RENDER_HISTORY_CPU);
    render_history_add(RENDER_HISTORY_CPU, 10.0);
    render_history_add(RENDER_HISTORY_CPU, 20.0);
    render_history_add(RENDER_HISTORY_CPU, 30.0);

    ASSERT_EQ(render_history_count(RENDER_HISTORY_CPU), 3);

    /* Most recent (0 samples ago) should be 30.0 */
    ASSERT_DOUBLE_EQ(render_history_get(RENDER_HISTORY_CPU, 0), 30.0);
    /* 1 sample ago should be 20.0 */
    ASSERT_DOUBLE_EQ(render_history_get(RENDER_HISTORY_CPU, 1), 20.0);
    /* 2 samples ago should be 10.0 */
    ASSERT_DOUBLE_EQ(render_history_get(RENDER_HISTORY_CPU, 2), 10.0);
}

/* Test: history get returns 0 for out of range */
TEST(test_history_get_out_of_range) {
    render_history_clear(RENDER_HISTORY_CPU);
    render_history_add(RENDER_HISTORY_CPU, 50.0);

    /* Should return 0.0 for samples beyond what we have */
    ASSERT_DOUBLE_EQ(render_history_get(RENDER_HISTORY_CPU, 1), 0.0);
    ASSERT_DOUBLE_EQ(render_history_get(RENDER_HISTORY_CPU, 100), 0.0);
}

/* Test: history types are independent */
TEST(test_history_types_independent) {
    render_history_clear(RENDER_HISTORY_CPU);
    render_history_clear(RENDER_HISTORY_MEMORY);

    render_history_add(RENDER_HISTORY_CPU, 25.0);
    render_history_add(RENDER_HISTORY_MEMORY, 75.0);

    ASSERT_EQ(render_history_count(RENDER_HISTORY_CPU), 1);
    ASSERT_EQ(render_history_count(RENDER_HISTORY_MEMORY), 1);

    ASSERT_DOUBLE_EQ(render_history_get(RENDER_HISTORY_CPU, 0), 25.0);
    ASSERT_DOUBLE_EQ(render_history_get(RENDER_HISTORY_MEMORY, 0), 75.0);
}

/* Test: history clear works */
TEST(test_history_clear) {
    render_history_clear(RENDER_HISTORY_GPU);
    render_history_add(RENDER_HISTORY_GPU, 50.0);
    render_history_add(RENDER_HISTORY_GPU, 60.0);

    ASSERT_EQ(render_history_count(RENDER_HISTORY_GPU), 2);

    render_history_clear(RENDER_HISTORY_GPU);

    ASSERT_EQ(render_history_count(RENDER_HISTORY_GPU), 0);
}

/* Test: history wraps around at max capacity */
TEST(test_history_wrap_around) {
    render_history_clear(RENDER_HISTORY_GPU_MEM);

    /* Add 130 values (more than MAX_HISTORY which is 128) */
    for (int i = 0; i < 130; i++) {
        render_history_add(RENDER_HISTORY_GPU_MEM, (double)i);
    }

    /* Count should be capped at 128 */
    ASSERT_EQ(render_history_count(RENDER_HISTORY_GPU_MEM), 128);

    /* Most recent should be 129 */
    ASSERT_DOUBLE_EQ(render_history_get(RENDER_HISTORY_GPU_MEM, 0), 129.0);

    /* Oldest available should be 130 - 128 = 2 */
    ASSERT_DOUBLE_EQ(render_history_get(RENDER_HISTORY_GPU_MEM, 127), 2.0);
}

/* Test: history with boundary values */
TEST(test_history_boundary_values) {
    render_history_clear(RENDER_HISTORY_CPU);

    /* Test with 0% */
    render_history_add(RENDER_HISTORY_CPU, 0.0);
    ASSERT_DOUBLE_EQ(render_history_get(RENDER_HISTORY_CPU, 0), 0.0);

    /* Test with 100% */
    render_history_add(RENDER_HISTORY_CPU, 100.0);
    ASSERT_DOUBLE_EQ(render_history_get(RENDER_HISTORY_CPU, 0), 100.0);

    /* Test with negative (shouldn't happen but test behavior) */
    render_history_add(RENDER_HISTORY_CPU, -5.0);
    ASSERT_DOUBLE_EQ(render_history_get(RENDER_HISTORY_CPU, 0), -5.0);

    /* Test with over 100 (shouldn't happen but test behavior) */
    render_history_add(RENDER_HISTORY_CPU, 150.0);
    ASSERT_DOUBLE_EQ(render_history_get(RENDER_HISTORY_CPU, 0), 150.0);
}

/* Test: all history types work */
TEST(test_all_history_types) {
    render_history_clear(RENDER_HISTORY_CPU);
    render_history_clear(RENDER_HISTORY_MEMORY);
    render_history_clear(RENDER_HISTORY_GPU);
    render_history_clear(RENDER_HISTORY_GPU_MEM);

    render_history_add(RENDER_HISTORY_CPU, 10.0);
    render_history_add(RENDER_HISTORY_MEMORY, 20.0);
    render_history_add(RENDER_HISTORY_GPU, 30.0);
    render_history_add(RENDER_HISTORY_GPU_MEM, 40.0);

    ASSERT_DOUBLE_EQ(render_history_get(RENDER_HISTORY_CPU, 0), 10.0);
    ASSERT_DOUBLE_EQ(render_history_get(RENDER_HISTORY_MEMORY, 0), 20.0);
    ASSERT_DOUBLE_EQ(render_history_get(RENDER_HISTORY_GPU, 0), 30.0);
    ASSERT_DOUBLE_EQ(render_history_get(RENDER_HISTORY_GPU_MEM, 0), 40.0);
}

/* Test: history count enum value */
TEST(test_history_count_enum) {
    ASSERT_EQ(RENDER_HISTORY_COUNT, 4);
}

int main(void) {
    printf("Running graph/history tests...\n\n");

    printf("Config tests:\n");
    RUN_TEST(test_config_default_graph_style);
    RUN_TEST(test_graph_style_enum_values);

    printf("\nHistory tests:\n");
    RUN_TEST(test_history_starts_empty);
    RUN_TEST(test_history_add_one);
    RUN_TEST(test_history_add_multiple);
    RUN_TEST(test_history_get_out_of_range);
    RUN_TEST(test_history_types_independent);
    RUN_TEST(test_history_clear);
    RUN_TEST(test_history_wrap_around);
    RUN_TEST(test_history_boundary_values);
    RUN_TEST(test_all_history_types);
    RUN_TEST(test_history_count_enum);

    printf("\n========================================\n");
    printf("Tests: %d passed, %d total\n", tests_passed, tests_run);
    printf("========================================\n");

    return 0;
}
