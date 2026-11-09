// The effects found in files and folders: KINGDOM HEARTS II's PAX (alone or inside its BAR containers: .mdlx,
// .mset, .bar, .a.*), KINGDOM HEARTS's and Re:Chain of Memories' DPX (alone, or inside other files), and the
// resident pictures some effects draw with (texcommon.dpd).
#pragma once

#include "names.h"

#include "khdays/assets/kh2_effect.h"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace pax {

namespace kh2 = khdays::assets::kh2;

// One effect file found: its blocks (each a set of emitters with their pictures, shapes and models), or why it
// could not be read.
struct Effect {
    std::string label; // where it is in its file ("", "a BAR entry", "at 0x1c40")
    std::vector<std::shared_ptr<const kh2::EffectBlock>> blocks;
    std::string error;
    std::uint64_t bytes = 0; // its bytes' FNV-1a hash: the same effect in another file
};

// A file on disk and the effects in it.
struct Source {
    std::filesystem::path path;
    std::string name; // relative to the folder opened
    Game game = Game::Unknown;
    std::vector<Effect> effects;
    Naming naming;
    std::vector<std::string> copies; // the languages' copies with these same effects, this one's first ("us" kept)
};

// `s` added to `sources`, or, when a file there is the same file in another language with the same effects, its
// language added to that one's copies (the "us" copy is the one kept). `by_key`: the sources of each naming key.
void add_source(std::vector<Source> &sources, std::map<std::string, std::vector<std::size_t>> &by_key, Source s);

// The effects in a file's bytes: a PAX, a DPX, a BAR (its entries, recursively) or, when `deep`, any DPX found
// inside other bytes (Re:Chain of Memories keeps its effects inside its own containers).
std::vector<Effect> effects_in(const std::uint8_t *data, std::size_t size, bool deep, const std::string &label = {});
// Resident pictures (a bare 0x96 block: texcommon.dpd), empty when the bytes are not one.
std::vector<kh2::EffectPicture> resident_pictures(const std::uint8_t *data, std::size_t size);
// Which game a file is from: its path's folders first (kh1_*, kh2_*, recom), then its kind (`kh2`: a PAX or a BAR,
// KINGDOM HEARTS II's).
Game game_of(const std::filesystem::path &path, const std::vector<Effect> &effects, bool kh2);

// What a folder's load passes over unless it looks inside every file: an extraction's "remastered" folder (the
// collection's HD pictures and sounds) and pictures and sounds (.dds, .png, .scd), which hold no effects.
bool passed_over(const std::filesystem::path &p, bool folder);

// Files and folders loaded in the background; what was found so far can be read at any time.
class Library {
public:
    ~Library();
    void open(std::vector<std::filesystem::path> paths, bool deep);
    bool busy() const { return busy_; }
    int seen() const { return seen_; } // files looked at so far
    double seconds() const { return seconds_; } // how long the last load took
    unsigned changes() const { return changes_; } // counts each change to what was found
    // A copy of what has been found (cheap: the blocks are shared).
    std::vector<Source> sources() const;
    std::vector<kh2::EffectPicture> resident(Game g) const;
    void clear();

private:
    void run(std::vector<std::filesystem::path> paths, bool deep);
    void add_file(const std::filesystem::path &file, const std::filesystem::path &root, bool deep,
                  std::uintmax_t size);

    mutable std::mutex lock_;
    std::vector<Source> sources_;
    std::map<std::string, std::vector<std::size_t>> by_key_;
    std::vector<kh2::EffectPicture> resident_[4];
    std::thread worker_;
    std::atomic<bool> busy_{false}, cancel_{false};
    std::atomic<int> seen_{0};
    std::atomic<double> seconds_{0};
    std::atomic<unsigned> changes_{0};
};

} // namespace pax
