// The library's finding of effects: a bare DPX, a PAX, a BAR's entries, a DPX inside other bytes (only when
// looking inside), resident pictures and the game a path names. Made-up bytes only, nothing of the games.
#include "library.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

int failures = 0;

void expect(bool ok, const char *what)
{
    if (!ok) {
        std::fprintf(stderr, "library: %s\n", what);
        ++failures;
    }
}

void put32(std::vector<std::uint8_t> &d, std::size_t at, std::uint32_t v)
{
    if (d.size() < at + 4)
        d.resize(at + 4);
    std::memcpy(d.data() + at, &v, 4);
}

// a block with nothing in it: 0x96 then five empty lists
std::vector<std::uint8_t> empty_block()
{
    std::vector<std::uint8_t> b;
    put32(b, 0, 0x96);
    for (int i = 0; i < 5; ++i)
        put32(b, 4 + 4 * std::size_t(i), 0);
    return b;
}

// a DPX: 0x82, three zero words, one element (32 bytes), the list of one block, the block
std::vector<std::uint8_t> dpx()
{
    std::vector<std::uint8_t> d(0x10 + 32, 0);
    put32(d, 0, 0x82);
    put32(d, 12, 1);
    const std::size_t list = d.size();
    put32(d, list, 1);
    const std::size_t block = list + 8;
    put32(d, list + 4, std::uint32_t(block));
    const auto b = empty_block();
    d.insert(d.end(), b.begin(), b.end());
    return d;
}

std::vector<std::uint8_t> made_pax()
{
    std::vector<std::uint8_t> p(0x20, 0);
    std::memcpy(p.data(), "PAX_", 4);
    put32(p, 12, 0x20);
    const auto d = dpx();
    p.insert(p.end(), d.begin(), d.end());
    return p;
}

std::vector<std::uint8_t> bar(const std::vector<std::uint8_t> &inner)
{
    std::vector<std::uint8_t> b(32, 0);
    std::memcpy(b.data(), "BAR\x01", 4);
    put32(b, 4, 1);
    put32(b, 16, 18);
    std::memcpy(b.data() + 20, "efct", 4);
    put32(b, 24, 32);
    put32(b, 28, std::uint32_t(inner.size()));
    b.insert(b.end(), inner.begin(), inner.end());
    return b;
}

} // namespace

int main()
{
    using namespace pax;
    const auto d = dpx();
    auto found = effects_in(d.data(), d.size(), false);
    expect(found.size() == 1 && found[0].error.empty() && found[0].blocks.size() == 1, "a bare DPX read");

    const auto p = made_pax();
    found = effects_in(p.data(), p.size(), false);
    expect(found.size() == 1 && found[0].error.empty() && found[0].blocks.size() == 1, "a PAX read");

    const auto b = bar(p);
    found = effects_in(b.data(), b.size(), false);
    expect(found.size() == 1 && found[0].label == "efct#0", "a BAR's PAX entry found and named");

    std::vector<std::uint8_t> other(0x40, 0xab);
    other.insert(other.end(), d.begin(), d.end());
    expect(effects_in(other.data(), other.size(), false).empty(), "nothing found inside other bytes unless asked");
    found = effects_in(other.data(), other.size(), true);
    expect(found.size() == 1 && found[0].label == "at 0x40" && found[0].error.empty(), "a DPX found inside");

    const auto e = empty_block();
    expect(resident_pictures(e.data(), e.size()).empty(), "a block without pictures gives none");
    expect(resident_pictures(d.data(), d.size()).empty(), "a DPX is not resident pictures");

    expect(game_of("C:/x/kh1_first.hed_out/original/always.dpx", {}, false) == Game::KH1, "KH1 by its folder");
    expect(game_of("D:/kh2_fifth/obj/PRIZE.mdlx", {}, false) == Game::KH2, "KH2 by its folder");
    expect(game_of("D:/Recom/abc.dat", {}, false) == Game::ReCoM, "Re:CoM by its folder");
    expect(game_of("D:/somewhere/x.pax", {}, true) == Game::KH2, "a PAX is KH2's");

    if (failures == 0)
        std::printf("library: all checks passed\n");
    return failures == 0 ? 0 : 1;
}
