# KH PAX Viewer

A standalone Windows desktop application that plays the particle effects of KINGDOM HEARTS games, from files
you extract from your own copy:

- **KINGDOM HEARTS II** (FINAL MIX): `.pax` files, and the effects inside its BAR containers (`.mdlx`, `.mset`,
  `.bar`, `.a.*`);
- **KINGDOM HEARTS** (FINAL MIX): `.dpx` files;
- **KINGDOM HEARTS Re:Chain of Memories**: the effects kept inside its own files (turn on *Look inside*).

The three games share one effect format: a KH2 PAX is a header followed by the same effect data (a `0x82`
directory of `0x96` blocks) that KH1's and Re:CoM's `.dpx` hold alone. Each block holds emitters (what plays),
pictures (PlayStation 2 pages with palettes), sprites and small models; an emitter runs layers of commands, frame by
frame. The viewer plays them with the effect player of
[khdays-port](https://github.com/Yokimitsuro/khdays-port) (`assets/src/kh2_effect.cpp`), which reproduces
KINGDOM HEARTS II's own effect interpreter, all 130 commands KH2's effects use.

No game files are included or downloaded. You need the games and a tool that extracts their files.

AI-assisted coding tools were used to help make this possible.

## Features

- Open files, or a folder searched with its subfolders; the folder opened is loaded again at the next start, and
  the emitter last chosen is chosen again. A collection's `Image\dt` folder can be opened whole: each extraction's
  `remastered` folder (HD pictures and sounds) is passed over, and its `<name>.hed_out\original` is left out of the
  names.
- Files grouped by game, then by kind of file (*Event effects*, *Rooms*, *Missions*, *Characters and objects*,
  *Magic*, *Limits*, *World map*), then by the world their names give (`tt_event_101.pax`: *Twilight Town*) or, for
  KINGDOM HEARTS II's objects, by the letter their names begin with (*Bosses (B_)*, *Enemies (M_)*, *Party (P_)*,
  ...); the worlds as OpenKh lists the games' codes. A filter by name, folder, world or kind.
- A file's copies for each language (`msn/us`, `msn/fr`, ..., `B_EX100.a.us`, `.a.fr`, ...) listed once when their
  effects are the same, the `us` copy kept; hovering over a file shows its path and its copies.
- Opening a file plays its first emitter; the arrow keys and *Previous* / *Next* go through the list.
- The list as wide as wanted (drag its right edge) and the text as large (*Text size*, or Ctrl + mouse wheel); both
  kept for the next start.
- Play, pause, step frame by frame, restart, speed from 0.1x to 4x, loop; *Stop repeating* closes a repeating
  effect as the game does.
- A 3D view with an orbit camera, a ground grid and a background colour of your choice.
- The pages (pictures) the effect draws with, as the PlayStation 2 placed them.
- *Swing points*: two points swung round as a weapon's base and tip, for the effects that draw a swing's trail.
- KH2's resident pictures (`effect/texcommon.dpd`) are used when the folder holds them.

## Requirements

- Windows x64 with a graphics driver that supports OpenGL 3.3.
- To build: Visual Studio 2022 or later with *Desktop development with C++* (it brings CMake and Ninja), and an
  internet connection the first time (CMake downloads SDL3 and Dear ImGui).

## Quick start

1. Build it with `Build.bat` (or take `KH PAX Viewer.exe` from a release).
2. Run `KH PAX Viewer.exe`, choose **Open folder...** and pick a folder of extracted files (for example the
   collection's `Image\dt` folder of extractions, KH2's `original` folder, or its `effect` and `obj` folders).
3. Open a file (it plays its first emitter) or select an emitter.

From a command line: `"KH PAX Viewer.exe" <files or folders> [--play <name> <block> <emitter>]` plays the first
file whose name holds `<name>`.

## Controls

| Action | Control |
| --- | --- |
| Orbit | Drag with the left mouse button |
| Pan | Drag with the right or middle button, or Shift + left |
| Zoom | Mouse wheel |
| Play / pause | Space, or *Play* / *Pause* |
| Step one frame (paused) | `.` (period), or *Step* |
| Restart | R, or *Restart* |
| The next / previous emitter in the list | Down / Up arrow, or *Next* / *Previous* |
| The next / previous file in the list | Page Down / Page Up |
| Text size | Ctrl + mouse wheel, Ctrl + plus / minus, Ctrl + 0 (back to 1x) |
| The list's width | Drag its right edge |

## What is not shown

- Draws that copy the screen behind (heat haze, distortions) are not drawn; *Screen copies* shows them as plain
  shapes.
- Effects that need the game (a character's points, the gummi ships' matrices) show only what they draw without
  it.
- A command an effect file uses and the player does not know yet is named under the controls.

## Building

```
Build.bat
```

builds `build\Release\KH PAX Viewer.exe` and runs the tests (made-up files, nothing of the games).

`tools/sync_player.py <khdays-port checkout>` copies the effect player from khdays-port, where it is developed.

## License

MIT (see `LICENSE`). Third-party libraries: `THIRD_PARTY_NOTICES.md`. KINGDOM HEARTS is a trademark of its
owners; this project is not affiliated with them and includes none of their files.
