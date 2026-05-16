#include "shared_utils.h"
#include <android/log.h>
#include <android/asset_manager.h>
#include <cstring>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define TAG "Vulkan-Example"
#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__))
#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__))

std::vector<char> readAsset(AAssetManager* mgr, const std::string& filename) {
    AAsset* asset = AAssetManager_open(mgr, filename.c_str(), AASSET_MODE_BUFFER);
    if (!asset) {
        LOGE("failed to open asset: %s", filename.c_str());
        return {};
    }
    size_t size = AAsset_getLength(asset);
    std::vector<char> buffer(size);
    AAsset_read(asset, buffer.data(), size);
    AAsset_close(asset);
    return buffer;
}

std::vector<unsigned char> loadPngRgbaAsset(AAssetManager* mgr, const char* filename, int &outW, int &outH) {
    AAsset* asset = AAssetManager_open(mgr, filename, AASSET_MODE_BUFFER);
    if (!asset) {
        LOGE("failed to open asset: %s", filename);
        return {};
    }
    size_t size = AAsset_getLength(asset);
    std::vector<unsigned char> buffer(size);
    AAsset_read(asset, buffer.data(), size);
    AAsset_close(asset);

    int n;
    unsigned char* data = stbi_load_from_memory(buffer.data(), (int)size, &outW, &outH, &n, STBI_rgb_alpha);
    if (!data) {
        LOGE("failed to load image with stb_image: %s", filename);
        return {};
    }
    std::vector<unsigned char> pixels((size_t)outW * (size_t)outH * 4);
    std::memcpy(pixels.data(), data, pixels.size());
    stbi_image_free(data);
    return pixels;
}


