cmake_minimum_required(VERSION 3.31)
set(TARGET vulkan-wrapper)

project(VulkanExample)

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)

include_directories(${Vulkan_INCLUDE_DIRS} .)

# Shader compilation
find_program(GLSLC glslc HINTS $ENV{VULKAN_SDK}/Bin $ENV{VULKAN_SDK}/bin)
if(NOT GLSLC)
    message(FATAL_ERROR "glslc not found. Please install the Vulkan SDK.")
endif()

set(SHADER_SOURCE_DIR ${CMAKE_SOURCE_DIR}/shaders)
set(SHADER_BINARY_DIR ${CMAKE_BINARY_DIR}/shaders)
file(MAKE_DIRECTORY ${SHADER_BINARY_DIR})

set(SHADER_SOURCES
    simple.vert simple.frag
    vertex.vert vertex.frag
    texture.vert texture.frag
    cube.vert cube.frag
    contrast.vert contrast.frag
    text.vert text.frag
    particles.vert particles.frag
    particles.comp
    explosion.comp
    smoke.comp
    vortex.comp
    brightness.frag
    blur.frag
    bloom_final.frag
)

set(SHADER_OUTPUTS "")
foreach(SHADER ${SHADER_SOURCES})
    set(SHADER_INPUT ${SHADER_SOURCE_DIR}/${SHADER})
    set(SHADER_OUTPUT ${SHADER_BINARY_DIR}/${SHADER}.spv)
    add_custom_command(
        OUTPUT ${SHADER_OUTPUT}
        COMMAND ${GLSLC} ${SHADER_INPUT} -o ${SHADER_OUTPUT}
        DEPENDS ${SHADER_INPUT}
        COMMENT "Compiling ${SHADER}"
    )
    list(APPEND SHADER_OUTPUTS ${SHADER_OUTPUT})
endforeach()

add_custom_target(Shaders ALL DEPENDS ${SHADER_OUTPUTS})

add_library(${TARGET} STATIC 
    src/VulkanInstance.cpp 
    src/VulkanSwapchain.cpp 
    src/VulkanBuffer.cpp 
    src/VulkanTexture.cpp 
    src/VulkanGraphicsPipeline.cpp 
    src/VulkanRenderer.cpp 
    src/vkUtils.cpp
    examples/utils.cpp
)
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

add_executable(vulkan-text examples/text.cpp)
target_link_libraries(vulkan-text PRIVATE glfw ${TARGET})

add_executable(vulkan-particles examples/particles.cpp)
target_link_libraries(vulkan-particles PRIVATE glfw ${TARGET})

add_executable(vulkan-particles-explosion examples/particles_explosion.cpp)
target_link_libraries(vulkan-particles-explosion PRIVATE glfw ${TARGET})

add_executable(vulkan-particles-smoke examples/particles_smoke.cpp)
target_link_libraries(vulkan-particles-smoke PRIVATE glfw ${TARGET})

add_executable(vulkan-particles-vortex examples/particles_vortex.cpp)
target_link_libraries(vulkan-particles-vortex PRIVATE glfw ${TARGET})

# Copy runtime assets next to each executable on build
set(ASSET_FILES
    ${CMAKE_SOURCE_DIR}/assets/texture.jpg
    ${CMAKE_SOURCE_DIR}/assets/font_texture.png
    ${CMAKE_SOURCE_DIR}/assets/windows.fft
)

set(EXAMPLE_TARGETS
    vulkan-simple
    vulkan-vertex
    vulkan-texture
    vulkan-depth
    vulkan-3d
    vulkan-proc
    vulkan-text
    vulkan-particles
    vulkan-particles-explosion
    vulkan-particles-smoke
    vulkan-particles-vortex
)

foreach(EXAMPLE ${EXAMPLE_TARGETS})
    add_dependencies(${EXAMPLE} Shaders)
    foreach(ASSET ${ASSET_FILES})
        add_custom_command(TARGET ${EXAMPLE} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${ASSET} $<TARGET_FILE_DIR:${EXAMPLE}>
        )
    endforeach()
    foreach(SHADER_OUT ${SHADER_OUTPUTS})
        add_custom_command(TARGET ${EXAMPLE} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${SHADER_OUT} $<TARGET_FILE_DIR:${EXAMPLE}>
        )
    endforeach()
endforeach()

target_compile_features(${TARGET} PUBLIC cxx_std_11)