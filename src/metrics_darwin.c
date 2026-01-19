#include "metrics.h"
#include <stdio.h>
#include <string.h>
#include <mach/mach.h>
#include <mach/mach_host.h>
#include <mach/processor_info.h>
#include <mach/vm_statistics.h>
#include <sys/mount.h>
#include <sys/sysctl.h>

/* Store previous CPU ticks for delta calculation */
static uint64_t prev_user_ticks = 0;
static uint64_t prev_system_ticks = 0;
static uint64_t prev_idle_ticks = 0;
static uint64_t prev_nice_ticks = 0;
static bool first_cpu_read = true;

bool metrics_init(void) {
    /* Perform an initial CPU read to establish baseline */
    cpu_metrics_t dummy;
    metrics_get_cpu(&dummy);
    return true;
}

void metrics_cleanup(void) {
    /* Nothing to clean up for macOS */
}

bool metrics_get_cpu(cpu_metrics_t *cpu) {
    processor_info_array_t cpu_info;
    mach_msg_type_number_t num_cpu_info;
    natural_t num_cpus = 0;

    kern_return_t kr = host_processor_info(mach_host_self(),
                                            PROCESSOR_CPU_LOAD_INFO,
                                            &num_cpus,
                                            &cpu_info,
                                            &num_cpu_info);
    if (kr != KERN_SUCCESS) {
        return false;
    }

    /* Aggregate ticks across all CPUs */
    uint64_t total_user = 0, total_system = 0, total_idle = 0, total_nice = 0;

    for (natural_t i = 0; i < num_cpus; i++) {
        processor_cpu_load_info_t info = (processor_cpu_load_info_t)cpu_info;
        total_user += info[i].cpu_ticks[CPU_STATE_USER];
        total_system += info[i].cpu_ticks[CPU_STATE_SYSTEM];
        total_idle += info[i].cpu_ticks[CPU_STATE_IDLE];
        total_nice += info[i].cpu_ticks[CPU_STATE_NICE];
    }

    /* Deallocate the cpu_info array */
    vm_deallocate(mach_task_self(), (vm_address_t)cpu_info,
                  num_cpu_info * sizeof(integer_t));

    if (first_cpu_read) {
        prev_user_ticks = total_user;
        prev_system_ticks = total_system;
        prev_idle_ticks = total_idle;
        prev_nice_ticks = total_nice;
        first_cpu_read = false;

        /* Return zeros for first read */
        cpu->user_percent = 0.0;
        cpu->system_percent = 0.0;
        cpu->idle_percent = 100.0;
        cpu->total_percent = 0.0;
        return true;
    }

    /* Calculate deltas */
    uint64_t user_delta = total_user - prev_user_ticks;
    uint64_t system_delta = total_system - prev_system_ticks;
    uint64_t idle_delta = total_idle - prev_idle_ticks;
    uint64_t nice_delta = total_nice - prev_nice_ticks;

    uint64_t total_delta = user_delta + system_delta + idle_delta + nice_delta;

    if (total_delta > 0) {
        cpu->user_percent = (double)(user_delta + nice_delta) / total_delta * 100.0;
        cpu->system_percent = (double)system_delta / total_delta * 100.0;
        cpu->idle_percent = (double)idle_delta / total_delta * 100.0;
        cpu->total_percent = cpu->user_percent + cpu->system_percent;
    } else {
        cpu->user_percent = 0.0;
        cpu->system_percent = 0.0;
        cpu->idle_percent = 100.0;
        cpu->total_percent = 0.0;
    }

    /* Store current values for next calculation */
    prev_user_ticks = total_user;
    prev_system_ticks = total_system;
    prev_idle_ticks = total_idle;
    prev_nice_ticks = total_nice;

    return true;
}

bool metrics_get_memory(memory_metrics_t *mem) {
    mach_port_t host = mach_host_self();
    vm_size_t page_size;
    vm_statistics64_data_t vm_stats;
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;

    if (host_page_size(host, &page_size) != KERN_SUCCESS) {
        return false;
    }

    if (host_statistics64(host, HOST_VM_INFO64,
                          (host_info64_t)&vm_stats, &count) != KERN_SUCCESS) {
        return false;
    }

    /* Get total physical memory via sysctl */
    int mib[2] = { CTL_HW, HW_MEMSIZE };
    uint64_t total_mem;
    size_t len = sizeof(total_mem);

    if (sysctl(mib, 2, &total_mem, &len, NULL, 0) != 0) {
        return false;
    }

    /* Calculate used memory (active + wired + compressed) */
    uint64_t used = ((uint64_t)vm_stats.active_count +
                     (uint64_t)vm_stats.wire_count +
                     (uint64_t)vm_stats.compressor_page_count) * page_size;

    mem->total_bytes = total_mem;
    mem->used_bytes = used;
    mem->free_bytes = total_mem - used;
    mem->used_percent = (double)used / total_mem * 100.0;

    return true;
}

bool metrics_get_disk(const char *mount_point, disk_metrics_t *disk) {
    struct statfs fs;

    if (statfs(mount_point, &fs) != 0) {
        return false;
    }

    strncpy(disk->mount_point, mount_point, MAX_PATH_LEN - 1);
    disk->mount_point[MAX_PATH_LEN - 1] = '\0';

    disk->total_bytes = (uint64_t)fs.f_blocks * fs.f_bsize;
    disk->free_bytes = (uint64_t)fs.f_bavail * fs.f_bsize;
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
