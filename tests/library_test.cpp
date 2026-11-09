// The library's finding of effects: a bare DPX, a PAX, a BAR's entries, a DPX inside other bytes (only when
// looking inside), resident pictures and the game a path names. Made-up bytes only, nothing of the games.
#include "library.h"

#include <cstdio>
#include <cstring>
#include <map>
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

    expect(passed_over("D:/dt/kh2_first.hed_out/remastered", true) && !passed_over("D:/mods/remastered", true) &&
               !passed_over("D:/dt/kh2_first.hed_out/original", true),
           "an extraction's remastered folder passed over, no other");
    expect(passed_over("D:/a/b.dds", false) && passed_over("D:/a/b.SCD", false) && !passed_over("D:/a/b.pax", false) &&
               !passed_over("D:/a/b.anb", false),
           "pictures and sounds passed over");

    // the names the list shows
    auto n = name_of("kh2_fifth.hed_out/original/event/effect/tt_event_101.pax", Game::KH2);
    expect(n.file == "tt_event_101.pax" && n.place == "kh2_fifth.hed_out" && n.folder == "event/effect" &&
               n.group == "Event effects" && n.section == "Twilight Town" && n.copy.empty(),
           "a KH2 event effect: its extraction left out, listed under its world");
    n = name_of("msn/us/AL00_MS101.bar", Game::KH2);
    const auto fr = name_of("msn/fr/AL00_MS101.bar", Game::KH2);
    expect(n.group == "Missions" && n.section == "Agrabah" && n.copy == "us" && fr.copy == "fr" && n.key == fr.key,
           "a mission's language copies: one key, their languages");
    n = name_of("obj/B_AL020.a.us", Game::KH2);
    expect(n.group == "Characters and objects" && n.section == "Bosses (B_)" && n.world == "Agrabah" &&
               n.copy == "us" && n.key == name_of("obj/B_AL020.a.fr", Game::KH2).key &&
               n.key != name_of("obj/B_AL020.mdlx", Game::KH2).key,
           "a KH2 object by its letter, its world kept, its .a.<language> copies one key");
    expect(name_of("obj/G_EB110.a.us", Game::KH2).section == "Other (G_)", "an object of an unknown letter");
    expect(name_of("worldmap/eft/wp_pc.dpx", Game::KH1).section == "Hollow Bastion" &&
               name_of("eveffect/tw/tw_000.dpx", Game::KH1).group == "Event effects",
           "KH1's world codes are its own (pc: Hollow Bastion)");
    n = name_of("always.dpx", Game::KH1);
    expect(n.group == "Main folder" && n.section.empty(), "a file in the folder opened");
    n = name_of("mods/mine/fx.pax", Game::KH2);
    expect(n.group == "mods/mine folder" && n.section.empty(), "an unknown folder listed by its path");
    expect(name_of("limit/fm/donald_wi.bar", Game::KH2).section == "Timeless River" &&
               name_of("magic/BLIZZARD_1.mag", Game::KH2).section.empty() &&
               name_of("event/effect/mission_end.pax", Game::KH2).section.empty(),
           "a world only where two letters and digits (or nothing) name one");
    expect(effect_title("p_ex#0") == "p_ex (entry 0)" &&
               effect_title("WM_C#14 / wm_c#0") == "WM_C (entry 14) > wm_c (entry 0)" &&
               effect_title("at 0x40") == "at 0x40",
           "an effect's place in its file");

    // a language's copy with the same effects is one entry, the "us" copy kept; different effects stay apart
    {
        std::vector<Source> sources;
        std::map<std::string, std::vector<std::size_t>> by_key;
        const auto make = [&](const char *relative, std::uint64_t bytes) {
            Source s;
            s.name = relative;
            s.game = Game::KH2;
            s.naming = name_of(relative, Game::KH2);
            Effect e;
            e.bytes = bytes;
            s.effects.push_back(e);
            return s;
        };
        add_source(sources, by_key, make("msn/fr/X.bar", 1));
        add_source(sources, by_key, make("msn/gr/X.bar", 1));
        add_source(sources, by_key, make("msn/us/X.bar", 1));
        add_source(sources, by_key, make("msn/it/X.bar", 2));
        add_source(sources, by_key, make("kh2_first.hed_out/original/msn/it/X.bar", 2));
        expect(sources.size() == 2 && sources[0].name == "msn/us/X.bar" && sources[0].copies.size() == 3 &&
                   sources[0].copies[0] == "us" && sources[1].copies.size() == 1,
               "language copies merged, the us one shown; a different one and the same file twice");
    }

    if (failures == 0)
        std::printf("library: all checks passed\n");
    return failures == 0 ? 0 : 1;
}
