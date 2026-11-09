#include "names.h"

#include <cctype>
#include <cstring>
#include <vector>

namespace pax {

namespace {

struct Code {
    const char *code, *name;
};

// OpenKh's world lists (openkh.dev/kh2/worlds.html, openkh.dev/kh1/worlds.html), the names as the games show them
const Code kh2_worlds[] = {
    {"zz", "World ZZ"},          {"es", "End of Sea"},       {"tt", "Twilight Town"},       {"di", "Destiny Islands"},
    {"hb", "Hollow Bastion"},    {"bb", "Beast's Castle"},   {"he", "Olympus Coliseum"},    {"al", "Agrabah"},
    {"mu", "The Land of Dragons"}, {"po", "100 Acre Wood"},  {"lk", "Pride Lands"},         {"lm", "Atlantica"},
    {"dc", "Disney Castle"},     {"wi", "Timeless River"},   {"nm", "Halloween Town"},      {"wm", "World Map"},
    {"ca", "Port Royal"},        {"tr", "Space Paranoids"},  {"eh", "The World That Never Was"},
};
const Code kh1_worlds[] = {
    {"al", "Agrabah"},           {"aw", "Wonderland"},       {"dc", "Disney Castle"},       {"dh", "Dive to the Heart"},
    {"di", "Destiny Islands"},   {"ew", "End of the World"}, {"he", "Olympus Coliseum"},    {"lm", "Atlantica"},
    {"nm", "Halloween Town"},    {"pc", "Hollow Bastion"},   {"pi", "Monstro"},             {"po", "100 Acre Wood"},
    {"pp", "Neverland"},         {"tw", "Traverse Town"},    {"tz", "Deep Jungle"},         {"zz", "Miscellaneous"},
};

// the folders of each game's kinds of file, in the order listed
struct Kind {
    Game game;
    const char *folder, *name;
};
const Kind kinds[] = {
    {Game::KH2, "event/effect", "Event effects"},
    {Game::KH2, "map", "Rooms"},
    {Game::KH2, "msn", "Missions"},
    {Game::KH2, "obj", "Characters and objects"},
    {Game::KH2, "magic", "Magic"},
    {Game::KH2, "limit", "Limits"},
    {Game::KH1, "eveffect", "Event effects"},
    {Game::KH1, "worldmap", "World map"},
};

// KH2's objects by the letter their names begin with (OpenKh's object list, openkh.dev/kh2/dictionary/obj.html)
const Code kh2_objects[] = {
    {"P", "Party (P_)"},   {"B", "Bosses (B_)"},  {"M", "Enemies (M_)"},        {"N", "NPCs (N_)"},
    {"H", "Characters (H_)"}, {"W", "Weapons (W_)"}, {"F", "Field objects (F_)"},
};

// the copies' languages (and the Final Mix edition), as folders (msn/us) or as last extensions (B_EX100.a.us)
const char *const copies[] = {"us", "fr", "gr", "it", "sp", "jp", "fm"};

std::string lower(std::string s)
{
    for (auto &c : s)
        c = char(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool is_copy(const std::string &s)
{
    const std::string l = lower(s);
    for (const char *c : copies)
        if (l == c)
            return true;
    return false;
}

std::vector<std::string> split(const std::string &s, char by)
{
    std::vector<std::string> parts;
    std::size_t from = 0;
    for (;;) {
        const auto at = s.find(by, from);
        parts.push_back(s.substr(from, at == std::string::npos ? std::string::npos : at - from));
        if (at == std::string::npos)
            return parts;
        from = at + 1;
    }
}

bool ends_with(const std::string &s, const char *end)
{
    const std::size_t n = std::strlen(end);
    return s.size() >= n && s.compare(s.size() - n, n, end) == 0;
}

// the world the name's first or second part names: two letters, then digits or nothing ("al00", "tt_event_101",
// "B_AL020", "wp_tw", "donald_wi")
std::string world_in(const std::string &file, Game game)
{
    const std::string stem = lower(file.substr(0, file.find('.')));
    const auto parts = split(stem, '_');
    for (std::size_t i = 0; i < parts.size() && i < 2; ++i) {
        const std::string &p = parts[i];
        if (p.size() < 2 || !std::isalpha(static_cast<unsigned char>(p[0])) ||
            !std::isalpha(static_cast<unsigned char>(p[1])))
            continue;
        bool digits = true;
        for (std::size_t c = 2; c < p.size(); ++c)
            digits = digits && std::isdigit(static_cast<unsigned char>(p[c])) != 0;
        if (!digits)
            continue;
        if (const char *w = world_name(game, p.substr(0, 2)); *w != '\0')
            return w;
    }
    return {};
}

} // namespace

const char *game_name(Game g)
{
    switch (g) {
    case Game::KH1: return "KINGDOM HEARTS";
    case Game::KH2: return "KINGDOM HEARTS II";
    case Game::ReCoM: return "Re:Chain of Memories";
    default: return "Other";
    }
}

const char *world_name(Game game, const std::string &code)
{
    const std::string c = lower(code);
    if (game == Game::KH2) {
        for (const auto &w : kh2_worlds)
            if (c == w.code)
                return w.name;
    } else if (game == Game::KH1) {
        for (const auto &w : kh1_worlds)
            if (c == w.code)
                return w.name;
    }
    return "";
}

Naming name_of(const std::string &relative, Game game)
{
    Naming n;
    auto parts = split(relative, '/');
    n.file = parts.back();
    parts.pop_back();
    // an extraction's "<name>.hed_out" and its "original" (or "remastered") folder
    std::size_t from = 0;
    for (std::size_t i = 0; i < parts.size(); ++i)
        if (ends_with(lower(parts[i]), ".hed_out")) {
            n.place = parts[i];
            from = i + 1;
        }
    if (!n.place.empty() && from < parts.size() &&
        (lower(parts[from]) == "original" || lower(parts[from]) == "remastered"))
        ++from;
    parts.erase(parts.begin(), parts.begin() + std::ptrdiff_t(from));
    // a language's copy: its folder (msn/us/x.bar) or its last extension (obj/x.a.us)
    std::string keyed = n.file;
    if (!parts.empty() && is_copy(parts.back())) {
        n.copy = lower(parts.back());
        parts.pop_back();
    } else if (const auto names = split(n.file, '.'); names.size() > 2 && is_copy(names.back())) {
        n.copy = lower(names.back());
        keyed = n.file.substr(0, n.file.size() - names.back().size()) + "*";
    }
    for (std::size_t i = 0; i < parts.size(); ++i)
        n.folder += (i ? "/" : "") + parts[i];
    n.key = std::string(game_name(game)) + "|" + lower(n.folder) + "|" + lower(keyed);

    n.group = n.folder.empty() ? "Main folder" : n.folder + " folder";
    n.group_rank = n.folder.empty() ? 1000 : 500;
    const std::string folder = lower(n.folder);
    for (std::size_t k = 0; k < std::size(kinds); ++k) {
        const std::string f = kinds[k].folder;
        if (kinds[k].game == game && (folder == f || folder.rfind(f + "/", 0) == 0)) {
            n.group = kinds[k].name;
            n.group_rank = int(k);
            break;
        }
    }

    n.world = world_in(n.file, game);
    n.section = n.world;
    n.section_rank = n.world.empty() ? 1000 : 100;
    if (game == Game::KH2 && n.group == "Characters and objects" && n.file.size() > 2 && n.file[1] == '_' &&
        std::isupper(static_cast<unsigned char>(n.file[0]))) {
        n.section = std::string("Other (") + n.file[0] + "_)";
        n.section_rank = 50;
        for (std::size_t k = 0; k < std::size(kh2_objects); ++k)
            if (n.file[0] == kh2_objects[k].code[0]) {
                n.section = kh2_objects[k].name;
                n.section_rank = int(k);
            }
    }
    return n;
}

std::string effect_title(const std::string &label)
{
    std::string out;
    for (const auto &part : split(label, '/')) {
        std::string p = part;
        while (!p.empty() && p.front() == ' ')
            p.erase(p.begin());
        while (!p.empty() && p.back() == ' ')
            p.pop_back();
        const auto hash = p.rfind('#');
        if (hash != std::string::npos && hash + 1 < p.size())
            p = p.substr(0, hash) + " (entry " + p.substr(hash + 1) + ")";
        out += (out.empty() ? "" : " > ") + p;
    }
    return out;
}

} // namespace pax
