#ifndef LUMINA_INTEGRATION_H
#define LUMINA_INTEGRATION_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialize the connection to the LUMINA64 plugin
bool lumina_init_plugin(const char* plugin_path);

// Shutdown and cleanup
void lumina_shutdown_plugin(void);

/**
 * Upscale a texture
 * @param input Raw RGB/RGBA buffer
 * @param w Width of input
 * @param h Height of input
 * @param output Pointer to buffer (must be pre-allocated or handled by plugin)
 * @param out_w Pointer to receive new width
 * @param out_h Pointer to receive new height
 * @return true if upscaled, false if failed/skipped
 */
bool lumina_upscale(const unsigned char* input, int w, int h,
                    unsigned char** output, int* out_w, int* out_h);

// Async API
typedef int LuminaRequestID;
int lumina_upscale_async(const unsigned char* input_rgb, int width, int height);
int lumina_async_status(LuminaRequestID id); // 0=Pending, 1=Ready, -1=Error
bool lumina_async_result(LuminaRequestID id, unsigned char** out_data, int* out_w, int* out_h);

// Toggles & OSD
void lumina_set_enabled(int enabled);
int lumina_get_enabled(void);
void lumina_set_osd_enabled(int enabled);
int lumina_get_osd_enabled(void);
void lumina_render_osd(void);

#ifdef __cplusplus
}
#endif

#endif // LUMINA_INTEGRATION_H
