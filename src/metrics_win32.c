#include "metrics.h"
#include <stdio.h>
#include <string.h>
#include <windows.h>

/* Store previous CPU times for delta calculation */
static ULARGE_INTEGER prev_idle_time;
static ULARGE_INTEGER prev_kernel_time;
static ULARGE_INTEGER prev_user_time;
static bool first_cpu_read = true;

/* Convert FILETIME to ULARGE_INTEGER */
static ULARGE_INTEGER filetime_to_uli(FILETIME ft) {
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    return uli;
}

bool metrics_init(void) {
    /* Perform an initial CPU read to establish baseline */
    cpu_metrics_t dummy;
    metrics_get_cpu(&dummy);
    return true;
}

void metrics_cleanup(void) {
    /* Nothing to clean up for Windows */
}

bool metrics_get_cpu(cpu_metrics_t *cpu) {
    FILETIME idle_time, kernel_time, user_time;

    if (!GetSystemTimes(&idle_time, &kernel_time, &user_time)) {
        return false;
    }

    ULARGE_INTEGER idle = filetime_to_uli(idle_time);
    ULARGE_INTEGER kernel = filetime_to_uli(kernel_time);
    ULARGE_INTEGER user = filetime_to_uli(user_time);

    if (first_cpu_read) {
        prev_idle_time = idle;
        prev_kernel_time = kernel;
        prev_user_time = user;
        first_cpu_read = false;

        /* Return zeros for first read */
        cpu->user_percent = 0.0;
        cpu->system_percent = 0.0;
        cpu->idle_percent = 100.0;
        cpu->total_percent = 0.0;
        cpu->temperature_celsius = -1;  /* Not available on first read */
        return true;
    }

    /* Calculate deltas */
    ULONGLONG idle_delta = idle.QuadPart - prev_idle_time.QuadPart;
    ULONGLONG kernel_delta = kernel.QuadPart - prev_kernel_time.QuadPart;
    ULONGLONG user_delta = user.QuadPart - prev_user_time.QuadPart;

    /* Note: kernel time includes idle time on Windows */
    ULONGLONG sys_delta = kernel_delta - idle_delta;
    ULONGLONG total_delta = kernel_delta + user_delta;

    if (total_delta > 0) {
        cpu->user_percent = (double)user_delta / total_delta * 100.0;
        cpu->system_percent = (double)sys_delta / total_delta * 100.0;
        cpu->idle_percent = (double)idle_delta / total_delta * 100.0;
        cpu->total_percent = cpu->user_percent + cpu->system_percent;
    } else {
        cpu->user_percent = 0.0;
        cpu->system_percent = 0.0;
        cpu->idle_percent = 100.0;
        cpu->total_percent = 0.0;
    }

    /* CPU temperature via WMI MSAcpi_ThermalZoneTemperature is not reliably
       available on many Windows systems. Return -1 to indicate unavailable. */
    cpu->temperature_celsius = -1;

    /* Store current values for next calculation */
    prev_idle_time = idle;
    prev_kernel_time = kernel;
    prev_user_time = user;

    return true;
}

bool metrics_get_memory(memory_metrics_t *mem) {
    MEMORYSTATUSEX statex;
    statex.dwLength = sizeof(statex);

    if (!GlobalMemoryStatusEx(&statex)) {
        return false;
    }

    mem->total_bytes = statex.ullTotalPhys;
    mem->free_bytes = statex.ullAvailPhys;
    mem->used_bytes = mem->total_bytes - mem->free_bytes;
    mem->used_percent = (double)mem->used_bytes / mem->total_bytes * 100.0;

    return true;
}

bool metrics_get_disk(const char *mount_point, disk_metrics_t *disk) {
    ULARGE_INTEGER free_bytes_available;
    ULARGE_INTEGER total_bytes;
    ULARGE_INTEGER total_free_bytes;

    /* GetDiskFreeSpaceExA works with drive letters like "C:\" or paths */
    if (!GetDiskFreeSpaceExA(mount_point,
                              &free_bytes_available,
                              &total_bytes,
                              &total_free_bytes)) {
        return false;
    }

    strncpy(disk->mount_point, mount_point, MAX_PATH_LEN - 1);
    disk->mount_point[MAX_PATH_LEN - 1] = '\0';

    disk->total_bytes = total_bytes.QuadPart;
    disk->free_bytes = free_bytes_available.QuadPart;
    disk->used_bytes = disk->total_bytes - disk->free_bytes;

    if (disk->total_bytes > 0) {
        disk->used_percent = (double)disk->used_bytes / disk->total_bytes * 100.0;
    } else {
        disk->used_percent = 0.0;
    }

    return true;
}

bool metrics_get_disks(const char **mount_points, int count, disk_metrics_list_t *disks) {
    disks->count = 0;

    for (int i = 0; i < count && i < MAX_DISKS; i++) {
        if (metrics_get_disk(mount_points[i], &disks->disks[disks->count])) {
            disks->count++;
        }
    }

    return disks->count > 0;
}

void metrics_format_bytes(uint64_t bytes, char *buf, size_t buf_size) {
    const char *units[] = {"B", "KB", "MB", "GB", "TB", "PB"};
    int unit_index = 0;
    double size = (double)bytes;

    while (size >= 1024.0 && unit_index < 5) {
        size /= 1024.0;
        unit_index++;
    }

    if (unit_index == 0) {
        snprintf(buf, buf_size, "%llu %s", (unsigned long long)bytes, units[unit_index]);
    } else {
        snprintf(buf, buf_size, "%.1f %s", size, units[unit_index]);
    }
}
