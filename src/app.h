// The viewer's window: the files loaded (by game, kind of file, world or kind of object, file, effect, block,
// emitter), the effect playing in a 3D view, its controls and the settings kept between runs.
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
    // the list on the left, as built from sources_ and the filter
    struct TreeFile {
        int source = -1;
        std::string label, note; // its name (with its copy when another file has the same), the world in grey
    };
    struct TreeSection {
        std::string label; // "" for the files of no section, listed after the sections
        std::vector<TreeFile> files;
    };
    struct TreeGroup {
        std::string label;
        std::vector<TreeSection> sections;
        std::size_t files = 0;
    };
    struct TreeGame {
        Game game = Game::Unknown;
        std::vector<TreeGroup> groups;
        std::size_t files = 0;
    };
    struct Where {
        int game = -1, group = -1, section = -1; // game -1: not listed (the filter leaves it out)
    };

    void tree();
    void build_tree();
    void draw_file(const TreeFile &f);
    void file_tooltip(const Source &s) const;
    void controls();
    void pages();
    void select(const Pick &p);
    Pick first_pick(int source, int effect = 0, int block = 0, int emitter = 0) const; // -1s when it has none
    void step(int by, bool whole_file); // the emitter (or file) before or after the one chosen, in the list's order
    void text_size(float to);
    void restart();
    void advance(double seconds);
    void replay(); // the instance run again to the frame shown, with the camera as it is now
    void tick(bool keep);
    const Source *picked_source() const;
    std::shared_ptr<const kh2::EffectBlock> picked_block() const;
    std::string picked_title() const;
    void load_settings();

    SDL_Window *window_ = nullptr;
    Library library_;
    Renderer renderer_;
    Orbit orbit_;
    std::vector<Source> sources_;
    unsigned changes_seen_ = ~0u; // the library's changes() when sources_ was taken

    std::vector<TreeGame> tree_;
    std::vector<Where> where_; // each source's place in tree_
    std::vector<Pick> order_;  // every emitter listed, in the list's order (what the arrow keys step through)
    std::string tree_filter_;  // the filter tree_ was built with
    bool tree_stale_ = true;
    bool reveal_ = false;   // this frame: the list opened down to the emitter chosen, scrolled to it
    bool expand_ = false;   // this frame: every group and section with a file listed opened (a new filter)
    bool collapse_ = false; // this frame: every group closed

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
    float panel_width_ = 420.0F; // the list's width, dragged by its right edge
    float bar_height_ = 120.0F;  // the controls' height, as tall as they last were
    float text_size_ = 1.0F;

    // the emitter chosen when the viewer was last closed, chosen again once its file is loaded
    std::string restore_key_;
    int restore_effect_ = 0, restore_block_ = 0, restore_emitter_ = 0;

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
