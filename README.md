# Vulkan Example

Minimal Vulkan examples in C++ for Windows using GLFW and a small convenience wrapper. Includes samples for triangle, vertex attributes, textures, depth, 3D scene with lights, and a post-processing pass.

## Requirements
- Windows 10/11
- `cmake` ≥ 3.31
- C++ compiler with C++11 support
- Vulkan SDK installed (includes `glslc.exe` and validation layers)
- GLFW 3 installed and detectable by CMake (`find_package(glfw3 REQUIRED)`)

## Building
1) Generate the project:
```powershell
cmake -S . -B build
```

### Manual Alternative:
Compile shaders:
```powershell
glslc shaders/simple.vert -o build/Release/simple.vert.spv
# ... (repeat for other shaders)
```
Build binaries:
```powershell
cmake --build build --config Release
```

## Running
Executables are generated in `build\Release`:
- `vulkan-simple`: basic triangle.
- `vulkan-vertex`: vertices with position and UV.
- `vulkan-texture`: texture sampling; place `texture.jpg` next to the executable.
- `vulkan-depth`: render with z-buffer.
- `vulkan-3d`: textured cube with three lights. Controls:
  - Left mouse button: orbit camera
- `vulkan-proc`: offscreen rendering and post-processing (uses `contrast.vert/frag`).

## Structure
- `vkApp.h` / `vkApp.cpp`: wrapper with main classes.
- `vkUtils.h`: utility functions for device selection, extensions, and queue families.
- `examples/`: source code for examples (`simple.cpp`, `vertex.cpp`, `texture.cpp`, `depth.cpp`, `cube.cpp`, `postprocess.cpp`).
- `shaders/`: GLSL files (`*.vert`, `*.frag`) that compile to `.spv`.
- `android/`: experimental Android support (includes `cube_android.cpp` and a specific wrapper).
- Dependencies: `stb_image.h`, `graphics_math.h` (custom math utility).
