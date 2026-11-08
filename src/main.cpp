// KH PAX Viewer: the window, OpenGL 3.3 and Dear ImGui around the App.
#include "app.h"
#include "gl.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>

#include <cstdlib>
#include <string>
#include <vector>

int main(int argc, char **argv)
{
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "KH PAX Viewer", SDL_GetError(), nullptr);
        return 1;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_Window *window = SDL_CreateWindow("KH PAX Viewer " PAX_VERSION_STRING, 1600, 900,
                                          SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    SDL_GLContext context = window != nullptr ? SDL_GL_CreateContext(window) : nullptr;
    const char *missing = nullptr;
    if (context == nullptr || !pax::gl.load(&missing)) {
        const std::string why = context == nullptr ? std::string(SDL_GetError())
                                                   : std::string("OpenGL 3.3 lacks ") + (missing ? missing : "?");
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "KH PAX Viewer", why.c_str(), window);
        return 1;
    }
    SDL_GL_MakeCurrent(window, context);
    SDL_GL_SetSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = nullptr; // the layout is fixed; the settings are the viewer's own
    ImGui::StyleColorsDark();
    ImGui_ImplSDL3_InitForOpenGL(window, context);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    pax::App app;
    std::string error;
    if (!app.init(window, error)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "KH PAX Viewer", error.c_str(), window);
        return 1;
    }
    // KH PAX Viewer [files or folders...] [--play <name> <block> <emitter>]
    std::vector<std::filesystem::path> args;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--play" && i + 3 < argc) {
            app.play_when_loaded(argv[i + 1], std::atoi(argv[i + 2]), std::atoi(argv[i + 3]));
            i += 3;
            continue;
        }
        args.emplace_back(std::filesystem::path(std::u8string(reinterpret_cast<const char8_t *>(argv[i]))));
    }
    if (!args.empty())
        app.open(args);

    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            ImGui_ImplSDL3_ProcessEvent(&e);
            if (e.type == SDL_EVENT_QUIT ||
                (e.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && e.window.windowID == SDL_GetWindowID(window)))
                running = false;
            app.event(e);
        }
        if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) {
            SDL_Delay(20);
            continue;
        }
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        int w = 0, h = 0;
        SDL_GetWindowSizeInPixels(window, &w, &h);
        app.frame(w, h);
        ImGui::Render();
        pax::gl.Viewport(0, 0, w, h);
        pax::gl.Disable(GL_SCISSOR_TEST);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }
    app.save_settings();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DestroyContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
