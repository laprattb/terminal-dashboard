# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```bash
# Build
mkdir build && cd build
cmake ..
cmake --build .

# Run
./dashboard                        # Use default config
./dashboard -c custom_config.ini   # Custom config file

# Run tests
ctest                    # Run all tests
ctest --verbose          # Verbose output
```

## Architecture

Terminal Dashboard is a cross-platform C11 system monitoring tool that displays real-time CPU, memory, and disk metrics in the terminal.

### Core Subsystems

**Configuration (`src/config.c`)** - INI file parser supporting sections: [general], [display], [colors], [thresholds], [style], [disks]. Falls back to sensible defaults if config is missing.

**Metrics (`src/metrics_*.c`)** - Platform-abstracted system monitoring with separate implementations:
- `metrics_win32.c` - Windows (GetSystemTimes, GlobalMemoryStatusEx, GetDiskFreeSpaceEx)
- `metrics_darwin.c` - macOS (Mach kernel APIs, statfs)
- `metrics_linux.c` - Linux (POSIX APIs)

All expose the same interface defined in `metrics.h`: init/cleanup functions, CPU/memory/disk collection.

**Render (`src/render.c`)** - Terminal rendering using ANSI escape codes. Handles Windows Virtual Terminal Processing setup. Dynamically calculates bar widths based on terminal size (48 chars overhead, 10 char minimum). Color thresholds: warning at 80%, critical at 90%.

**Main (`src/main.c`)** - Event loop with cross-platform signal handling (Unix SIGINT/SIGTERM, Windows SetConsoleCtrlHandler). CLI parsing for `-c/--config`, `-h/--help`, `-v/--version`.

### Data Flow

```
main.c → config.c (load settings)
       → metrics_*.c (collect system data)
       → render.c (display to terminal)
```

### Platform Abstractions

- Signal handling: Unix signals vs Windows console handlers
- Default config paths: `%APPDATA%\dashboard\` (Windows) vs `~/.config/dashboard/` (Unix)
- Default monitored disk: `C:\` (Windows) vs `/` (Unix)
- Terminal setup: ANSI codes work natively on Unix; Windows requires enabling Virtual Terminal Processing

## Testing

Tests use a custom lightweight framework (TEST/RUN_TEST/ASSERT macros in each test file).

- `tests/test_render.c` - Tests bar width calculation across various terminal sizes
- `tests/test_memory_leaks.c` - Memory leak detection using Windows CRT debug heap, verifies config and metrics init/cleanup cycles
