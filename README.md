# Vulkan Example

Minimal Vulkan examples in C++ for Windows using GLFW and a small convenience wrapper. Includes samples for triangle, vertex attributes, textures, depth, 3D scene with lights, post-processing, text rendering, and compute particle systems.

## Requirements
- Windows 10/11
- `cmake` >= 3.31
- C++ compiler with C++11 support
- Vulkan SDK installed (includes `glslc.exe` and validation layers)
- GLFW 3 installed and detectable by CMake (`find_package(glfw3 REQUIRED)`)

## Building
1) Generate the project:
```powershell
cmake -S . -B build
```

2) Build binaries:
```powershell
cmake --build build --config Release
```

## Running
Executables are generated in `build\Release`:

| Executable | Description |
|------------|-------------|
| `vulkan-simple` | Basic triangle. |
| `vulkan-vertex` | Vertices with position and UV. |
| `vulkan-texture` | Texture sampling; place `texture.jpg` next to the executable. |
| `vulkan-depth` | Render with z-buffer. |
| `vulkan-3d` | Textured cube with three lights. Controls: Left mouse button to orbit camera. |
| `vulkan-proc` | Offscreen rendering and post-processing (uses `contrast.vert/frag`). |
| `vulkan-text` | Text rendering using a bitmap font (`font_texture.png`). |
| `vulkan-particles` | GPU compute particle system. |
| `vulkan-particles-explosion` | Explosion particle effect (compute). |
| `vulkan-particles-smoke` | Smoke particle effect (compute). |
| `vulkan-particles-vortex` | Vortex particle effect (compute). |

### Assets
The build system automatically copies the required runtime assets next to each executable on build. They are located in the `assets/` directory:
- `texture.jpg` – used by `vulkan-texture` and `vulkan-3d`.
- `font_texture.png` – used by `vulkan-text`.
- `windows.fft` – used by text rendering examples.

## Shaders
The `shaders/` directory contains GLSL source files that are compiled automatically by CMake to `.spv` using `glslc` during the build process, and then copied next to each executable.

**Graphics shaders:**
- `simple.vert` / `simple.frag`
- `vertex.vert` / `vertex.frag`
- `texture.vert` / `texture.frag`
- `cube.vert` / `cube.frag`
- `contrast.vert` / `contrast.frag`
- `text.vert` / `text.frag`
- `particles.vert` / `particles.frag`

**Compute shaders:**
- `particles.comp`
- `explosion.comp`
- `smoke.comp`
- `vortex.comp`

**Post-processing:**
- `brightness.frag`
- `blur.frag`
- `bloom_final.frag`

## Structure
- `vkApp.h` / `vkApp.cpp`: wrapper with main classes.
- `src/`: modular Vulkan wrapper components
  - `VulkanInstance.h` / `.cpp`
  - `VulkanSwapchain.h` / `.cpp`
  - `VulkanBuffer.h` / `.cpp`
  - `VulkanTexture.h` / `.cpp`
  - `VulkanGraphicsPipeline.h` / `.cpp`
  - `VulkanRenderer.h` / `.cpp`
  - `vkCommon.h`
  - `vkUtils.h` / `.cpp`
- `examples/`: source code for desktop examples
  - `simple.cpp`, `vertex.cpp`, `texture.cpp`, `depth.cpp`, `cube.cpp`, `postprocess.cpp`
  - `text.cpp`
  - `particles.cpp`, `particles_explosion.cpp`, `particles_smoke.cpp`, `particles_vortex.cpp`
- `shaders/`: GLSL files (`*.vert`, `*.frag`, `*.comp`) that compile to `.spv`.
- `android/`: experimental Android support with Gradle project and additional examples.
- `assets/`: runtime assets (`texture.jpg`, `font_texture.png`, `windows.fft`).
- Dependencies: `stb_image.h`, `graphics_math.h` (custom math utility).
