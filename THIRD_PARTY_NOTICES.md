# Third-party notices

KH PAX Viewer downloads and builds these libraries; none of their files are kept in this repository.

## SDL3

Project: Simple DirectMedia Layer, <https://github.com/libsdl-org/SDL>

Pinned release: **3.4.12**, fetched by CMake (`CMakeLists.txt`) and checked by its SHA-256.

Usage: compiled as a static library: the window, input, the OpenGL context and the file dialogs.

License: **zlib License**, copyright (c) 1997-2026 Sam Lantinga.

## Dear ImGui

Project: Dear ImGui, by Omar Cornut, <https://github.com/ocornut/imgui>

Pinned version: **v1.92.9b**, fetched by CMake (`CMakeLists.txt`) and checked by its SHA-256.

Usage: compiled unmodified as a static library: `imgui.cpp`, `imgui_draw.cpp`, `imgui_tables.cpp`,
`imgui_widgets.cpp`, `backends/imgui_impl_sdl3.cpp` and `backends/imgui_impl_opengl3.cpp`.

License: **MIT**, copyright (c) 2014-2026 Omar Cornut.
