cmake_minimum_required(VERSION 3.31)
set(TARGET vulkan-wrapper)

project(VulkanExample)

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)

include_directories(${Vulkan_INCLUDE_DIRS} .)

add_library(${TARGET} STATIC vkApp.cpp)
target_link_libraries(${TARGET} PRIVATE glfw ${Vulkan_LIBRARIES})

add_executable(vulkan-simple examples/simple.cpp)
target_link_libraries(vulkan-simple PRIVATE glfw ${TARGET})

add_executable(vulkan-vertex examples/vertex.cpp)
target_link_libraries(vulkan-vertex PRIVATE glfw ${TARGET})

add_executable(vulkan-texture examples/texture.cpp)
target_link_libraries(vulkan-texture PRIVATE glfw ${TARGET})

add_executable(vulkan-depth examples/depth.cpp)
target_link_libraries(vulkan-depth PRIVATE glfw ${TARGET})

add_executable(vulkan-3d examples/cube.cpp)
target_link_libraries(vulkan-3d PRIVATE glfw ${TARGET})

add_executable(vulkan-proc examples/postprocess.cpp)
target_link_libraries(vulkan-proc PRIVATE glfw ${TARGET})

target_compile_features(${TARGET} PUBLIC cxx_std_11)