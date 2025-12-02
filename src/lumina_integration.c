#include "lumina_integration.h"
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h> // For Linux/macOS dynamic loading

static void* g_plugin_handle = NULL;

// Function pointers
static int (*fn_lumina_init)(void) = NULL;
static void (*fn_lumina_shutdown)(void) = NULL;
static int (*fn_lumina_process)(const unsigned char*, int, int, unsigned char*, int*, int*) = NULL;

// Async API pointers
static int (*fn_lumina_upscale_async)(const unsigned char*, int, int) = NULL;
static int (*fn_lumina_async_status)(LuminaRequestID) = NULL;
static bool (*fn_lumina_async_result)(LuminaRequestID, unsigned char**, int*, int*) = NULL;

// Toggle API pointers
static void (*fn_lumina_set_enabled)(int) = NULL;
static int (*fn_lumina_get_enabled)(void) = NULL;
static void (*fn_lumina_set_osd_enabled)(int) = NULL;
static int (*fn_lumina_get_osd_enabled)(void) = NULL;
static void (*fn_lumina_render_osd)(void) = NULL;

bool lumina_init_plugin(const char* plugin_path) {
    if (g_plugin_handle) return true; // Already loaded

    // Clear errors
    dlerror();

    printf("[Rice-Lumina] Loading plugin from: %s\n", plugin_path);
    
    // 1. Load the shared library
    g_plugin_handle = dlopen(plugin_path, RTLD_NOW | RTLD_LOCAL);
    if (!g_plugin_handle) {
        fprintf(stderr, "[Rice-Lumina] Error loading library: %s\n", dlerror());
        return false;
    }

    // 2. Resolve symbols (functions)
    // Note: These names must match extern "C" functions in your plugin_hooks.cpp
    *(void**)(&fn_lumina_init) = dlsym(g_plugin_handle, "lumina_init");
    *(void**)(&fn_lumina_shutdown) = dlsym(g_plugin_handle, "lumina_shutdown");
    *(void**)(&fn_lumina_process) = dlsym(g_plugin_handle, "lumina_process_texture");
    
    // Load Async symbols
    *(void**)(&fn_lumina_upscale_async) = dlsym(g_plugin_handle, "lumina_upscale_async");
    *(void**)(&fn_lumina_async_status) = dlsym(g_plugin_handle, "lumina_async_status");
    *(void**)(&fn_lumina_async_result) = dlsym(g_plugin_handle, "lumina_async_result");

    // Load Toggle symbols
    *(void**)(&fn_lumina_set_enabled) = dlsym(g_plugin_handle, "lumina_set_enabled");
    *(void**)(&fn_lumina_get_enabled) = dlsym(g_plugin_handle, "lumina_get_enabled");
    *(void**)(&fn_lumina_set_osd_enabled) = dlsym(g_plugin_handle, "lumina_set_osd_enabled");
    *(void**)(&fn_lumina_get_osd_enabled) = dlsym(g_plugin_handle, "lumina_get_osd_enabled");
    *(void**)(&fn_lumina_render_osd) = dlsym(g_plugin_handle, "lumina_render_osd");

    if (!fn_lumina_init || !fn_lumina_shutdown || !fn_lumina_process) {
        fprintf(stderr, "[Rice-Lumina] Error loading symbols: %s\n", dlerror());
        dlclose(g_plugin_handle);
        g_plugin_handle = NULL;
        return false;
    }

    // 3. Initialize the plugin
    if (fn_lumina_init() != 1) { // Assuming 1 is success
        fprintf(stderr, "[Rice-Lumina] Plugin internal init failed.\n");
        dlclose(g_plugin_handle);
        g_plugin_handle = NULL;
        return false;
    }

    printf("[Rice-Lumina] Integration successful! Neural upscaling active.\n");
    return true;
}

void lumina_shutdown_plugin(void) {
    if (g_plugin_handle) {
        if (fn_lumina_shutdown) fn_lumina_shutdown();
        dlclose(g_plugin_handle);
        g_plugin_handle = NULL;
        fn_lumina_init = NULL;
        fn_lumina_shutdown = NULL;
        fn_lumina_process = NULL;
        fn_lumina_upscale_async = NULL;
        fn_lumina_async_status = NULL;
        fn_lumina_async_result = NULL;
        fn_lumina_set_enabled = NULL;
        fn_lumina_get_enabled = NULL;
        fn_lumina_set_osd_enabled = NULL;
        fn_lumina_get_osd_enabled = NULL;
        fn_lumina_render_osd = NULL;
        printf("[Rice-Lumina] Plugin unloaded.\n");
    }
}

bool lumina_upscale(const unsigned char* input, int w, int h,
                    unsigned char** output, int* out_w, int* out_h) {
    if (!g_plugin_handle || !fn_lumina_process) return false;
    
    // The plugin allocates the output buffer, we need to free it later?
    // Wait, the previous implementation assumed plugin allocates.
    // Let's check plugin_hooks.cpp:
    // It calls ProcessTexture which calls UpscaleTextureWithCache.
    // UpscaleTextureWithCache expects pre-allocated output buffer if we look at previous code?
    // No, my new plugin_hooks.cpp wrapper `lumina_process_texture` calls `lumina::ProcessTexture`.
    // `lumina::ProcessTexture` takes `uint8_t* output`.
    // So the CALLER must allocate.
    // BUT `lumina_async_result` allocates.
    // The synchronous `lumina_upscale` wrapper in `lumina_integration.c` (THIS FILE)
    // currently signature is `unsigned char** output`.
    // So it expects to return a pointer.
    
    // Let's fix the synchronous wrapper to allocate.
    // Max 4x upscale.
    int max_size = w * 4 * h * 4 * 3;
    *output = (unsigned char*)malloc(max_size);
    if (!*output) return false;
    
    int result = fn_lumina_process(input, w, h, *output, out_w, out_h);
    if (result != 1) {
        free(*output);
        *output = NULL;
        return false;
    }
    return true;
}

// Async Wrappers
int lumina_upscale_async(const unsigned char* input_rgb, int width, int height) {
    if (!g_plugin_handle || !fn_lumina_upscale_async) return 0;
    return fn_lumina_upscale_async(input_rgb, width, height);
}

int lumina_async_status(LuminaRequestID id) {
    if (!g_plugin_handle || !fn_lumina_async_status) return -1;
    return fn_lumina_async_status(id);
}

bool lumina_async_result(LuminaRequestID id, unsigned char** out_data, int* out_w, int* out_h) {
    if (!g_plugin_handle || !fn_lumina_async_result) return false;
    return fn_lumina_async_result(id, out_data, out_w, out_h);
}

// Toggle Wrappers
void lumina_set_enabled(int enabled) {
    if (g_plugin_handle && fn_lumina_set_enabled) fn_lumina_set_enabled(enabled);
}

int lumina_get_enabled(void) {
    if (g_plugin_handle && fn_lumina_get_enabled) return fn_lumina_get_enabled();
    return 0;
}

void lumina_set_osd_enabled(int enabled) {
    if (g_plugin_handle && fn_lumina_set_osd_enabled) fn_lumina_set_osd_enabled(enabled);
}

int lumina_get_osd_enabled(void) {
    if (g_plugin_handle && fn_lumina_get_osd_enabled) return fn_lumina_get_osd_enabled();
    return 0;
}

void lumina_render_osd(void) {
    if (g_plugin_handle && fn_lumina_render_osd) fn_lumina_render_osd();
}
