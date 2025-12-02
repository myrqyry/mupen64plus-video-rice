#include "ai_upscaler.h"
#include <cstring>
#include <iostream>
#include <chrono>
#include <algorithm>

// xxHash implementation (fast hashing)
#define XXH_INLINE_ALL
#include "xxhash.h"

AIUpscaler::AIUpscaler(const std::string& model_path)
    : model_path_(model_path), xnnpack_delegate_(nullptr) {
}

AIUpscaler::~AIUpscaler() {
    if (xnnpack_delegate_) {
        TfLiteXNNPackDelegateDelete(xnnpack_delegate_);
    }
}

bool AIUpscaler::Initialize() {
    // Load model
    model_ = tflite::FlatBufferModel::BuildFromFile(model_path_.c_str());
    if (!model_) {
        std::cerr << "Failed to load model: " << model_path_ << std::endl;
        return false;
    }

    // Build interpreter
    tflite::ops::builtin::BuiltinOpResolver resolver;
    tflite::InterpreterBuilder builder(*model_, resolver);
    builder(&interpreter_);
    if (!interpreter_) {
        std::cerr << "Failed to create interpreter" << std::endl;
        return false;
    }

    // Create XNNPACK delegate for CPU optimization
    TfLiteXNNPackDelegateOptions options = TfLiteXNNPackDelegateOptionsDefault();
    options.num_threads = 4; // Adjust based on CPU
    xnnpack_delegate_ = TfLiteXNNPackDelegateCreate(&options);
    
    if (interpreter_->ModifyGraphWithDelegate(xnnpack_delegate_) != kTfLiteOk) {
        std::cerr << "Failed to apply XNNPACK delegate, falling back to CPU" << std::endl;
        // Don't return false, just continue without the delegate
    } else {
        std::cout << "XNNPACK delegate applied successfully" << std::endl;
    }

    // Allocate tensors
    if (interpreter_->AllocateTensors() != kTfLiteOk) {
        std::cerr << "Failed to allocate tensors" << std::endl;
        return false;
    }

    std::cout << "AI Upscaler initialized with XNNPACK" << std::endl;
    return true;
}

bool AIUpscaler::UpscaleTexture(const uint8_t* input, int width, int height,
                                uint8_t* output, int* out_width, int* out_height) {
    // Get input tensor
    int input_idx = interpreter_->inputs()[0];
    TfLiteTensor* input_tensor = interpreter_->tensor(input_idx);

    // Copy and normalize input (0-255 -> 0-1 if model expects float)
    if (input_tensor->type == kTfLiteFloat32) {
        float* input_data = interpreter_->typed_tensor<float>(input_idx);
        for (int i = 0; i < width * height * 3; i++) {
            input_data[i] = input[i] / 255.0f;
        }
    } else {
        // INT8 model - direct copy
        uint8_t* input_data = interpreter_->typed_tensor<uint8_t>(input_idx);
        memcpy(input_data, input, width * height * 3);
    }

    // Run inference
    auto start = std::chrono::high_resolution_clock::now();
    if (interpreter_->Invoke() != kTfLiteOk) {
        std::cerr << "Inference failed" << std::endl;
        return false;
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    // std::cout << "Inference time: " << duration.count() << " us" << std::endl;

    // Get output tensor
    int output_idx = interpreter_->outputs()[0];
    TfLiteTensor* output_tensor = interpreter_->tensor(output_idx);

    // Assuming NHWC format, height is at index 1, width at index 2
    // But usually [batch, height, width, channels]
    // So height is dims->data[1], width is dims->data[2]
    // The cheat sheet used [^2] and [^1], which implies last-2 and last-1.
    // Let's assume standard 4D tensor.
    
    if (output_tensor->dims->size >= 3) {
        *out_height = output_tensor->dims->data[1];
        *out_width = output_tensor->dims->data[2];
    } else {
         // Fallback or error
         *out_width = width * 4; // Assumption
         *out_height = height * 4;
    }

    // Copy output (denormalize if needed)
    if (output_tensor->type == kTfLiteFloat32) {
        float* output_data = interpreter_->typed_tensor<float>(output_idx);
        for (int i = 0; i < (*out_width) * (*out_height) * 3; i++) {
            output[i] = static_cast<uint8_t>(
                std::min(255.0f, std::max(0.0f, output_data[i] * 255.0f))
            );
        }
    } else {
        uint8_t* output_data = interpreter_->typed_tensor<uint8_t>(output_idx);
        memcpy(output, output_data, (*out_width) * (*out_height) * 3);
    }

    return true;
}

uint64_t AIUpscaler::ComputeHash(const uint8_t* data, int size) {
    return XXH64(data, size, 0); // xxHash64 for speed
}

bool AIUpscaler::IsCached(uint64_t texture_hash) const {
    return cache_.find(texture_hash) != cache_.end();
}

void AIUpscaler::CacheTexture(uint64_t texture_hash,
                              const uint8_t* data, int size) {
    cache_[texture_hash] = std::vector<uint8_t>(data, data + size);
}

const uint8_t* AIUpscaler::GetCached(uint64_t texture_hash) const {
    auto it = cache_.find(texture_hash);
    return (it != cache_.end()) ? it->second.data() : nullptr;
}
