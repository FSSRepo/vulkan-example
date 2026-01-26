cmake_minimum_required(VERSION 3.4.1)

project(vk-android-test)

set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -u ANativeActivity_onCreate")

add_library(native_app_glue STATIC ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c)

include_directories(
        src/main/cpp/
        ${ANDROID_NDK}/sources/android/native_app_glue)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror -Wno-constexpr-not-const -std=c++11  \
                     -DVK_USE_PLATFORM_ANDROID_KHR -DANDROID_VULKAN")

add_library(vk-example SHARED cube_android.cpp vulkan_wrapper.cpp ../vkApp.cpp)

target_link_libraries(vk-example android log native_app_glue)