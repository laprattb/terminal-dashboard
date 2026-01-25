#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/render.h"
#include "../src/config.h"

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

/* Test: bar width calculation with standard terminal width */
TEST(test_bar_width_standard_terminal) {
    /* 80 column terminal: 80 - 48 = 32 */
    int width = render_calculate_bar_width(80);
    ASSERT_EQ(width, 32);
}

/* Test: bar width calculation with wide terminal */
TEST(test_bar_width_wide_terminal) {
    /* 120 column terminal: 120 - 48 = 72 */
    int width = render_calculate_bar_width(120);
    ASSERT_EQ(width, 72);
}

/* Test: bar width calculation with very wide terminal */
TEST(test_bar_width_very_wide_terminal) {
    /* 200 column terminal: 200 - 48 = 152 */
    int width = render_calculate_bar_width(200);
    ASSERT_EQ(width, 152);
}

/* Test: bar width minimum enforced for narrow terminal */
TEST(test_bar_width_narrow_terminal) {
    /* 50 column terminal: 50 - 48 = 2, but min is 10 */
    int width = render_calculate_bar_width(50);
    ASSERT_EQ(width, 10);
}

/* Test: bar width minimum enforced for very narrow terminal */
TEST(test_bar_width_very_narrow_terminal) {
    /* 30 column terminal: 30 - 48 = -18, but min is 10 */
    int width = render_calculate_bar_width(30);
    ASSERT_EQ(width, 10);
}

/* Test: bar width at exact minimum boundary */
TEST(test_bar_width_at_minimum_boundary) {
    /* 58 column terminal: 58 - 48 = 10 (exactly minimum) */
    int width = render_calculate_bar_width(58);
    ASSERT_EQ(width, 10);
}

/* Test: bar width just above minimum boundary */
TEST(test_bar_width_above_minimum_boundary) {
    /* 59 column terminal: 59 - 48 = 11 */
    int width = render_calculate_bar_width(59);
    ASSERT_EQ(width, 11);
}

/* Test: bar width just below minimum boundary */
TEST(test_bar_width_below_minimum_boundary) {
    /* 57 column terminal: 57 - 48 = 9, but min is 10 */
    int width = render_calculate_bar_width(57);
    ASSERT_EQ(width, 10);
}

/* Test: terminal size returns reasonable values */
TEST(test_terminal_size_reasonable) {
    int width, height;
    render_get_terminal_size(&width, &height);

    /* Terminal should have positive dimensions */
    ASSERT(width > 0);
    ASSERT(height > 0);

    /* Terminal dimensions should be reasonable (at least minimal) */
    ASSERT(width >= 20);
    ASSERT(height >= 5);

    /* Terminal dimensions should not be absurdly large */
    ASSERT(width < 10000);
    ASSERT(height < 10000);
}

/* Test: bar width scales linearly */
TEST(test_bar_width_linear_scaling) {
    int width1 = render_calculate_bar_width(100);
    int width2 = render_calculate_bar_width(110);

    /* Adding 10 to terminal width should add 10 to bar width */
    ASSERT_EQ(width2 - width1, 10);
}

int main(void) {
    printf("Running render tests...\n\n");

    printf("Bar width calculation tests:\n");
    RUN_TEST(test_bar_width_standard_terminal);
    RUN_TEST(test_bar_width_wide_terminal);
    RUN_TEST(test_bar_width_very_wide_terminal);
    RUN_TEST(test_bar_width_narrow_terminal);
    RUN_TEST(test_bar_width_very_narrow_terminal);
    RUN_TEST(test_bar_width_at_minimum_boundary);
    RUN_TEST(test_bar_width_above_minimum_boundary);
    RUN_TEST(test_bar_width_below_minimum_boundary);
    RUN_TEST(test_bar_width_linear_scaling);

    printf("\nTerminal size tests:\n");
    RUN_TEST(test_terminal_size_reasonable);

    printf("\n========================================\n");
    printf("Tests: %d passed, %d total\n", tests_passed, tests_run);
    printf("========================================\n");

    return 0;
}
