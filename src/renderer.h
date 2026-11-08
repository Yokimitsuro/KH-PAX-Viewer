// Draws what the effect player gives (triangles in clip space, a GS page each, the GS blend) with OpenGL 3.3.
#pragma once

#include "khdays/assets/kh2_effect.h"

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace pax {

namespace kh2 = khdays::assets::kh2;

class Renderer {
public:
    bool init(std::string &error);
    // the area drawn in (window pixels, y up from the bottom), cleared to `background`
    void begin(int x, int y, int w, int h, const float background[3]);
    void grid(const kh2::EffectCamera &camera, float spacing, int lines);
    // `owner` names the pictures' set (a block), for the pages kept made
    void draw(const std::vector<kh2::EffectDraw> &draws, const std::vector<const kh2::EffectPicture *> &pictures,
              const void *owner);
    void forget(); // the pages made so far dropped
    // a page as RGBA, for a preview: its texture (0 when there is none)
    unsigned page_texture(const std::vector<const kh2::EffectPicture *> &pictures, const void *owner,
                          std::uint64_t tex0);

    bool smooth = true;      // bilinear pages (the PS2's effects mostly were)
    bool show_screen = false; // the draws of the screen behind (distortions): drawn untextured when shown
    std::size_t triangles = 0, calls = 0;

private:
    void blend(std::uint8_t alpha);
    void submit(const std::vector<kh2::EffectVertex> &v, unsigned mode, unsigned texture);

    unsigned program_ = 0, vao_ = 0, vbo_ = 0;
    int textured_ = -1;
    std::map<std::pair<const void *, std::uint64_t>, unsigned> pages_;
    std::vector<float> scratch_;
};

} // namespace pax
