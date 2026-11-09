#include "app.h"

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <tuple>

namespace pax {

namespace {

constexpr double tick_seconds = 1.0 / 60.0; // KINGDOM HEARTS II's effects tick 60 times a second
constexpr float least_text = 0.7F, most_text = 2.5F;

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

// widths of the next item, to put it beside the last when it fits
float button_width(const char *label)
{
    return ImGui::CalcTextSize(label, nullptr, true).x + 2 * ImGui::GetStyle().FramePadding.x;
}

float check_width(const char *label)
{
    return ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x + ImGui::CalcTextSize(label, nullptr, true).x;
}

float labelled_width(float field, const char *label)
{
    return field + ImGui::GetStyle().ItemInnerSpacing.x + ImGui::CalcTextSize(label, nullptr, true).x;
}

// on the same line as the last item when `width` fits there, else on the next
void beside(float width)
{
    const float edge = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
    if (ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x + width <= edge)
        ImGui::SameLine();
}

// the next tree node opened (`open`) or closed (`close`) this frame; neither: as the user left it
void open_next(bool open, bool close)
{
    if (close)
        ImGui::SetNextItemOpen(false, ImGuiCond_Always);
    else if (open)
        ImGui::SetNextItemOpen(true, ImGuiCond_Always);
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
        } else if (key == "panel") {
            panel_width_ = std::max(200.0F, std::strtof(value.c_str(), nullptr));
        } else if (key == "text") {
            text_size(std::strtof(value.c_str(), nullptr));
        } else if (key == "pick") { // key, effect, block, emitter, tab between
            std::istringstream parts(value);
            std::string effect, block, emitter;
            std::getline(parts, restore_key_, '\t');
            std::getline(parts, effect, '\t');
            std::getline(parts, block, '\t');
            std::getline(parts, emitter, '\t');
            restore_effect_ = std::atoi(effect.c_str());
            restore_block_ = std::atoi(block.c_str());
            restore_emitter_ = std::atoi(emitter.c_str());
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
    out << "panel=" << panel_width_ << "\n";
    out << "text=" << text_size_ << "\n";
    if (const Source *s = picked_source(); s != nullptr && pick_.emitter >= 0)
        out << "pick=" << s->naming.key << "\t" << pick_.effect << "\t" << pick_.block << "\t" << pick_.emitter << "\n";
    else if (!restore_key_.empty()) // not loaded yet: kept for the next run
        out << "pick=" << restore_key_ << "\t" << restore_effect_ << "\t" << restore_block_ << "\t" << restore_emitter_
            << "\n";
}

void App::play_when_loaded(std::string name, int block, int emitter)
{
    wanted_ = lower(std::move(name));
    wanted_block_ = block;
    wanted_emitter_ = emitter;
    restore_key_.clear();
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
    restore_key_.clear();
    library_.clear();
    sources_.clear();
    tree_stale_ = true;
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

std::string App::picked_title() const
{
    const Source *s = picked_source();
    if (s == nullptr || pick_.emitter < 0 || pick_.effect < 0 || std::size_t(pick_.effect) >= s->effects.size())
        return {};
    const std::string between = "  >  ";
    std::string t = game_name(s->game) + between + s->naming.group;
    if (!s->naming.section.empty())
        t += between + s->naming.section;
    t += between + s->naming.file;
    const auto &e = s->effects[std::size_t(pick_.effect)];
    if (s->effects.size() > 1)
        t += between + (e.label.empty() ? "Effect " + std::to_string(pick_.effect) : effect_title(e.label));
    if (e.blocks.size() > 1)
        t += between + "Block " + std::to_string(pick_.block);
    return t + between + "Emitter " + std::to_string(pick_.emitter);
}

App::Pick App::first_pick(int source, int effect, int block, int emitter) const
{
    if (source < 0 || std::size_t(source) >= sources_.size())
        return {};
    const Source &s = sources_[std::size_t(source)];
    // the one asked for when it is there, else the file's first emitter
    if (effect >= 0 && std::size_t(effect) < s.effects.size()) {
        const auto &e = s.effects[std::size_t(effect)];
        if (block >= 0 && std::size_t(block) < e.blocks.size() && emitter >= 0 &&
            std::size_t(emitter) < e.blocks[std::size_t(block)]->emitters.size())
            return {source, effect, block, emitter};
    }
    for (std::size_t ei = 0; ei < s.effects.size(); ++ei)
        for (std::size_t bi = 0; bi < s.effects[ei].blocks.size(); ++bi)
            if (!s.effects[ei].blocks[bi]->emitters.empty())
                return {source, int(ei), int(bi), 0};
    return {};
}

void App::select(const Pick &p)
{
    pick_ = p;
    restore_key_.clear();
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
    save_settings(); // the emitter chosen again at the next start
}

void App::step(int by, bool whole_file)
{
    if (order_.empty())
        return;
    const int n = int(order_.size());
    int at = -1;
    for (int i = 0; i < n && at < 0; ++i)
        if (order_[std::size_t(i)] == pick_)
            at = i;
    int to = 0;
    if (at < 0) {
        to = by > 0 ? 0 : n - 1;
    } else if (!whole_file) {
        to = std::clamp(at + by, 0, n - 1);
    } else {
        const int file = order_[std::size_t(at)].source;
        to = at;
        if (by > 0) {
            while (to < n && order_[std::size_t(to)].source == file)
                ++to;
            if (to >= n)
                return;
        } else { // the first emitter of the file before
            while (to >= 0 && order_[std::size_t(to)].source == file)
                --to;
            if (to < 0)
                return;
            const int before = order_[std::size_t(to)].source;
            while (to > 0 && order_[std::size_t(to - 1)].source == before)
                --to;
        }
    }
    if (order_[std::size_t(to)] == pick_)
        return;
    select(order_[std::size_t(to)]);
    reveal_ = true;
}

void App::text_size(float to)
{
    text_size_ = std::clamp(std::round(to * 10.0F) / 10.0F, least_text, most_text);
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
    const bool ctrl = (SDL_GetModState() & SDL_KMOD_CTRL) != 0;
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
        if (ctrl) {
            text_size(text_size_ + (e.wheel.y > 0 ? 0.1F : e.wheel.y < 0 ? -0.1F : 0.0F));
        } else if (!io.WantCaptureMouse) {
            orbit_.zoom(e.wheel.y);
            camera_moved_ = true;
        }
        break;
    case SDL_EVENT_KEY_DOWN:
        if (io.WantCaptureKeyboard)
            break;
        if (ctrl && (e.key.key == SDLK_EQUALS || e.key.key == SDLK_PLUS || e.key.key == SDLK_KP_PLUS))
            text_size(text_size_ + 0.1F);
        else if (ctrl && (e.key.key == SDLK_MINUS || e.key.key == SDLK_KP_MINUS))
            text_size(text_size_ - 0.1F);
        else if (ctrl && (e.key.key == SDLK_0 || e.key.key == SDLK_KP_0))
            text_size(1.0F);
        else if (e.key.key == SDLK_SPACE)
            playing_ = !playing_;
        else if (e.key.key == SDLK_PERIOD && fx_ && !playing_)
            tick(true);
        else if (e.key.key == SDLK_R)
            restart();
        else if (e.key.key == SDLK_DOWN)
            step(1, false);
        else if (e.key.key == SDLK_UP)
            step(-1, false);
        else if (e.key.key == SDLK_PAGEDOWN)
            step(1, true);
        else if (e.key.key == SDLK_PAGEUP)
            step(-1, true);
        break;
    default: break;
    }
}

void App::build_tree()
{
    tree_.clear();
    order_.clear();
    where_.assign(sources_.size(), Where{});
    tree_filter_ = filter_;
    tree_stale_ = false;
    const std::string filter = lower(filter_);
    std::map<std::string, int> named; // files of the same name, told apart by their copy or extraction
    for (const auto &s : sources_)
        ++named[s.naming.key];
    struct Row {
        int source;
        int group_rank, section_rank;
        std::string group, section, file;
    };
    for (const Game g : {Game::KH2, Game::KH1, Game::ReCoM, Game::Unknown}) {
        std::vector<Row> rows;
        for (std::size_t i = 0; i < sources_.size(); ++i) {
            const Source &s = sources_[i];
            if (s.game != g)
                continue;
            const Naming &n = s.naming;
            if (!filter.empty()) {
                std::string words = s.name + "|" + n.group + "|" + n.section + "|" + n.world + "|" + game_name(g);
                for (const auto &c : s.copies)
                    words += "|" + c;
                if (lower(words).find(filter) == std::string::npos)
                    continue;
            }
            rows.push_back({int(i), n.group_rank, n.section_rank, lower(n.group), lower(n.section), lower(n.file)});
        }
        if (rows.empty())
            continue;
        std::sort(rows.begin(), rows.end(), [](const Row &a, const Row &b) {
            return std::tie(a.group_rank, a.group, a.section_rank, a.section, a.file, a.source) <
                   std::tie(b.group_rank, b.group, b.section_rank, b.section, b.file, b.source);
        });
        TreeGame game;
        game.game = g;
        for (const Row &r : rows) {
            const Source &s = sources_[std::size_t(r.source)];
            const Naming &n = s.naming;
            if (game.groups.empty() || game.groups.back().label != n.group)
                game.groups.push_back({n.group, {}, 0});
            TreeGroup &group = game.groups.back();
            if (group.sections.empty() || group.sections.back().label != n.section)
                group.sections.push_back({n.section, {}});
            TreeFile f{r.source, n.file, {}};
            if (named[n.key] > 1)
                f.label += " (" + (!n.copy.empty() ? n.copy : !n.place.empty() ? n.place : s.name) + ")";
            if (!n.world.empty() && n.world != n.section)
                f.note = n.world;
            group.sections.back().files.push_back(std::move(f));
            ++group.files;
            ++game.files;
            where_[std::size_t(r.source)] = {int(tree_.size()), int(game.groups.size()) - 1,
                                             int(group.sections.size()) - 1};
        }
        tree_.push_back(std::move(game));
    }
    for (const auto &game : tree_)
        for (const auto &group : game.groups)
            for (const auto &section : group.sections)
                for (const auto &f : section.files) {
                    const Source &s = sources_[std::size_t(f.source)];
                    for (std::size_t ei = 0; ei < s.effects.size(); ++ei)
                        for (std::size_t bi = 0; bi < s.effects[ei].blocks.size(); ++bi)
                            for (std::size_t m = 0; m < s.effects[ei].blocks[bi]->emitters.size(); ++m)
                                order_.push_back({f.source, int(ei), int(bi), int(m)});
                }
}

void App::file_tooltip(const Source &s) const
{
    ImGui::BeginTooltip();
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 40.0F);
    ImGui::TextUnformatted(s.name.c_str());
    std::string where = std::string(game_name(s.game)) + "  >  " + s.naming.group;
    if (!s.naming.section.empty())
        where += "  >  " + s.naming.section;
    if (!s.naming.world.empty() && s.naming.world != s.naming.section)
        where += "  (" + s.naming.world + ")";
    ImGui::TextDisabled("%s", where.c_str());
    if (s.copies.size() > 1) {
        std::string copies;
        for (const auto &c : s.copies)
            copies += (copies.empty() ? "" : ", ") + c;
        ImGui::TextDisabled("The same effects in its copies: %s", copies.c_str());
    }
    std::size_t blocks = 0, emitters = 0;
    for (const auto &e : s.effects) {
        blocks += e.blocks.size();
        for (const auto &b : e.blocks)
            emitters += b->emitters.size();
    }
    if (s.effects.size() == 1 && !s.effects[0].label.empty())
        ImGui::TextDisabled("Its effect: %s", effect_title(s.effects[0].label).c_str());
    ImGui::TextDisabled("%zu effect%s, %zu block%s, %zu emitter%s", s.effects.size(), s.effects.size() == 1 ? "" : "s",
                        blocks, blocks == 1 ? "" : "s", emitters, emitters == 1 ? "" : "s");
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
}

void App::draw_file(const TreeFile &f)
{
    const Source &s = sources_[std::size_t(f.source)];
    const bool chosen = pick_.source == f.source;
    ImGui::PushID(f.source);
    open_next(reveal_ && chosen, collapse_);
    const bool open = ImGui::TreeNodeEx("file", chosen ? ImGuiTreeNodeFlags_Selected : 0, "%s", f.label.c_str());
    const bool opened = ImGui::IsItemToggledOpen() && open;
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        file_tooltip(s);
    if (!f.note.empty()) {
        ImGui::SameLine();
        ImGui::TextDisabled("%s", f.note.c_str());
    }
    if (opened && !chosen) // a file opened plays its first emitter
        if (const Pick p = first_pick(f.source); p.emitter >= 0)
            select(p);
    if (open) {
        for (std::size_t ei = 0; ei < s.effects.size(); ++ei) {
            const Effect &e = s.effects[ei];
            ImGui::PushID(int(ei));
            const bool one = s.effects.size() == 1;
            bool open_effect = one;
            if (!one) {
                open_next(reveal_ && chosen && pick_.effect == int(ei), collapse_);
                const std::string title = e.label.empty() ? "Effect " + std::to_string(ei) : effect_title(e.label);
                open_effect = ImGui::TreeNode("effect", "%s", title.c_str());
            }
            if (open_effect) {
                if (!e.error.empty())
                    ImGui::TextColored(ImVec4(1, 0.5F, 0.4F, 1), "Not read: %s", e.error.c_str());
                for (std::size_t bi = 0; bi < e.blocks.size(); ++bi) {
                    const auto &b = *e.blocks[bi];
                    ImGui::PushID(int(bi));
                    bool open_block = e.blocks.size() == 1;
                    if (!open_block) {
                        open_next(reveal_ && chosen && pick_.effect == int(ei) && pick_.block == int(bi), collapse_);
                        open_block = ImGui::TreeNode("block", "Block %zu   %zu emitter%s", bi, b.emitters.size(),
                                                     b.emitters.size() == 1 ? "" : "s");
                    }
                    if (open_block) {
                        for (std::size_t m = 0; m < b.emitters.size(); ++m) {
                            const auto &em = b.emitters[m];
                            const Pick p{f.source, int(ei), int(bi), int(m)};
                            const int frames = em.length / 4096;
                            char label[96];
                            std::snprintf(label, sizeof label, "Emitter %zu   %d frames, %.1f s%s", m, frames,
                                          double(frames) * tick_seconds, em.repeat ? ", repeats" : "");
                            if (ImGui::Selectable(label, pick_ == p))
                                select(p);
                            if (reveal_ && pick_ == p)
                                ImGui::SetScrollHereY(0.5F);
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

void App::tree()
{
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const float least = 14.0F * ImGui::GetFontSize();
    const float most = std::max(least, display.x - 20.0F * ImGui::GetFontSize());
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(panel_width_, display.y), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(least, display.y), ImVec2(most, display.y));
    ImGui::Begin("Files", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);
    panel_width_ = ImGui::GetWindowWidth(); // its right edge dragged
    if (ImGui::Button("Open files..."))
        SDL_ShowOpenFileDialog(on_files, this, window_, nullptr, 0, nullptr, true);
    beside(button_width("Open folder..."));
    if (ImGui::Button("Open folder..."))
        SDL_ShowOpenFolderDialog(on_files, this, window_, last_folder_.empty() ? nullptr : to_utf8(last_folder_).c_str(),
                                 false);
    beside(check_width("Look inside"));
    if (ImGui::Checkbox("Look inside", &deep_))
        save_settings();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Look for effects inside any file, not only PAX, DPX and BAR files\n"
                          "(Re:Chain of Memories keeps its effects inside its own files). Slower.");
    std::size_t listed = 0;
    for (const auto &g : tree_)
        listed += g.files;
    if (library_.busy())
        ImGui::TextWrapped("Loading... %d files looked at, %zu with effects", library_.seen(), sources_.size());
    else if (filter_[0] != '\0')
        ImGui::TextWrapped("%zu of %zu files with effects", listed, sources_.size());
    else
        ImGui::TextWrapped("%zu file%s with effects (%d looked at in %.0f s)", sources_.size(),
                           sources_.size() == 1 ? "" : "s", library_.seen(), library_.seconds());
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##filter", "Filter (name, world, kind)", filter_, sizeof filter_);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        ImGui::SetTooltip("Lists the files whose name, folder, world or kind holds it:\n"
                          "tt_event, Agrabah, Bosses, Rooms, Missions, B_EX100...");
    if (ImGui::Button("Close all"))
        collapse_ = true;
    beside(button_width("Show the one playing"));
    ImGui::BeginDisabled(pick_.emitter < 0);
    if (ImGui::Button("Show the one playing"))
        reveal_ = true;
    ImGui::EndDisabled();
    ImGui::Separator();

    ImGui::BeginChild("tree", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);
    const Where at = pick_.source >= 0 && std::size_t(pick_.source) < where_.size() ? where_[std::size_t(pick_.source)]
                                                                                       : Where{};
    for (std::size_t gi = 0; gi < tree_.size(); ++gi) {
        const TreeGame &game = tree_[gi];
        open_next(reveal_ && at.game == int(gi), false);
        if (!ImGui::TreeNodeEx(game_name(game.game), ImGuiTreeNodeFlags_DefaultOpen, "%s (%zu)", game_name(game.game),
                               game.files))
            continue;
        for (std::size_t ri = 0; ri < game.groups.size(); ++ri) {
            const TreeGroup &group = game.groups[ri];
            const bool here = at.game == int(gi) && at.group == int(ri);
            open_next((reveal_ && here) || expand_, collapse_);
            if (!ImGui::TreeNodeEx(group.label.c_str(), 0, "%s (%zu)", group.label.c_str(), group.files))
                continue;
            for (std::size_t ci = 0; ci < group.sections.size(); ++ci) {
                const TreeSection &section = group.sections[ci];
                if (section.label.empty()) {
                    for (const auto &f : section.files)
                        draw_file(f);
                    continue;
                }
                open_next((reveal_ && here && at.section == int(ci)) || expand_, collapse_);
                if (ImGui::TreeNodeEx(section.label.c_str(), 0, "%s (%zu)", section.label.c_str(),
                                      section.files.size())) {
                    for (const auto &f : section.files)
                        draw_file(f);
                    ImGui::TreePop();
                }
            }
            ImGui::TreePop();
        }
        ImGui::TreePop();
    }
    if (tree_.empty() && !library_.busy())
        ImGui::TextDisabled(sources_.empty() ? "Open a folder of extracted game files."
                                             : "Nothing matches the filter.");
    ImGui::EndChild();
    ImGui::End();
    reveal_ = expand_ = collapse_ = false;
}

void App::controls()
{
    const ImVec2 size = ImGui::GetIO().DisplaySize;
    const ImGuiStyle &style = ImGui::GetStyle();
    ImGui::SetNextWindowPos(ImVec2(panel_width_, size.y - bar_height_));
    ImGui::SetNextWindowSize(ImVec2(size.x - panel_width_, bar_height_));
    ImGui::Begin("Controls", nullptr,
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar);
    if (const std::string title = picked_title(); !title.empty())
        ImGui::TextWrapped("%s", title.c_str());
    else
        ImGui::TextDisabled("Choose an emitter on the left (open a folder of extracted game files first).");
    if (!refused_.empty())
        ImGui::TextColored(ImVec4(1, 0.5F, 0.4F, 1), "Cannot play it: %s", refused_.c_str());
    ImGui::BeginDisabled(order_.empty());
    if (ImGui::Button("< Previous"))
        step(-1, false);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("The emitter before in the list (Up arrow; Page Up: the file before).");
    beside(button_width("Next >"));
    if (ImGui::Button("Next >"))
        step(1, false);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("The emitter after in the list (Down arrow; Page Down: the file after).");
    ImGui::EndDisabled();
    beside(button_width(playing_ ? "Pause" : "Play"));
    if (ImGui::Button(playing_ ? "Pause" : "Play"))
        playing_ = !playing_;
    beside(button_width("Step"));
    ImGui::BeginDisabled(playing_ || !fx_);
    if (ImGui::Button("Step"))
        tick(true);
    ImGui::EndDisabled();
    beside(button_width("Restart"));
    if (ImGui::Button("Restart"))
        restart();
    beside(button_width("Stop repeating"));
    ImGui::BeginDisabled(!fx_);
    if (ImGui::Button("Stop repeating") && fx_)
        fx_->stop();
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("A repeating effect closes as the game closes it, then ends.");
    beside(check_width("Loop"));
    ImGui::Checkbox("Loop", &loop_);
    const float slider = 9.0F * ImGui::GetFontSize();
    beside(labelled_width(slider, "Speed"));
    ImGui::SetNextItemWidth(slider);
    ImGui::SliderFloat("Speed", &speed_, 0.1F, 4.0F, "%.2fx");
    beside(button_width("Reset camera"));
    if (ImGui::Button("Reset camera")) {
        orbit_.reset();
        camera_moved_ = true;
    }
    if (ImGui::Checkbox("Grid", &grid_))
        save_settings();
    beside(check_width("Smooth pictures"));
    if (ImGui::Checkbox("Smooth pictures", &renderer_.smooth)) {
        renderer_.forget();
        save_settings();
    }
    beside(check_width("Swing points"));
    ImGui::Checkbox("Swing points", &swing_);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Give the effect two points swung round, as a weapon's base and tip\n"
                          "(the effects that draw a swing's trail follow them).");
    beside(check_width("Screen copies"));
    ImGui::Checkbox("Screen copies", &renderer_.show_screen);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Show the draws that copy the screen behind (heat haze, distortions) as plain shapes.");
    beside(check_width("Pages"));
    ImGui::Checkbox("Pages", &pages_window_);
    beside(labelled_width(ImGui::GetFrameHeight(), "Background"));
    if (ImGui::ColorEdit3("Background", background_, ImGuiColorEditFlags_NoInputs))
        save_settings();
    const float small = 7.0F * ImGui::GetFontSize();
    beside(labelled_width(small, "Text size"));
    ImGui::SetNextItemWidth(small);
    float size_now = text_size_;
    if (ImGui::SliderFloat("Text size", &size_now, least_text, most_text, "%.1fx"))
        text_size(size_now);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Also Ctrl + mouse wheel, Ctrl + plus / minus, Ctrl + 0.");
    if (fx_)
        ImGui::Text("frame %d   objects %zu   draws %zu   triangles %zu%s", frame_, fx_->objects(), draws_.size(),
                    renderer_.triangles, fx_->done() ? "   (ended)" : "");
    else
        ImGui::TextDisabled("frame -");
    // as tall as what it holds, from the next frame
    bar_height_ = std::max(2.0F * ImGui::GetFrameHeightWithSpacing(),
                           ImGui::GetCursorPosY() - style.ItemSpacing.y + style.WindowPadding.y);
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
    ImGui::GetStyle().FontScaleMain = text_size_; // the text size chosen, from the next frame
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
        tree_stale_ = true;
    }
    if (tree_filter_ != filter_) {
        tree_stale_ = true;
        expand_ = filter_[0] != '\0'; // a new filter opens what it lists
    }
    if (tree_stale_)
        build_tree();
    if (!busy && !wanted_.empty()) {
        for (std::size_t i = 0; i < sources_.size(); ++i)
            if (lower(sources_[i].name).find(wanted_) != std::string::npos) {
                const Pick p{int(i), 0, wanted_block_, wanted_emitter_};
                select(p);
                reveal_ = true;
                break;
            }
        wanted_.clear();
    }
    if (!restore_key_.empty()) { // the emitter chosen last time, once its file is loaded
        for (std::size_t i = 0; i < sources_.size(); ++i)
            if (sources_[i].naming.key == restore_key_) {
                if (const Pick p = first_pick(int(i), restore_effect_, restore_block_, restore_emitter_);
                    p.emitter >= 0) {
                    select(p);
                    reveal_ = true;
                }
                restore_key_.clear();
                break;
            }
        if (!busy)
            restore_key_.clear();
    }

    view_x_ = int(panel_width_);
    view_y_ = 0;
    view_w_ = std::max(1, width - int(panel_width_));
    view_h_ = std::max(1, height - int(bar_height_));

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
