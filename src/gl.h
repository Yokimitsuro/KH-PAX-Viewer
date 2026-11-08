// The OpenGL 3.3 functions the viewer draws with, loaded through SDL once a context exists.
#pragma once

#include <SDL3/SDL_opengl.h>

namespace pax {

struct Gl {
    PFNGLGENBUFFERSPROC GenBuffers = nullptr;
    PFNGLBINDBUFFERPROC BindBuffer = nullptr;
    PFNGLBUFFERDATAPROC BufferData = nullptr;
    PFNGLGENVERTEXARRAYSPROC GenVertexArrays = nullptr;
    PFNGLBINDVERTEXARRAYPROC BindVertexArray = nullptr;
    PFNGLVERTEXATTRIBPOINTERPROC VertexAttribPointer = nullptr;
    PFNGLENABLEVERTEXATTRIBARRAYPROC EnableVertexAttribArray = nullptr;
    PFNGLCREATESHADERPROC CreateShader = nullptr;
    PFNGLSHADERSOURCEPROC ShaderSource = nullptr;
    PFNGLCOMPILESHADERPROC CompileShader = nullptr;
    PFNGLGETSHADERIVPROC GetShaderiv = nullptr;
    PFNGLGETSHADERINFOLOGPROC GetShaderInfoLog = nullptr;
    PFNGLCREATEPROGRAMPROC CreateProgram = nullptr;
    PFNGLATTACHSHADERPROC AttachShader = nullptr;
    PFNGLLINKPROGRAMPROC LinkProgram = nullptr;
    PFNGLGETPROGRAMIVPROC GetProgramiv = nullptr;
    PFNGLUSEPROGRAMPROC UseProgram = nullptr;
    PFNGLGETUNIFORMLOCATIONPROC GetUniformLocation = nullptr;
    PFNGLUNIFORM1IPROC Uniform1i = nullptr;
    PFNGLUNIFORM1FPROC Uniform1f = nullptr;
    PFNGLACTIVETEXTUREPROC ActiveTexture = nullptr;
    PFNGLBLENDEQUATIONPROC BlendEquation = nullptr;
    PFNGLBLENDFUNCSEPARATEPROC BlendFuncSeparate = nullptr;
    PFNGLBLENDCOLORPROC BlendColor = nullptr;
    void(APIENTRY *GenTextures)(GLsizei, GLuint *) = nullptr;
    void(APIENTRY *DeleteTextures)(GLsizei, const GLuint *) = nullptr;
    void(APIENTRY *BindTexture)(GLenum, GLuint) = nullptr;
    void(APIENTRY *TexImage2D)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *) = nullptr;
    void(APIENTRY *TexParameteri)(GLenum, GLenum, GLint) = nullptr;
    void(APIENTRY *DrawArrays)(GLenum, GLint, GLsizei) = nullptr;
    void(APIENTRY *Enable)(GLenum) = nullptr;
    void(APIENTRY *Disable)(GLenum) = nullptr;
    void(APIENTRY *DepthFunc)(GLenum) = nullptr;
    void(APIENTRY *DepthMask)(GLboolean) = nullptr;
    void(APIENTRY *ColorMask)(GLboolean, GLboolean, GLboolean, GLboolean) = nullptr;
    void(APIENTRY *Viewport)(GLint, GLint, GLsizei, GLsizei) = nullptr;
    void(APIENTRY *Scissor)(GLint, GLint, GLsizei, GLsizei) = nullptr;
    void(APIENTRY *ClearColor)(GLfloat, GLfloat, GLfloat, GLfloat) = nullptr;
    void(APIENTRY *ClearDepth)(GLdouble) = nullptr;
    void(APIENTRY *Clear)(GLbitfield) = nullptr;

    // false (and `missing` named) when a function is not there
    bool load(const char **missing);
};

extern Gl gl;

} // namespace pax
