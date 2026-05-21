#pragma once

#include <vector>
#include <string>
#include <fstream>
#include <stdexcept>
#include <cstring>

#include "stb_image.h"

inline std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("failed to open file!");
    }
    size_t fileSize = (size_t) file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();
    return buffer;
}

inline std::vector<unsigned char> load_png_rgba(const char* filename, int &outW, int &outH) {
    int n;
    unsigned char* data = stbi_load(filename, &outW, &outH, &n, STBI_rgb_alpha);
    if (!data) {
        throw std::runtime_error("failed to load image with stb_image");
    }
    std::vector<unsigned char> pixels((size_t)outW * (size_t)outH * 4);
    std::memcpy(pixels.data(), data, pixels.size());
    stbi_image_free(data);
    return pixels;
}
