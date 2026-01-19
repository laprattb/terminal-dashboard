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
elseif(WIN32)
    list(APPEND PLATFORM_SOURCES src/metrics_win32.c)
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
