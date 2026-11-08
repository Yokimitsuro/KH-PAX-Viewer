// The effects found in files and folders: KINGDOM HEARTS II's PAX (alone or inside its BAR containers: .mdlx,
// .mset, .bar, .a.*), KINGDOM HEARTS's and Re:Chain of Memories' DPX (alone, or inside other files), and the
// resident pictures some effects draw with (texcommon.dpd).
#pragma once

#include "khdays/assets/kh2_effect.h"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace pax {

namespace kh2 = khdays::assets::kh2;

enum class Game { Unknown, KH1, KH2, ReCoM };
const char *game_name(Game g);

// One effect file found: its blocks (each a set of emitters with their pictures, shapes and models), or why it
// could not be read.
struct Effect {
    std::string label; // where it is in its file ("", "a BAR entry", "at 0x1c40")
    std::vector<std::shared_ptr<const kh2::EffectBlock>> blocks;
    std::string error;
};

// A file on disk and the effects in it.
struct Source {
    std::filesystem::path path;
    std::string name; // shown: relative to the folder opened
    Game game = Game::Unknown;
    std::vector<Effect> effects;
};

// The effects in a file's bytes: a PAX, a DPX, a BAR (its entries, recursively) or, when `deep`, any DPX found
// inside other bytes (Re:Chain of Memories keeps its effects inside its own containers).
std::vector<Effect> effects_in(const std::uint8_t *data, std::size_t size, bool deep, const std::string &label = {});
// Resident pictures (a bare 0x96 block: texcommon.dpd), empty when the bytes are not one.
std::vector<kh2::EffectPicture> resident_pictures(const std::uint8_t *data, std::size_t size);
// Which game a file is from: its path's folders first (kh1_*, kh2_*, recom), then its kind.
Game game_of(const std::filesystem::path &path, const std::vector<Effect> &effects, bool pax);

// Files and folders loaded in the background; what was found so far can be read at any time.
class Library {
public:
    ~Library();
    void open(std::vector<std::filesystem::path> paths, bool deep);
    bool busy() const { return busy_; }
    int seen() const { return seen_; } // files looked at so far
    // A copy of what has been found (cheap: the blocks are shared).
    std::vector<Source> sources() const;
    std::vector<kh2::EffectPicture> resident(Game g) const;
    void clear();

private:
    void run(std::vector<std::filesystem::path> paths, bool deep);
    void add_file(const std::filesystem::path &file, const std::filesystem::path &root, bool deep);

    mutable std::mutex lock_;
    std::vector<Source> sources_;
    std::vector<kh2::EffectPicture> resident_[4];
    std::thread worker_;
    std::atomic<bool> busy_{false}, cancel_{false};
    std::atomic<int> seen_{0};
};

} // namespace pax
