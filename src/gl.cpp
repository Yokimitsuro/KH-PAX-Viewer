#include "gl.h"

#include <SDL3/SDL_video.h>

namespace pax {

Gl gl;

namespace {

template <class F> bool get(F &f, const char *name, const char **missing)
{
    f = reinterpret_cast<F>(SDL_GL_GetProcAddress(name));
    if (f == nullptr && missing != nullptr)
        *missing = name;
    return f != nullptr;
}

} // namespace

bool Gl::load(const char **missing)
{
    bool ok = true;
#define PAX_GL(f) ok = get(f, "gl" #f, missing) && ok
    PAX_GL(GenBuffers);
    PAX_GL(BindBuffer);
    PAX_GL(BufferData);
    PAX_GL(GenVertexArrays);
    PAX_GL(BindVertexArray);
    PAX_GL(VertexAttribPointer);
    PAX_GL(EnableVertexAttribArray);
    PAX_GL(CreateShader);
    PAX_GL(ShaderSource);
    PAX_GL(CompileShader);
    PAX_GL(GetShaderiv);
    PAX_GL(GetShaderInfoLog);
    PAX_GL(CreateProgram);
    PAX_GL(AttachShader);
    PAX_GL(LinkProgram);
    PAX_GL(GetProgramiv);
    PAX_GL(UseProgram);
    PAX_GL(GetUniformLocation);
    PAX_GL(Uniform1i);
    PAX_GL(Uniform1f);
    PAX_GL(ActiveTexture);
    PAX_GL(BlendEquation);
    PAX_GL(BlendFuncSeparate);
    PAX_GL(BlendColor);
    PAX_GL(GenTextures);
    PAX_GL(DeleteTextures);
    PAX_GL(BindTexture);
    PAX_GL(TexImage2D);
    PAX_GL(TexParameteri);
    PAX_GL(DrawArrays);
    PAX_GL(Enable);
    PAX_GL(Disable);
    PAX_GL(DepthFunc);
    PAX_GL(DepthMask);
    PAX_GL(ColorMask);
    PAX_GL(Viewport);
    PAX_GL(Scissor);
    PAX_GL(ClearColor);
    PAX_GL(ClearDepth);
    PAX_GL(Clear);
#undef PAX_GL
    return ok;
}

} // namespace pax
