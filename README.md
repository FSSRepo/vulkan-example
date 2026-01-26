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
```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
```

2) Compilar shaders y binarios:
Puedes usar el script `run.bat` que automatiza la compilación de shaders a SPIR-V y la construcción del proyecto:
```powershell
run.bat
```
Esto compila `shaders/*.vert` y `shaders/*.frag` a `build\Release\*.spv` y luego compila los binarios en `build\Release`.

### Alternativa manual:
Compilar shaders:
```powershell
glslc shaders/simple.vert -o build/Release/simple.vert.spv
# ... (repetir para otros shaders)
```
Compilar binarios:
```powershell
cmake --build build --config Release
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
- `vkApp.h` / `vkApp.cpp`: wrapper con clases principales.
- `vkUtils.h`: funciones de utilidad para selección de dispositivos, extensiones y familias de colas.
- `examples/`: código fuente de los ejemplos (`simple.cpp`, `vertex.cpp`, `texture.cpp`, `depth.cpp`, `cube.cpp`, `postprocess.cpp`).
- `shaders/`: archivos GLSL (`*.vert`, `*.frag`) que se compilan a `.spv`.
- `android/`: soporte experimental para Android (incluye `cube_android.cpp` y un wrapper específico).
- Dependencias: `stb_image.h`, `graphics_math.h` (utilidad matemática propia).

## Notas
- Los ejemplos usan `debug_app = true` para activar capas de validación si el Vulkan SDK está presente.
- Si CMake no encuentra `glfw3`, puedes instalarlo con vcpkg y pasar el toolchain:
```powershell
vcpkg install glfw3:x64-windows
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE=C:\path\to\vcpkg\scripts\buildsystems\vcpkg.cmake
```
- Asegúrate de que `glslc.exe` esté en `PATH` para que `run.bat` funcione correctamente.
