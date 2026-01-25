cmake_minimum_required(VERSION 3.16)
project(terminal_dashboard C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Common source files
set(COMMON_SOURCES
    src/main.c
    src/config.c
    src/render.c
)

# Platform-specific sources
if(APPLE)
    list(APPEND PLATFORM_SOURCES src/metrics_darwin.c)
elseif(UNIX AND NOT APPLE)
    list(APPEND PLATFORM_SOURCES src/metrics_linux.c)
    list(APPEND PLATFORM_SOURCES src/metrics_gpu_nvidia.c)
elseif(WIN32)
    list(APPEND PLATFORM_SOURCES src/metrics_win32.c)
    list(APPEND PLATFORM_SOURCES src/metrics_gpu_nvidia.c)
endif()

add_executable(dashboard ${COMMON_SOURCES} ${PLATFORM_SOURCES})

# Platform-specific libraries
if(APPLE)
    # macOS needs CoreFoundation and IOKit for some system APIs
    target_link_libraries(dashboard "-framework CoreFoundation" "-framework IOKit")
endif()

# Compiler warnings
if(MSVC)
    target_compile_options(dashboard PRIVATE /W4)
else()
    target_compile_options(dashboard PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Install target
install(TARGETS dashboard RUNTIME DESTINATION bin)

# Testing
enable_testing()

# Test executable - needs render.c, config.c, and platform metrics for the functions being tested
add_executable(test_render
    tests/test_render.c
    src/render.c
    src/config.c
    ${PLATFORM_SOURCES}
)

# Platform-specific test libraries
if(APPLE)
    target_link_libraries(test_render "-framework CoreFoundation" "-framework IOKit")
endif()

# Compiler warnings for tests
if(MSVC)
    target_compile_options(test_render PRIVATE /W4)
else()
    target_compile_options(test_render PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME render_tests COMMAND test_render)

# Memory leak test executable
add_executable(test_memory_leaks
    tests/test_memory_leaks.c
    src/config.c
    src/render.c
    ${PLATFORM_SOURCES}
)

# Platform-specific test libraries for memory leak tests
if(APPLE)
    target_link_libraries(test_memory_leaks "-framework CoreFoundation" "-framework IOKit")
elseif(WIN32)
    target_link_libraries(test_memory_leaks psapi)
endif()

# Compiler warnings for memory leak tests
if(MSVC)
    target_compile_options(test_memory_leaks PRIVATE /W4)
else()
    target_compile_options(test_memory_leaks PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME memory_leak_tests COMMAND test_memory_leaks)

# Graph/history test executable
add_executable(test_graph
    tests/test_graph.c
    src/config.c
    src/render.c
    ${PLATFORM_SOURCES}
)

# Platform-specific test libraries for graph tests
if(APPLE)
    target_link_libraries(test_graph "-framework CoreFoundation" "-framework IOKit")
endif()

# Link math library on Unix
if(UNIX)
    target_link_libraries(test_graph m)
endif()

# Compiler warnings for graph tests
if(MSVC)
    target_compile_options(test_graph PRIVATE /W4)
else()
    target_compile_options(test_graph PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME graph_tests COMMAND test_graph)
