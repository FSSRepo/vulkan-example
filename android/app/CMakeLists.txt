cmake_minimum_required(VERSION 3.5)

project(vk-test)

set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -u ANativeActivity_onCreate")

add_library(native_app_glue STATIC ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c)

include_directories(
        src/main/cpp/
        ../../src
        ../../
        ${ANDROID_NDK}/sources/android/native_app_glue)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror -Wno-constexpr-not-const -std=c++11  \
                     -DVK_USE_PLATFORM_ANDROID_KHR -DANDROID_VULKAN")

set(SRC_FILES
    src/main/cpp/vulkan_wrapper.cpp
    ../../src/VulkanInstance.cpp
    ../../src/VulkanSwapchain.cpp
    ../../src/VulkanBuffer.cpp
    ../../src/VulkanTexture.cpp
    ../../src/VulkanGraphicsPipeline.cpp
    ../../src/VulkanRenderer.cpp
    ../../src/vkUtils.cpp
    src/main/cpp/android_main.cpp
    src/main/cpp/shared_utils.cpp
    src/main/cpp/mesh_data.cpp
    src/main/cpp/example_cube.cpp
    src/main/cpp/example_menu.cpp
    src/main/cpp/example_postprocess.cpp
    src/main/cpp/text_renderer.cpp
)

add_library(vk-example SHARED ${SRC_FILES})

target_link_libraries(vk-example android log native_app_glue)
