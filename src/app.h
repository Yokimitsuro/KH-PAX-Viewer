// The viewer's window: the files loaded (by game, file, effect, block, emitter), the effect playing in a 3D view,
// its controls and the settings kept between runs.
#pragma once

#include "camera.h"
#include "library.h"
#include "renderer.h"

#include <SDL3/SDL.h>

#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace pax {

class App {
public:
    bool init(SDL_Window *window, std::string &error);
    void event(const SDL_Event &e);
    void frame(int width, int height); // the UI built and the 3D view drawn (ImGui's frame already begun)
    void open(std::vector<std::filesystem::path> paths);
    void queue(std::vector<std::filesystem::path> paths); // from another thread (the file dialogs)
    void save_settings() const;
    // once loading ends, play the first file whose name holds `name` (its block and emitter)
    void play_when_loaded(std::string name, int block, int emitter);

private:
    struct Pick {
        int source = -1, effect = -1, block = -1, emitter = -1;
        bool operator==(const Pick &) const = default;
    };

    void tree();
    void controls();
    void pages();
    void select(const Pick &p);
    void restart();
    void advance(double seconds);
    void replay(); // the instance run again to the frame shown, with the camera as it is now
    void tick(bool keep);
    const Source *picked_source() const;
    std::shared_ptr<const kh2::EffectBlock> picked_block() const;
    void load_settings();

    SDL_Window *window_ = nullptr;
    Library library_;
    Renderer renderer_;
    Orbit orbit_;
    std::vector<Source> sources_;
    std::size_t sources_seen_ = 0;
    bool was_busy_ = false;

    Pick pick_;
    std::unique_ptr<kh2::EffectInstance> fx_;
    std::string refused_;
    std::vector<kh2::EffectDraw> draws_;
    std::vector<const kh2::EffectPicture *> pictures_;
    std::vector<kh2::EffectPicture> resident_;
    int frame_ = 0;
    double carry_ = 0;
    bool playing_ = true, loop_ = true, swing_ = false, grid_ = true, pages_window_ = false;
    float speed_ = 1.0F;
    float background_[3] = {0.08F, 0.08F, 0.1F};
    bool deep_ = false;
    char filter_[128] = {};
    std::filesystem::path last_folder_;

    int view_x_ = 0, view_y_ = 0, view_w_ = 1, view_h_ = 1; // the 3D view, in window pixels (y down)
    bool dragging_ = false;
    int drag_button_ = 0;
    bool camera_moved_ = false;

    std::string wanted_;
    int wanted_block_ = 0, wanted_emitter_ = 0;

    std::mutex queued_lock_;
    std::vector<std::filesystem::path> queued_;
};

} // namespace pax
