cmake_minimum_required(VERSION 3.31)
set(TARGET vulkan-app)

project(VulkanExample)

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)

include_directories(${Vulkan_INCLUDE_DIRS})

add_executable(${TARGET} vkApp.cpp simple.cpp)

target_link_libraries(${TARGET} PRIVATE glfw ${Vulkan_LIBRARIES})
target_compile_features(${TARGET} PUBLIC cxx_std_11)