#ifndef AI_UPSCALER_H
#define AI_UPSCALER_H

#include <cstdint>
#include <string>
#include <unordered_map>
#include <memory>
#include <vector>

#include "tensorflow/lite/interpreter.h"
#include "tensorflow/lite/kernels/register.h"
#include "tensorflow/lite/model.h"
#include "tensorflow/lite/delegates/xnnpack/xnnpack_delegate.h"

class AIUpscaler {
public:
    AIUpscaler(const std::string& model_path);
    ~AIUpscaler();

    bool Initialize();
    bool UpscaleTexture(const uint8_t* input, int width, int height,
                        uint8_t* output, int* out_width, int* out_height);

    // Cache management
    bool IsCached(uint64_t texture_hash) const;
    void CacheTexture(uint64_t texture_hash, const uint8_t* data, int size);
    const uint8_t* GetCached(uint64_t texture_hash) const;
    uint64_t ComputeHash(const uint8_t* data, int size);

private:
    std::string model_path_;
    std::unique_ptr<tflite::FlatBufferModel> model_;
    std::unique_ptr<tflite::Interpreter> interpreter_;
    TfLiteDelegate* xnnpack_delegate_;

    // LRU cache for upscaled textures
    std::unordered_map<uint64_t, std::vector<uint8_t>> cache_;
};

#endif // AI_UPSCALER_H
