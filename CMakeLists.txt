# KH PAX Viewer: plays KINGDOM HEARTS effects (KH2's .pax, KH1's and Re:Chain of Memories' .dpx) from files the
# user extracted from their own copy of the games. Nothing of the games is in this repository.
cmake_minimum_required(VERSION 3.25)
project(kh_pax_viewer VERSION 0.1.1 LANGUAGES C CXX)
if(WIN32)
    enable_language(RC) # the icon and version (resources/viewer.rc)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
include(FetchContent)

# SDL3, static: the window, input, the OpenGL context and the file dialogs.
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    SDL3
    URL https://github.com/libsdl-org/SDL/releases/download/release-3.4.12/SDL3-3.4.12.tar.gz
    URL_HASH SHA256=f07b958a9ac5020fb7a44cadb957f658b2149c3c8abb4f63145fac9303249db7
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(SDL3)

# Dear ImGui (MIT): only its sources.
FetchContent_Declare(
    imgui
    URL https://github.com/ocornut/imgui/archive/refs/tags/v1.92.9b.tar.gz
    URL_HASH SHA256=21d8a0a565e85dce943e375db00812c2f3f0ab21f3f0f7964e364a63422d7f99
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR none
)
FetchContent_MakeAvailable(imgui)

add_library(pax-imgui STATIC
    "${imgui_SOURCE_DIR}/imgui.cpp"
    "${imgui_SOURCE_DIR}/imgui_draw.cpp"
    "${imgui_SOURCE_DIR}/imgui_tables.cpp"
    "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp")
target_include_directories(pax-imgui PUBLIC "${imgui_SOURCE_DIR}" "${imgui_SOURCE_DIR}/backends")
target_link_libraries(pax-imgui PUBLIC SDL3::SDL3-static)

# The effect player: the same files as khdays-port's assets/src/kh2_effect.cpp and its header
# (tools/sync_player.py copies them across).
add_library(pax-player STATIC src/player/kh2_effect.cpp)
target_include_directories(pax-player PUBLIC src/player/include)

add_library(pax-core STATIC src/library.cpp src/names.cpp src/gl.cpp src/renderer.cpp src/camera.cpp)
target_include_directories(pax-core PUBLIC src)
target_link_libraries(pax-core PUBLIC pax-player SDL3::SDL3-static)

add_executable(kh-pax-viewer WIN32 src/main.cpp src/app.cpp)
if(WIN32)
    target_sources(kh-pax-viewer PRIVATE resources/viewer.rc)
endif()
target_link_libraries(kh-pax-viewer PRIVATE pax-core pax-imgui)
target_compile_definitions(kh-pax-viewer PRIVATE
    PAX_VERSION_MAJOR=${PROJECT_VERSION_MAJOR} PAX_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    PAX_VERSION_PATCH=${PROJECT_VERSION_PATCH} PAX_VERSION_STRING="${PROJECT_VERSION}")
set_target_properties(kh-pax-viewer PROPERTIES OUTPUT_NAME "KH PAX Viewer")

if(MSVC)
    foreach(t pax-player pax-core kh-pax-viewer)
        target_compile_options(${t} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:/W4;/utf-8;/permissive->")
    endforeach()
endif()

# Tests: made-up files, nothing of the games.
enable_testing()
add_executable(pax-library-test tests/library_test.cpp)
target_link_libraries(pax-library-test PRIVATE pax-core)
add_test(NAME library COMMAND pax-library-test)
