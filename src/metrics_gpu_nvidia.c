#include "metrics_gpu.h"
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#define NVML_LIB_NAME "nvml.dll"
typedef HMODULE lib_handle_t;
#define LOAD_LIBRARY(name) LoadLibraryA(name)
#define GET_PROC(lib, name) GetProcAddress(lib, name)
#define FREE_LIBRARY(lib) FreeLibrary(lib)
#else
#include <dlfcn.h>
#define NVML_LIB_NAME "libnvidia-ml.so.1"
typedef void* lib_handle_t;
#define LOAD_LIBRARY(name) dlopen(name, RTLD_LAZY)
#define GET_PROC(lib, name) dlsym(lib, name)
#define FREE_LIBRARY(lib) dlclose(lib)
#endif

/* NVML return codes */
#define NVML_SUCCESS 0

/* NVML temperature sensor type */
#define NVML_TEMPERATURE_GPU 0

/* NVML types */
typedef void* nvmlDevice_t;

typedef struct {
    unsigned int gpu;
    unsigned int memory;
} nvmlUtilization_t;

typedef struct {
    unsigned long long total;
    unsigned long long free;
    unsigned long long used;
} nvmlMemory_t;

/* NVML function pointer types */
typedef int (*nvmlInit_t)(void);
typedef int (*nvmlShutdown_t)(void);
typedef int (*nvmlDeviceGetCount_t)(unsigned int *deviceCount);
typedef int (*nvmlDeviceGetHandleByIndex_t)(unsigned int index, nvmlDevice_t *device);
typedef int (*nvmlDeviceGetName_t)(nvmlDevice_t device, char *name, unsigned int length);
typedef int (*nvmlDeviceGetUtilizationRates_t)(nvmlDevice_t device, nvmlUtilization_t *utilization);
typedef int (*nvmlDeviceGetMemoryInfo_t)(nvmlDevice_t device, nvmlMemory_t *memory);
typedef int (*nvmlDeviceGetTemperature_t)(nvmlDevice_t device, int sensorType, unsigned int *temp);
typedef int (*nvmlDeviceGetPowerUsage_t)(nvmlDevice_t device, unsigned int *power);

/* NVML function pointers */
static nvmlInit_t fn_nvmlInit = NULL;
static nvmlShutdown_t fn_nvmlShutdown = NULL;
static nvmlDeviceGetCount_t fn_nvmlDeviceGetCount = NULL;
static nvmlDeviceGetHandleByIndex_t fn_nvmlDeviceGetHandleByIndex = NULL;
static nvmlDeviceGetName_t fn_nvmlDeviceGetName = NULL;
static nvmlDeviceGetUtilizationRates_t fn_nvmlDeviceGetUtilizationRates = NULL;
static nvmlDeviceGetMemoryInfo_t fn_nvmlDeviceGetMemoryInfo = NULL;
static nvmlDeviceGetTemperature_t fn_nvmlDeviceGetTemperature = NULL;
static nvmlDeviceGetPowerUsage_t fn_nvmlDeviceGetPowerUsage = NULL;

/* Library state */
static lib_handle_t nvml_lib = NULL;
static bool nvml_initialized = false;
static nvmlDevice_t gpu_device = NULL;

static bool load_nvml_functions(void) {
    fn_nvmlInit = (nvmlInit_t)GET_PROC(nvml_lib, "nvmlInit_v2");
    if (!fn_nvmlInit) {
        fn_nvmlInit = (nvmlInit_t)GET_PROC(nvml_lib, "nvmlInit");
    }

    fn_nvmlShutdown = (nvmlShutdown_t)GET_PROC(nvml_lib, "nvmlShutdown");
    fn_nvmlDeviceGetCount = (nvmlDeviceGetCount_t)GET_PROC(nvml_lib, "nvmlDeviceGetCount_v2");
    if (!fn_nvmlDeviceGetCount) {
        fn_nvmlDeviceGetCount = (nvmlDeviceGetCount_t)GET_PROC(nvml_lib, "nvmlDeviceGetCount");
    }

    fn_nvmlDeviceGetHandleByIndex = (nvmlDeviceGetHandleByIndex_t)GET_PROC(nvml_lib, "nvmlDeviceGetHandleByIndex_v2");
    if (!fn_nvmlDeviceGetHandleByIndex) {
        fn_nvmlDeviceGetHandleByIndex = (nvmlDeviceGetHandleByIndex_t)GET_PROC(nvml_lib, "nvmlDeviceGetHandleByIndex");
    }

    fn_nvmlDeviceGetName = (nvmlDeviceGetName_t)GET_PROC(nvml_lib, "nvmlDeviceGetName");
    fn_nvmlDeviceGetUtilizationRates = (nvmlDeviceGetUtilizationRates_t)GET_PROC(nvml_lib, "nvmlDeviceGetUtilizationRates");
    fn_nvmlDeviceGetMemoryInfo = (nvmlDeviceGetMemoryInfo_t)GET_PROC(nvml_lib, "nvmlDeviceGetMemoryInfo");
    fn_nvmlDeviceGetTemperature = (nvmlDeviceGetTemperature_t)GET_PROC(nvml_lib, "nvmlDeviceGetTemperature");
    fn_nvmlDeviceGetPowerUsage = (nvmlDeviceGetPowerUsage_t)GET_PROC(nvml_lib, "nvmlDeviceGetPowerUsage");

    /* Check required functions are loaded */
    return (fn_nvmlInit != NULL &&
            fn_nvmlShutdown != NULL &&
            fn_nvmlDeviceGetCount != NULL &&
            fn_nvmlDeviceGetHandleByIndex != NULL);
}

bool gpu_metrics_init(void) {
    /* Try to load NVML library */
    nvml_lib = LOAD_LIBRARY(NVML_LIB_NAME);
    if (!nvml_lib) {
#ifdef _WIN32
        /* Try alternate Windows paths */
        const char *program_files = getenv("ProgramFiles");
        if (program_files) {
            char path[512];
            snprintf(path, sizeof(path), "%s\\NVIDIA Corporation\\NVSMI\\nvml.dll", program_files);
            nvml_lib = LOAD_LIBRARY(path);
        }
        if (!nvml_lib) {
            const char *windir = getenv("SystemRoot");
            if (windir) {
                char path[512];
                snprintf(path, sizeof(path), "%s\\System32\\nvml.dll", windir);
                nvml_lib = LOAD_LIBRARY(path);
            }
        }
#endif
        if (!nvml_lib) {
            return false;  /* NVML not available */
        }
    }

    /* Load function pointers */
    if (!load_nvml_functions()) {
        FREE_LIBRARY(nvml_lib);
        nvml_lib = NULL;
        return false;
    }

    /* Initialize NVML */
    if (fn_nvmlInit() != NVML_SUCCESS) {
        FREE_LIBRARY(nvml_lib);
        nvml_lib = NULL;
        return false;
    }

    /* Get device count */
    unsigned int device_count = 0;
    if (fn_nvmlDeviceGetCount(&device_count) != NVML_SUCCESS || device_count == 0) {
        fn_nvmlShutdown();
        FREE_LIBRARY(nvml_lib);
        nvml_lib = NULL;
        return false;
    }

    /* Get handle to first GPU */
    if (fn_nvmlDeviceGetHandleByIndex(0, &gpu_device) != NVML_SUCCESS) {
        fn_nvmlShutdown();
        FREE_LIBRARY(nvml_lib);
        nvml_lib = NULL;
        return false;
    }

    nvml_initialized = true;
    return true;
}

void gpu_metrics_cleanup(void) {
    if (nvml_initialized && fn_nvmlShutdown) {
        fn_nvmlShutdown();
    }

    if (nvml_lib) {
        FREE_LIBRARY(nvml_lib);
        nvml_lib = NULL;
    }

    nvml_initialized = false;
    gpu_device = NULL;
}

bool gpu_metrics_get(gpu_metrics_t *gpu) {
    memset(gpu, 0, sizeof(gpu_metrics_t));
    gpu->temperature_celsius = -1;
    gpu->power_watts = -1;

    if (!nvml_initialized || !gpu_device) {
        gpu->available = false;
        return false;
    }

    gpu->available = true;

    /* Get GPU name */
    if (fn_nvmlDeviceGetName) {
        if (fn_nvmlDeviceGetName(gpu_device, gpu->name, sizeof(gpu->name)) != NVML_SUCCESS) {
            strncpy(gpu->name, "NVIDIA GPU", sizeof(gpu->name) - 1);
        }
    } else {
        strncpy(gpu->name, "NVIDIA GPU", sizeof(gpu->name) - 1);
    }
    gpu->name[sizeof(gpu->name) - 1] = '\0';

    /* Get utilization */
    if (fn_nvmlDeviceGetUtilizationRates) {
        nvmlUtilization_t util;
        if (fn_nvmlDeviceGetUtilizationRates(gpu_device, &util) == NVML_SUCCESS) {
            gpu->utilization_percent = (int)util.gpu;
        }
    }

    /* Get memory info */
    if (fn_nvmlDeviceGetMemoryInfo) {
        nvmlMemory_t mem;
        if (fn_nvmlDeviceGetMemoryInfo(gpu_device, &mem) == NVML_SUCCESS) {
            gpu->memory_total = mem.total;
            gpu->memory_used = mem.used;
            if (mem.total > 0) {
                gpu->memory_percent = (double)mem.used / mem.total * 100.0;
            }
        }
    }

    /* Get temperature */
    if (fn_nvmlDeviceGetTemperature) {
        unsigned int temp;
        if (fn_nvmlDeviceGetTemperature(gpu_device, NVML_TEMPERATURE_GPU, &temp) == NVML_SUCCESS) {
            gpu->temperature_celsius = (int)temp;
        }
    }

    /* Get power usage (returned in milliwatts, convert to watts) */
    if (fn_nvmlDeviceGetPowerUsage) {
        unsigned int power_mw;
        if (fn_nvmlDeviceGetPowerUsage(gpu_device, &power_mw) == NVML_SUCCESS) {
            gpu->power_watts = (int)(power_mw / 1000);
        }
    }

    return true;
}
