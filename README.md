# Vulkan Example

Ejemplos mínimos de Vulkan en C++ para Windows usando GLFW y un pequeño wrapper de conveniencia. Incluye muestras de triángulo, atributos de vértice, texturas, profundidad (depth), escena 3D con luces y un paso de postproceso.

## Requisitos
- Windows 10/11
- `cmake` ≥ 3.31
- Compilador C++ con soporte de C++11 (p. ej. Visual Studio 2022/MSVC)
- Vulkan SDK instalado (incluye `glslc.exe` y capas de validación)
- GLFW 3 instalado y detectable por CMake (`find_package(glfw3 REQUIRED)`)

## Construcción
1) Generar el proyecto (Visual Studio 2022 x64):
```
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
```

2) Compilar shaders a SPIR-V (usa `glslc.exe` del Vulkan SDK):
```
cpu.bat
```
Esto compila `shaders/*.vert` y `shaders/*.frag` a `build\Release\*.spv`.

3) Compilar binarios:
```
cmake --build build --config Release
```

Alternativa (generadores single-config):
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Ejecución
Los ejecutables se generan en `build\Release`:
- `vulkan-simple`: triángulo básico.
- `vulkan-vertex`: vértices con posición y UV.
- `vulkan-texture`: muestreo de textura; coloca `texture.jpg` junto al ejecutable.
- `vulkan-depth`: render con z-buffer.
- `vulkan-3d`: cubo texturizado con tres luces. Controles:
  - Botón izquierdo: orbitar cámara
  - Botón derecho: mover cámara
- `vulkan-proc`: render offscreen y postproceso (usa `contrast.vert/frag`).

## Estructura
- `vkApp.h` / `vkApp.cpp`: wrapper con clases principales:
  - `VulkanInstance` (`vkApp.h:20`)
  - `VulkanSwapchain` (`vkApp.h:48`)
  - `VulkanGraphicsPipeline` (`vkApp.h:80`)
  - `VulkanRenderer` (`vkApp.h:111`)
  - `VulkanBuffer` (`vkApp.h:138`)
  - `VulkanTexture` (`vkApp.h:162`)
- Ejemplos: `simple.cpp`, `vertex.cpp`, `texture.cpp`, `depth.cpp`, `cube.cpp`, `postprocess.cpp`.
- Shaders: `shaders/*.vert`, `shaders/*.frag` → `.spv` vía `glslc`.
- Dependencias incluidas: `glm/` (header-only), `stb_image.h`.

## Notas
- Los ejemplos usan `debug_app = true` para activar capas de validación si el Vulkan SDK está presente.
- Si CMake no encuentra `glfw3`, puedes instalarlo con vcpkg y pasar el toolchain:
```
vcpkg install glfw3:x64-windows
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE=C:\path\to\vcpkg\scripts\buildsystems\vcpkg.cmake
```
- Asegúrate de que `glslc.exe` esté en `PATH` o ajusta `GLSLC` en `cpu.bat`.
