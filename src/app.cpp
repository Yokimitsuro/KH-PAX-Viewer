#include "app.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <set>
#include <sstream>

namespace pax {

namespace {

constexpr float panel_width = 380.0F, bar_height = 118.0F;
constexpr double tick_seconds = 1.0 / 60.0; // KINGDOM HEARTS II's effects tick 60 times a second

std::filesystem::path from_utf8(const char *s)
{
    return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t *>(s)));
}

std::string to_utf8(const std::filesystem::path &p)
{
    const auto u = p.generic_u8string();
    return std::string(u.begin(), u.end());
}

std::filesystem::path settings_file()
{
    char *dir = SDL_GetPrefPath("KH PAX Viewer", "KH PAX Viewer");
    if (dir == nullptr)
        return {};
    std::filesystem::path p = from_utf8(dir) / "settings.ini";
    SDL_free(dir);
    return p;
}

void on_files(void *self, const char *const *list, int)
{
    if (list == nullptr)
        return;
    std::vector<std::filesystem::path> paths;
    for (; *list != nullptr; ++list)
        paths.push_back(from_utf8(*list));
    if (!paths.empty())
        static_cast<App *>(self)->queue(std::move(paths));
}

std::string lower(std::string s)
{
    for (auto &c : s)
        c = char(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

} // namespace

bool App::init(SDL_Window *window, std::string &error)
{
    window_ = window;
    if (!renderer_.init(error))
        return false;
    load_settings();
    if (!last_folder_.empty() && std::filesystem::is_directory(last_folder_))
        library_.open({last_folder_}, deep_);
    return true;
}

void App::load_settings()
{
    std::ifstream in(settings_file());
    std::string line;
    while (std::getline(in, line)) {
        const auto eq = line.find('=');
        if (eq == std::string::npos)
            continue;
        const std::string key = line.substr(0, eq), value = line.substr(eq + 1);
        if (key == "folder")
            last_folder_ = from_utf8(value.c_str());
        else if (key == "deep")
            deep_ = value == "1";
        else if (key == "grid")
            grid_ = value == "1";
        else if (key == "smooth")
            renderer_.smooth = value == "1";
        else if (key == "background") {
            std::istringstream in3(value);
            in3 >> background_[0] >> background_[1] >> background_[2];
        }
    }
}

void App::save_settings() const
{
    const auto file = settings_file();
    if (file.empty())
        return;
    std::ofstream out(file);
    out << "folder=" << to_utf8(last_folder_) << "\n";
    out << "deep=" << (deep_ ? 1 : 0) << "\n";
    out << "grid=" << (grid_ ? 1 : 0) << "\n";
    out << "smooth=" << (renderer_.smooth ? 1 : 0) << "\n";
    out << "background=" << background_[0] << " " << background_[1] << " " << background_[2] << "\n";
}

void App::play_when_loaded(std::string name, int block, int emitter)
{
    wanted_ = lower(std::move(name));
    wanted_block_ = block;
    wanted_emitter_ = emitter;
}

void App::queue(std::vector<std::filesystem::path> paths)
{
    std::lock_guard<std::mutex> g(queued_lock_);
    queued_.insert(queued_.end(), paths.begin(), paths.end());
}

void App::open(std::vector<std::filesystem::path> paths)
{
    if (paths.empty())
        return;
    // a folder opened becomes the one loaded again at the next start
    for (const auto &p : paths)
        if (std::filesystem::is_directory(p))
            last_folder_ = p;
    select({});
    library_.clear();
    sources_.clear();
    renderer_.forget();
    library_.open(std::move(paths), deep_);
    save_settings();
}

const Source *App::picked_source() const
{
    if (pick_.source < 0 || std::size_t(pick_.source) >= sources_.size())
        return nullptr;
    return &sources_[std::size_t(pick_.source)];
}

std::shared_ptr<const kh2::EffectBlock> App::picked_block() const
{
    const Source *s = picked_source();
    if (s == nullptr || pick_.effect < 0 || std::size_t(pick_.effect) >= s->effects.size())
        return nullptr;
    const auto &e = s->effects[std::size_t(pick_.effect)];
    if (pick_.block < 0 || std::size_t(pick_.block) >= e.blocks.size())
        return nullptr;
    return e.blocks[std::size_t(pick_.block)];
}

void App::select(const Pick &p)
{
    pick_ = p;
    fx_.reset();
    refused_.clear();
    draws_.clear();
    frame_ = 0;
    carry_ = 0;
    const auto block = picked_block();
    if (!block || pick_.emitter < 0)
        return;
    resident_ = library_.resident(picked_source()->game);
    pictures_.clear();
    for (const auto &pic : block->pictures)
        pictures_.push_back(&pic);
    for (const auto &pic : resident_)
        pictures_.push_back(&pic);
    restart();
}

void App::restart()
{
    fx_.reset();
    refused_.clear();
    draws_.clear();
    frame_ = 0;
    carry_ = 0;
    const auto block = picked_block();
    if (!block || pick_.emitter < 0)
        return;
    try {
        fx_ = std::make_unique<kh2::EffectInstance>(block, std::size_t(pick_.emitter), 3u);
        tick(true);
    } catch (const std::exception &e) {
        fx_.reset();
        refused_ = e.what();
    }
}

void App::tick(bool keep)
{
    if (!fx_)
        return;
    if (swing_) { // the game's points: a weapon's base and tip swung round (the swing trails read them)
        const float a = 0.1F * float(frame_);
        fx_->set_points({{{0, -100, 0}, {}}, {{100 * std::cos(a), -100 + 100 * std::sin(a), 0}, {}}});
    }
    const float aspect = float(view_w_) / float(std::max(1, view_h_));
    try {
        if (keep) {
            draws_.clear();
            fx_->tick(orbit_.camera(aspect), &draws_);
        } else {
            fx_->tick(orbit_.camera(aspect), nullptr);
        }
        ++frame_;
    } catch (const std::exception &e) {
        refused_ = e.what();
        fx_.reset();
    }
}

void App::advance(double seconds)
{
    if (!fx_ || !playing_)
        return;
    carry_ += seconds * double(speed_);
    int steps = int(carry_ / tick_seconds);
    carry_ -= double(steps) * tick_seconds;
    steps = std::min(steps, 8); // a slow frame does not run away
    for (int i = 0; i < steps && fx_; ++i) {
        tick(i == steps - 1);
        if (fx_ && fx_->done()) {
            if (loop_)
                restart();
            else
                playing_ = false;
            break;
        }
    }
}

void App::replay()
{
    if (!fx_)
        return;
    const int to = frame_;
    const auto block = picked_block();
    try {
        fx_ = std::make_unique<kh2::EffectInstance>(block, std::size_t(pick_.emitter), 3u);
        frame_ = 0;
        while (frame_ < to - 1 && fx_)
            tick(false);
        tick(true);
    } catch (const std::exception &e) {
        fx_.reset();
        refused_ = e.what();
    }
}

void App::event(const SDL_Event &e)
{
    const ImGuiIO &io = ImGui::GetIO();
    switch (e.type) {
    case SDL_EVENT_DROP_FILE:
        if (e.drop.data != nullptr)
            open({from_utf8(e.drop.data)});
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN: {
        const float x = e.button.x, y = e.button.y;
        const bool inside = x >= float(view_x_) && x < float(view_x_ + view_w_) && y >= float(view_y_) &&
                            y < float(view_y_ + view_h_);
        if (inside && !io.WantCaptureMouse) {
            dragging_ = true;
            drag_button_ = e.button.button;
        }
        break;
    }
    case SDL_EVENT_MOUSE_BUTTON_UP:
        dragging_ = false;
        break;
    case SDL_EVENT_MOUSE_MOTION:
        if (dragging_) {
            const bool pan = drag_button_ != SDL_BUTTON_LEFT || (SDL_GetModState() & SDL_KMOD_SHIFT) != 0;
            if (pan)
                orbit_.pan(e.motion.xrel, e.motion.yrel, float(view_h_));
            else
                orbit_.turn(e.motion.xrel, e.motion.yrel);
            camera_moved_ = true;
        }
        break;
    case SDL_EVENT_MOUSE_WHEEL:
        if (!io.WantCaptureMouse) {
            orbit_.zoom(e.wheel.y);
            camera_moved_ = true;
        }
        break;
    case SDL_EVENT_KEY_DOWN:
        if (io.WantCaptureKeyboard)
            break;
        if (e.key.key == SDLK_SPACE)
            playing_ = !playing_;
        else if (e.key.key == SDLK_PERIOD && fx_ && !playing_)
            tick(true);
        else if (e.key.key == SDLK_R)
            restart();
        break;
    default: break;
    }
}

void App::tree()
{
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(panel_width, ImGui::GetIO().DisplaySize.y));
    ImGui::Begin("Files", nullptr,
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoTitleBar);
    if (ImGui::Button("Open files..."))
        SDL_ShowOpenFileDialog(on_files, this, window_, nullptr, 0, nullptr, true);
    ImGui::SameLine();
    if (ImGui::Button("Open folder..."))
        SDL_ShowOpenFolderDialog(on_files, this, window_, last_folder_.empty() ? nullptr : to_utf8(last_folder_).c_str(),
                                 false);
    ImGui::SameLine();
    if (ImGui::Checkbox("Look inside", &deep_))
        save_settings();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Look for effects inside any file, not only PAX, DPX and BAR files\n"
                          "(Re:Chain of Memories keeps its effects inside its own files). Slower.");
    if (library_.busy())
        ImGui::Text("Loading... %d files looked at, %zu with effects", library_.seen(), sources_.size());
    else
        ImGui::Text("%zu file%s with effects", sources_.size(), sources_.size() == 1 ? "" : "s");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##filter", "Filter by name", filter_, sizeof filter_);
    ImGui::Separator();
    ImGui::BeginChild("tree");
    const std::string filter = lower(filter_);
    for (const Game g : {Game::KH2, Game::KH1, Game::ReCoM, Game::Unknown}) {
        std::vector<int> in;
        for (std::size_t i = 0; i < sources_.size(); ++i)
            if (sources_[i].game == g && (filter.empty() || lower(sources_[i].name).find(filter) != std::string::npos))
                in.push_back(int(i));
        if (in.empty())
            continue;
        std::sort(in.begin(), in.end(), [&](int a, int b) { return sources_[a].name < sources_[b].name; });
        const bool open = ImGui::TreeNodeEx(game_name(g), ImGuiTreeNodeFlags_DefaultOpen, "%s (%zu)", game_name(g),
                                            in.size());
        if (!open)
            continue;
        for (const int si : in) {
            const Source &s = sources_[std::size_t(si)];
            ImGui::PushID(si);
            if (ImGui::TreeNode(s.name.c_str())) {
                for (std::size_t ei = 0; ei < s.effects.size(); ++ei) {
                    const Effect &e = s.effects[ei];
                    ImGui::PushID(int(ei));
                    const bool one = s.effects.size() == 1 && e.label.empty();
                    const bool open_effect = one || ImGui::TreeNode(e.label.empty() ? "effect" : e.label.c_str());
                    if (open_effect) {
                        if (!e.error.empty())
                            ImGui::TextColored(ImVec4(1, 0.5F, 0.4F, 1), "not read: %s", e.error.c_str());
                        for (std::size_t bi = 0; bi < e.blocks.size(); ++bi) {
                            const auto &b = *e.blocks[bi];
                            ImGui::PushID(int(bi));
                            const bool open_block =
                                e.blocks.size() == 1 ||
                                ImGui::TreeNode("block", "block %zu (%zu emitters)", bi, b.emitters.size());
                            if (open_block) {
                                for (std::size_t m = 0; m < b.emitters.size(); ++m) {
                                    const auto &em = b.emitters[m];
                                    const Pick p{si, int(ei), int(bi), int(m)};
                                    char label[96];
                                    std::snprintf(label, sizeof label, "emitter %zu  (%d frames%s)", m,
                                                  em.length / 4096, em.repeat ? ", repeats" : "");
                                    if (ImGui::Selectable(label, pick_ == p))
                                        select(p);
                                }
                                if (e.blocks.size() != 1)
                                    ImGui::TreePop();
                            }
                            ImGui::PopID();
                        }
                        if (!one)
                            ImGui::TreePop();
                    }
                    ImGui::PopID();
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
        ImGui::TreePop();
    }
    ImGui::EndChild();
    ImGui::End();
}

void App::controls()
{
    const ImVec2 size = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(panel_width, size.y - bar_height));
    ImGui::SetNextWindowSize(ImVec2(size.x - panel_width, bar_height));
    ImGui::Begin("Controls", nullptr,
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoTitleBar);
    const Source *s = picked_source();
    if (s != nullptr && pick_.emitter >= 0)
        ImGui::Text("%s  %s  block %d  emitter %d", game_name(s->game), s->name.c_str(), pick_.block, pick_.emitter);
    else
        ImGui::TextDisabled("Choose an emitter on the left (open a folder of extracted game files first).");
    if (!refused_.empty())
        ImGui::TextColored(ImVec4(1, 0.5F, 0.4F, 1), "Cannot play it: %s", refused_.c_str());
    if (ImGui::Button(playing_ ? "Pause" : "Play"))
        playing_ = !playing_;
    ImGui::SameLine();
    ImGui::BeginDisabled(playing_ || !fx_);
    if (ImGui::Button("Step"))
        tick(true);
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Restart"))
        restart();
    ImGui::SameLine();
    ImGui::BeginDisabled(!fx_);
    if (ImGui::Button("Stop repeating") && fx_)
        fx_->stop();
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("A repeating effect closes as the game closes it, then ends.");
    ImGui::SameLine();
    ImGui::Checkbox("Loop", &loop_);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(140);
    ImGui::SliderFloat("Speed", &speed_, 0.1F, 4.0F, "%.2fx");
    ImGui::SameLine();
    if (ImGui::Button("Reset camera")) {
        orbit_.reset();
        camera_moved_ = true;
    }
    if (ImGui::Checkbox("Grid", &grid_))
        save_settings();
    ImGui::SameLine();
    if (ImGui::Checkbox("Smooth pictures", &renderer_.smooth)) {
        renderer_.forget();
        save_settings();
    }
    ImGui::SameLine();
    ImGui::Checkbox("Swing points", &swing_);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Give the effect two points swung round, as a weapon's base and tip\n"
                          "(the effects that draw a swing's trail follow them).");
    ImGui::SameLine();
    ImGui::Checkbox("Screen copies", &renderer_.show_screen);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Show the draws that copy the screen behind (heat haze, distortions) as plain shapes.");
    ImGui::SameLine();
    ImGui::Checkbox("Pages", &pages_window_);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(160);
    if (ImGui::ColorEdit3("Background", background_, ImGuiColorEditFlags_NoInputs))
        save_settings();
    if (fx_)
        ImGui::Text("frame %d   objects %zu   draws %zu   triangles %zu%s", frame_, fx_->objects(), draws_.size(),
                    renderer_.triangles, fx_->done() ? "   (ended)" : "");
    ImGui::End();
}

void App::pages()
{
    if (!pages_window_)
        return;
    ImGui::SetNextWindowSize(ImVec2(420, 480), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Pages drawn with", &pages_window_)) {
        ImGui::End();
        return;
    }
    const auto block = picked_block();
    std::set<std::uint64_t> seen;
    for (const auto &d : draws_)
        if (d.tex0 != 0)
            seen.insert(d.tex0);
    if (seen.empty())
        ImGui::TextDisabled("Nothing textured is drawn now.");
    for (const auto t : seen) {
        const unsigned tex = renderer_.page_texture(pictures_, block.get(), t);
        const int w = 1 << ((t >> 26) & 15), h = 1 << ((t >> 30) & 15);
        ImGui::Text("TBP %#llx  CBP %#llx  %dx%d", (unsigned long long)(t & 0x3fff),
                    (unsigned long long)((t >> 37) & 0x3fff), w, h);
        const float scale = std::min(1.0F, 380.0F / float(w));
        ImGui::Image(ImTextureRef(ImTextureID(tex)), ImVec2(float(w) * scale, float(h) * scale));
    }
    ImGui::End();
}

void App::frame(int width, int height)
{
    {
        std::vector<std::filesystem::path> q;
        {
            std::lock_guard<std::mutex> g(queued_lock_);
            q.swap(queued_);
        }
        if (!q.empty())
            open(std::move(q));
    }
    // what the library found, taken when it changes (a load may end before the first frame); busy read first, so
    // a finished load's last file is in the copy
    const bool busy = library_.busy();
    if (const unsigned changes = library_.changes(); changes != changes_seen_) {
        changes_seen_ = changes;
        sources_ = library_.sources();
    }
    if (!busy && !wanted_.empty()) {
        for (std::size_t i = 0; i < sources_.size(); ++i)
            if (lower(sources_[i].name).find(wanted_) != std::string::npos) {
                const Pick p{int(i), 0, wanted_block_, wanted_emitter_};
                select(p);
                break;
            }
        wanted_.clear();
    }

    view_x_ = int(panel_width);
    view_y_ = 0;
    view_w_ = std::max(1, width - int(panel_width));
    view_h_ = std::max(1, height - int(bar_height));

    advance(double(ImGui::GetIO().DeltaTime));
    if (camera_moved_ && !playing_)
        replay();
    camera_moved_ = false;

    tree();
    controls();
    pages();

    renderer_.begin(view_x_, height - view_y_ - view_h_, view_w_, view_h_, background_);
    if (grid_)
        renderer_.grid(orbit_.camera(float(view_w_) / float(view_h_)), 50.0F, 20);
    if (const auto block = picked_block())
        renderer_.draw(draws_, pictures_, block.get());
}

} // namespace pax
