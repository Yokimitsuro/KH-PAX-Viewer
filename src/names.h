// What the file list calls a file: its name, the group and section it is listed under (by kind of file, then by
// world: "Event effects" / "Twilight Town"), and the key its copies in each language share. The worlds are the
// games' own two-letter codes in their file names (OpenKh's KINGDOM HEARTS and KINGDOM HEARTS II world lists).
#pragma once

#include <string>

namespace pax {

enum class Game { Unknown, KH1, KH2, ReCoM };
const char *game_name(Game g);

struct Naming {
    std::string file;     // its name on disk
    std::string place;    // the extraction it is in ("kh2_fifth.hed_out"), "" when none
    std::string folder;   // its folder there, a language's folder taken out ("msn/us" -> "msn")
    std::string copy;     // which language's (or edition's) copy it is ("us", "fr", "fm"), "" when none
    std::string key;      // the same for each language's copy of a file (game, folder, name without the language)
    std::string group;    // "Event effects", "Rooms", ... or its folder ("Main folder" when none)
    int group_rank = 0;   // the kinds of file first, in the games' own order
    std::string section;  // "Twilight Town", "Bosses (B_)", or "" (listed after the sections)
    int section_rank = 0; // the kinds of object first, then the worlds, then the files of no section
    std::string world;    // the world its name names, "" when none
};

// `relative`: the file's path from the folder opened, '/' between its parts.
Naming name_of(const std::string &relative, Game game);

// The world a game's two-letter code names ("tt" -> "Twilight Town"), "" when none.
const char *world_name(Game game, const std::string &code);

// An effect's place in its file as the list shows it: "p_ex#0" -> "p_ex (entry 0)", "A#1 / b#0" -> "A (entry 1) >
// b (entry 0)", "at 0x40" kept.
std::string effect_title(const std::string &label);

} // namespace pax
