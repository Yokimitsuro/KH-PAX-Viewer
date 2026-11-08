#include "renderer.h"

#include "gl.h"

#include <cmath>

namespace pax {

namespace {

const char *vertex_source = R"(#version 330 core
layout(location = 0) in vec4 a_pos;
layout(location = 1) in vec2 a_uv;
layout(location = 2) in vec4 a_col;
out vec2 v_uv;
out vec4 v_col;
void main()
{
    v_uv = a_uv;
    v_col = a_col;
    // the player's depth runs 0 to 1 (w the view's depth); OpenGL's -w to w
    gl_Position = vec4(a_pos.x, a_pos.y, a_pos.z * 2.0 - a_pos.w, a_pos.w);
}
)";

// the GS's MODULATE: the picture times the vertex colour (0x80 = 1, up to 2), alpha likewise
const char *fragment_source = R"(#version 330 core
in vec2 v_uv;
in vec4 v_col;
uniform sampler2D u_tex;
uniform int u_textured;
out vec4 o_col;
void main()
{
    vec4 t = u_textured != 0 ? texture(u_tex, v_uv) : vec4(1.0);
    o_col = t * v_col;
}
)";

unsigned compile(GLenum kind, const char *source, std::string &error)
{
    const unsigned s = gl.CreateShader(kind);
    gl.ShaderSource(s, 1, &source, nullptr);
    gl.CompileShader(s);
    GLint ok = 0;
    gl.GetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024] = {};
        gl.GetShaderInfoLog(s, sizeof log, nullptr, log);
        error = log;
        return 0;
    }
    return s;
}

} // namespace

bool Renderer::init(std::string &error)
{
    const unsigned vs = compile(GL_VERTEX_SHADER, vertex_source, error);
    const unsigned fs = compile(GL_FRAGMENT_SHADER, fragment_source, error);
    if (vs == 0 || fs == 0)
        return false;
    program_ = gl.CreateProgram();
    gl.AttachShader(program_, vs);
    gl.AttachShader(program_, fs);
    gl.LinkProgram(program_);
    GLint ok = 0;
    gl.GetProgramiv(program_, GL_LINK_STATUS, &ok);
    if (!ok) {
        error = "the shaders did not link";
        return false;
    }
    textured_ = gl.GetUniformLocation(program_, "u_textured");
    gl.UseProgram(program_);
    gl.Uniform1i(gl.GetUniformLocation(program_, "u_tex"), 0);
    gl.GenVertexArrays(1, &vao_);
    gl.GenBuffers(1, &vbo_);
    gl.BindVertexArray(vao_);
    gl.BindBuffer(GL_ARRAY_BUFFER, vbo_);
    const GLsizei stride = 10 * sizeof(float);
    gl.VertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, stride, nullptr);
    gl.VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void *>(4 * sizeof(float)));
    gl.VertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void *>(6 * sizeof(float)));
    gl.EnableVertexAttribArray(0);
    gl.EnableVertexAttribArray(1);
    gl.EnableVertexAttribArray(2);
    return true;
}

void Renderer::begin(int x, int y, int w, int h, const float background[3])
{
    gl.Viewport(x, y, w, h);
    gl.Scissor(x, y, w, h);
    gl.Enable(GL_SCISSOR_TEST);
    gl.ClearColor(background[0], background[1], background[2], 1.0F);
    gl.ClearDepth(1.0);
    gl.DepthMask(GL_TRUE);
    gl.ColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    gl.Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    triangles = calls = 0;
}

void Renderer::forget()
{
    for (auto &[key, t] : pages_)
        gl.DeleteTextures(1, &t);
    pages_.clear();
}

unsigned Renderer::page_texture(const std::vector<const kh2::EffectPicture *> &pictures, const void *owner,
                                std::uint64_t tex0)
{
    if (tex0 == 0)
        return 0;
    const auto key = std::make_pair(owner, tex0);
    if (const auto it = pages_.find(key); it != pages_.end())
        return it->second;
    const auto page = kh2::effect_page(pictures, tex0);
    unsigned t = 0;
    gl.GenTextures(1, &t);
    gl.BindTexture(GL_TEXTURE_2D, t);
    gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, page.width, page.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, page.rgba.data());
    const GLint filter = smooth ? GL_LINEAR : GL_NEAREST;
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    pages_[key] = t;
    return t;
}

// The GS's ALPHA byte: (A - B) x C + D, A, B and D each the source colour, the screen's or nothing, C the source's
// alpha, the screen's or FIX (taken as 1: the player gives no FIX). The mixes the effects use, as blending.
void Renderer::blend(std::uint8_t alpha)
{
    const int a = alpha & 3, b = (alpha >> 2) & 3, c = (alpha >> 4) & 3, d = (alpha >> 6) & 3;
    const GLenum factor = c == 0 ? GL_SRC_ALPHA : c == 1 ? GL_DST_ALPHA : GL_ONE;
    const GLenum minus = c == 0 ? GL_ONE_MINUS_SRC_ALPHA : c == 1 ? GL_ONE_MINUS_DST_ALPHA : GL_ZERO;
    gl.Enable(GL_BLEND);
    gl.BlendEquation(GL_FUNC_ADD);
    if (a == 0 && b == 1 && d == 1) { // between the source and the screen
        gl.BlendFuncSeparate(factor, minus, GL_ZERO, GL_ONE);
    } else if (a == 0 && b == 2 && d == 1) { // added
        gl.BlendFuncSeparate(factor, GL_ONE, GL_ZERO, GL_ONE);
    } else if (a == 2 && b == 0 && d == 1) { // taken away
        gl.BlendEquation(GL_FUNC_REVERSE_SUBTRACT);
        gl.BlendFuncSeparate(factor, GL_ONE, GL_ZERO, GL_ONE);
    } else if (a == 0 && b == 2 && d == 2) { // the source alone, scaled
        gl.BlendFuncSeparate(factor, GL_ZERO, GL_ZERO, GL_ONE);
    } else if (a == 2 && b == 1 && d == 1) { // the screen darkened
        gl.BlendFuncSeparate(GL_ZERO, minus, GL_ZERO, GL_ONE);
    } else { // others: as between the source and the screen
        gl.BlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE);
    }
}

void Renderer::submit(const std::vector<kh2::EffectVertex> &v, unsigned mode, unsigned texture)
{
    scratch_.resize(v.size() * 10);
    for (std::size_t i = 0; i < v.size(); ++i) {
        const auto &x = v[i];
        const float f[10] = {x.x, x.y, x.z, x.w, x.u, x.v, x.r, x.g, x.b, x.a};
        std::copy(f, f + 10, scratch_.begin() + std::ptrdiff_t(i * 10));
    }
    gl.UseProgram(program_);
    gl.BindVertexArray(vao_);
    gl.BindBuffer(GL_ARRAY_BUFFER, vbo_);
    gl.BufferData(GL_ARRAY_BUFFER, GLsizeiptr(scratch_.size() * sizeof(float)), scratch_.data(), GL_STREAM_DRAW);
    gl.Uniform1i(textured_, texture != 0 ? 1 : 0);
    gl.ActiveTexture(GL_TEXTURE0);
    gl.BindTexture(GL_TEXTURE_2D, texture);
    gl.DrawArrays(mode, 0, GLsizei(v.size()));
    ++calls;
}

void Renderer::grid(const kh2::EffectCamera &camera, float spacing, int lines)
{
    // the ground (y 0) through view and projection, as row vectors
    kh2::EffectMatrix vp{};
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c) {
            float s = 0;
            for (int k = 0; k < 4; ++k)
                s += camera.view[std::size_t(r * 4 + k)] * camera.projection[std::size_t(k * 4 + c)];
            vp[std::size_t(r * 4 + c)] = s;
        }
    std::vector<kh2::EffectVertex> v;
    const float half = spacing * float(lines);
    auto put = [&](float x, float z, float shade) {
        kh2::EffectVertex e;
        const float p[4] = {x, 0, z, 1};
        float o[4] = {};
        for (int c = 0; c < 4; ++c)
            for (int k = 0; k < 4; ++k)
                o[c] += p[k] * vp[std::size_t(k * 4 + c)];
        e.x = o[0], e.y = o[1], e.z = o[2], e.w = o[3];
        e.r = e.g = e.b = shade;
        e.a = 0.6F;
        v.push_back(e);
    };
    for (int i = -lines; i <= lines; ++i) {
        const float t = spacing * float(i), shade = i == 0 ? 0.55F : 0.3F;
        put(t, -half, shade), put(t, half, shade);
        put(-half, t, shade), put(half, t, shade);
    }
    gl.Enable(GL_DEPTH_TEST);
    gl.DepthFunc(GL_LEQUAL);
    gl.DepthMask(GL_FALSE);
    blend(0x44);
    submit(v, GL_LINES, 0);
}

void Renderer::draw(const std::vector<kh2::EffectDraw> &draws, const std::vector<const kh2::EffectPicture *> &pictures,
                    const void *owner)
{
    for (const auto &d : draws) {
        if (d.triangles.empty() || (d.screen && !show_screen))
            continue;
        if (d.depth_test) {
            gl.Enable(GL_DEPTH_TEST);
            gl.DepthFunc(GL_LEQUAL);
        } else {
            gl.Disable(GL_DEPTH_TEST);
        }
        gl.DepthMask(d.depth_write || d.depth_only ? GL_TRUE : GL_FALSE);
        const GLboolean colour = d.depth_only ? GL_FALSE : GL_TRUE;
        gl.ColorMask(colour, colour, colour, colour);
        blend(d.alpha);
        submit(d.triangles, GL_TRIANGLES, d.screen ? 0 : page_texture(pictures, owner, d.tex0));
        triangles += d.triangles.size() / 3;
    }
    gl.ColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    gl.DepthMask(GL_TRUE);
    gl.Disable(GL_BLEND);
}

} // namespace pax
