#ifndef SHARED_UTILS_H
#define SHARED_UTILS_H

#include <vector>
#include <string>
#include <android/asset_manager.h>

std::vector<char> readAsset(AAssetManager* mgr, const std::string& filename);
std::vector<unsigned char> loadPngRgbaAsset(AAssetManager* mgr, const char* filename, int &outW, int &outH);

#endif
