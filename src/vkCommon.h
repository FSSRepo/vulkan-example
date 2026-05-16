#ifndef __VK_COMMON_H__
#define __VK_COMMON_H__

#ifdef ANDROID_VULKAN
#include "vulkan_wrapper.h"
#else
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#endif

#include <vector>
#include <stdexcept>
#include <algorithm>
#include <iostream>
#include <set>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <limits>

#define MAX_FRAMES_IN_FLIGHT 2

#endif // __VK_COMMON_H__
