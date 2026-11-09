#include "library.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iterator>
#include <system_error>

namespace pax {

namespace {

std::uint32_t u32(const std::uint8_t *p)
{
    std::uint32_t v;
    std::memcpy(&v, p, 4);
    return v;
}

// A DPX directory at `at`: 0x82, three zero words, its element count and elements (32 bytes each), then the
// list of its blocks' offsets, each leading to a 0x96 block.
bool dpx_at(const std::uint8_t *d, std::size_t size, std::size_t at)
{
    if (at + 16 > size || u32(d + at) != 0x82 || u32(d + at + 4) != 0 || u32(d + at + 8) != 0)
        return false;
    const std::uint32_t count = u32(d + at + 12);
    if (count == 0 || count > 0x10000)
        return false;
    const std::size_t list = at + 0x10 + std::size_t(count) * 32;
    if (list + 4 > size)
        return false;
    const std::uint32_t n = u32(d + list);
    if (n == 0 || n > 0x10000 || list + 4 + std::size_t(n) * 4 > size)
        return false;
    for (std::uint32_t i = 0; i < n; ++i) {
        const std::size_t o = at + u32(d + list + 4 + 4 * std::size_t(i));
        if (o + 4 > size || u32(d + o) != 0x96)
            return false;
    }
    return true;
}

std::uint64_t fnv1a(const std::uint8_t *data, std::size_t size)
{
    std::uint64_t h = 0xcbf29ce484222325ull;
    for (std::size_t i = 0; i < size; ++i)
        h = (h ^ data[i]) * 0x100000001b3ull;
    return h;
}

Effect read_one(const std::uint8_t *data, std::size_t size, const std::string &label)
{
    Effect e;
    e.label = label;
    e.bytes = fnv1a(data, size);
    try {
        for (auto &b : kh2::read_effect(data, size))
            e.blocks.push_back(std::make_shared<const kh2::EffectBlock>(std::move(b)));
    } catch (const std::exception &x) {
        e.error = x.what();
    }
    return e;
}

std::string lower(std::string s)
{
    for (auto &c : s)
        c = char(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string hex(std::size_t v)
{
    static const char digits[] = "0123456789abcdef";
    std::string s;
    do {
        s.insert(s.begin(), digits[v & 15]);
        v >>= 4;
    } while (v != 0);
    return "0x" + s;
}

} // namespace

void add_source(std::vector<Source> &sources, std::map<std::string, std::vector<std::size_t>> &by_key, Source s)
{
    auto &same_name = by_key[s.naming.key];
    for (const std::size_t i : same_name) {
        Source &kept = sources[i];
        if (kept.effects.size() != s.effects.size())
            continue;
        bool same = true;
        for (std::size_t e = 0; e < s.effects.size() && same; ++e)
            same = kept.effects[e].bytes == s.effects[e].bytes && kept.effects[e].label == s.effects[e].label;
        if (!same)
            continue;
        // the same copy again (another extraction holds it too): listed once
        if (s.naming.copy == kept.naming.copy ||
            std::find(kept.copies.begin(), kept.copies.end(), s.naming.copy) != kept.copies.end())
            return;
        if (s.naming.copy == "us") { // the copy shown: KINGDOM HEARTS II's own (English) one
            auto copies = std::move(kept.copies);
            copies.insert(copies.begin(), s.naming.copy);
            const auto effects = std::move(kept.effects); // the blocks already shared out stay the ones in use
            kept = std::move(s);
            kept.effects = effects;
            kept.copies = std::move(copies);
        } else {
            kept.copies.push_back(s.naming.copy);
        }
        return;
    }
    if (s.copies.empty() && !s.naming.copy.empty())
        s.copies.push_back(s.naming.copy);
    same_name.push_back(sources.size());
    sources.push_back(std::move(s));
}

std::vector<Effect> effects_in(const std::uint8_t *data, std::size_t size, bool deep, const std::string &label)
{
    std::vector<Effect> out;
    if (size < 16)
        return out;
    if (std::memcmp(data, "PAX_", 4) == 0 || dpx_at(data, size, 0)) {
        out.push_back(read_one(data, size, label));
        return out;
    }
    if (std::memcmp(data, "BAR\x01", 4) == 0) {
        const std::uint32_t n = u32(data + 4);
        for (std::uint32_t i = 0; i < n && 16 + 16 * std::size_t(i + 1) <= size; ++i) {
            const std::uint8_t *e = data + 16 + 16 * std::size_t(i);
            char name[5] = {};
            std::memcpy(name, e + 4, 4);
            const std::size_t at = u32(e + 8), length = u32(e + 12);
            if (at >= size || length > size - at)
                continue;
            std::string inner = label.empty() ? "" : label + " / ";
            inner += std::string(name) + "#" + std::to_string(i);
            for (auto &x : effects_in(data + at, length, deep, inner))
                out.push_back(std::move(x));
        }
        return out;
    }
    if (!deep)
        return out;
    // inside other bytes: each DPX directory found on a 16-byte boundary, and each PAX
    for (std::size_t at = 0; at + 16 <= size; at += 16) {
        if (std::memcmp(data + at, "PAX_", 4) == 0 || dpx_at(data, size, at)) {
            std::string where = (label.empty() ? "" : label + " / ") + "at " + hex(at);
            out.push_back(read_one(data + at, size - at, where));
        }
    }
    return out;
}

std::vector<kh2::EffectPicture> resident_pictures(const std::uint8_t *data, std::size_t size)
{
    if (size < 8 || u32(data) != 0x96)
        return {};
    try {
        return kh2::read_effect_block(data, size, 0).pictures;
    } catch (const std::exception &) {
        return {};
    }
}

Game game_of(const std::filesystem::path &path, const std::vector<Effect> &effects, bool kh2)
{
    for (const auto &part : path) {
        const std::string p = lower(part.string());
        if (p.rfind("kh1", 0) == 0)
            return Game::KH1;
        if (p.rfind("kh2", 0) == 0)
            return Game::KH2;
        if (p.rfind("recom", 0) == 0)
            return Game::ReCoM;
    }
    if (kh2)
        return Game::KH2;
    (void)effects;
    return Game::Unknown;
}

Library::~Library()
{
    cancel_ = true;
    if (worker_.joinable())
        worker_.join();
}

void Library::clear()
{
    cancel_ = true;
    if (worker_.joinable())
        worker_.join();
    cancel_ = false;
    std::lock_guard<std::mutex> g(lock_);
    sources_.clear();
    by_key_.clear();
    for (auto &r : resident_)
        r.clear();
    seen_ = 0;
    ++changes_;
}

void Library::open(std::vector<std::filesystem::path> paths, bool deep)
{
    if (worker_.joinable())
        worker_.join();
    busy_ = true;
    worker_ = std::thread([this, paths = std::move(paths), deep]() mutable { run(std::move(paths), deep); });
}

std::vector<Source> Library::sources() const
{
    std::lock_guard<std::mutex> g(lock_);
    return sources_;
}

std::vector<kh2::EffectPicture> Library::resident(Game g) const
{
    std::lock_guard<std::mutex> l(lock_);
    return resident_[int(g)];
}

bool passed_over(const std::filesystem::path &p, bool folder)
{
    if (folder) // the collection's HD pictures and sounds beside each extraction's original files
        return lower(p.filename().string()) == "remastered" &&
               lower(p.parent_path().filename().string()).ends_with(".hed_out");
    const std::string extension = lower(p.extension().string());
    return extension == ".dds" || extension == ".png" || extension == ".scd";
}

namespace {

// Whether a BAR's entries may hold an effect (an entry beginning as a PAX, a DPX or another BAR), from its
// directory and each entry's first 16 bytes: the whole file is read only then.
bool bar_may_hold_effects(std::ifstream &in, std::uintmax_t size)
{
    std::uint8_t head[16] = {};
    in.seekg(0);
    if (!in.read(reinterpret_cast<char *>(head), 16))
        return false;
    const std::uint32_t n = u32(head + 4);
    if (16 + 16 * std::uintmax_t(n) > size)
        return true; // a directory past the end: left to the reading to refuse
    std::vector<std::uint8_t> entries(16 * std::size_t(n));
    if (n != 0 && !in.read(reinterpret_cast<char *>(entries.data()), std::streamsize(entries.size())))
        return false;
    for (std::uint32_t i = 0; i < n; ++i) {
        const std::uint8_t *e = entries.data() + 16 * std::size_t(i);
        const std::uintmax_t at = u32(e + 8), length = u32(e + 12);
        if (at >= size || length > size - at || length < 16)
            continue;
        std::uint8_t start[16] = {};
        in.seekg(std::streamoff(at));
        if (!in.read(reinterpret_cast<char *>(start), 16))
            return true;
        if (std::memcmp(start, "PAX_", 4) == 0 || std::memcmp(start, "BAR\x01", 4) == 0 ||
            (u32(start) == 0x82 && u32(start + 4) == 0 && u32(start + 8) == 0))
            return true;
    }
    return false;
}

} // namespace

void Library::run(std::vector<std::filesystem::path> paths, bool deep)
{
    const auto started = std::chrono::steady_clock::now();
    for (const auto &p : paths) {
        std::error_code ec;
        if (std::filesystem::is_directory(p, ec)) {
            for (auto it = std::filesystem::recursive_directory_iterator(
                     p, std::filesystem::directory_options::skip_permission_denied, ec);
                 it != std::filesystem::recursive_directory_iterator() && !cancel_; it.increment(ec)) {
                if (ec)
                    break;
                if (it->is_directory(ec)) {
                    if (!deep && passed_over(it->path(), true))
                        it.disable_recursion_pending();
                    continue;
                }
                if (!it->is_regular_file(ec) || (!deep && passed_over(it->path(), false)))
                    continue;
                const auto size = it->file_size(ec); // from the folder's listing: no opening
                add_file(it->path(), p, deep, ec ? 0 : size);
            }
        } else {
            add_file(p, p.parent_path(), deep, std::filesystem::file_size(p, ec));
        }
        if (cancel_)
            break;
    }
    seconds_ = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    busy_ = false;
}

void Library::add_file(const std::filesystem::path &file, const std::filesystem::path &root, bool deep,
                       std::uintmax_t size)
{
    ++seen_;
    std::error_code ec;
    if (size < 16 || size > (std::uintmax_t(512) << 20))
        return;
    std::ifstream in(file, std::ios::binary);
    std::uint8_t head[4] = {};
    in.read(reinterpret_cast<char *>(head), 4);
    const bool bar = std::memcmp(head, "BAR\x01", 4) == 0;
    const bool known = std::memcmp(head, "PAX_", 4) == 0 || bar || u32(head) == 0x82 || u32(head) == 0x96;
    if (!known && !deep)
        return;
    if (bar && !deep && !bar_may_hold_effects(in, size))
        return;
    in.clear();
    in.seekg(0);
    std::vector<std::uint8_t> data(static_cast<std::size_t>(size));
    in.read(reinterpret_cast<char *>(data.data()), std::streamsize(data.size()));
    if (!in)
        return;
    Source s;
    s.path = file;
    s.name = std::filesystem::relative(file, root, ec).generic_string();
    if (ec || s.name.empty())
        s.name = file.filename().string();
    if (u32(head) == 0x96) { // resident pictures
        auto pics = resident_pictures(data.data(), data.size());
        if (!pics.empty()) {
            const Game g = game_of(file, {}, false);
            std::lock_guard<std::mutex> l(lock_);
            auto &r = resident_[int(g)];
            r.insert(r.end(), pics.begin(), pics.end());
        }
        return;
    }
    s.effects = effects_in(data.data(), data.size(), deep);
    if (s.effects.empty())
        return;
    s.game = game_of(file, s.effects, std::memcmp(head, "PAX_", 4) == 0 || std::memcmp(head, "BAR\x01", 4) == 0);
    s.naming = name_of(s.name, s.game);
    std::lock_guard<std::mutex> l(lock_);
    add_source(sources_, by_key_, std::move(s));
    ++changes_;
}

} // namespace pax
