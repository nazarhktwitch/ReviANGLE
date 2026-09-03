// Boost: on-the-fly texture compression
// Intercepts glTexImage2D calls and compresses RGBA8 textures to DXT1 (BC1).
// Result: 4x less VRAM usage on GT 630M.

#include <windows.h>
#include <cstdlib>
#include <cstring>
#include <vector>
#include "config.hpp"
#include "gl_proxy.hpp"
#include "common/dxt1_encoder.hpp"
#include "angle_loader.hpp"

typedef unsigned int GLenum;
typedef int          GLint;
typedef int          GLsizei;

constexpr GLenum GL_RGBA            = 0x1908;
constexpr GLenum GL_UNSIGNED_BYTE   = 0x1401;
constexpr GLenum GL_TEXTURE_2D      = 0x0DE1;
constexpr GLenum GL_COMPRESSED_RGB_S3TC_DXT1_EXT = 0x83F0;

using TexImage2DFn = void(WINAPI*)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*);
using CompressedTexImage2DFn = void(WINAPI*)(GLenum, GLint, GLenum, GLsizei, GLsizei, GLint, GLsizei, const void*);

static TexImage2DFn           s_origTexImage2D = nullptr;
static CompressedTexImage2DFn s_compressedTex  = nullptr;
static bool                   s_active = false;

static void WINAPI hookedTexImage2D(GLenum target, GLint level, GLint internalformat,
    GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void* pixels)
{
    // only compress RGBA8 2D textures that are multiples of 4 and >= 16px
    if (s_active && target == GL_TEXTURE_2D && format == GL_RGBA &&
        type == GL_UNSIGNED_BYTE && pixels &&
        width >= 16 && height >= 16 && (width & 3) == 0 && (height & 3) == 0)
    {
        // Don't compress font atlases or transparent UI sprites (partial alpha)
        const uint8_t* p = (const uint8_t*)pixels;
        size_t total = (size_t)width * height * 4;
        bool hasPartialAlpha = false;
        for (size_t i = 3; i < total; i += 16) { // sample alpha channel
            uint8_t a = p[i];
            if (a > 0 && a < 255) {
                hasPartialAlpha = true;
                break;
            }
        }
        if (hasPartialAlpha) {
            if (s_origTexImage2D) s_origTexImage2D(target, level, internalformat, width, height, border, format, type, pixels);
            return;
        }

        size_t compressedSize = (size_t)(width / 4) * (height / 4) * 8;
        std::vector<uint8_t> compressed(compressedSize);

        size_t wrote = dxt1::compress((const uint8_t*)pixels, width, height, compressed.data());
        if (wrote > 0 && s_compressedTex) {
            s_compressedTex(target, level, GL_COMPRESSED_RGB_S3TC_DXT1_EXT,
                            width, height, border, (GLsizei)wrote, compressed.data());
            return;
        }
    }
    // fallback to original
    if (s_origTexImage2D) s_origTexImage2D(target, level, internalformat, width, height, border, format, type, pixels);
}

namespace boost_tex_compress {
    void apply() {
        if (!Config::get().tex_compress) return;

        s_origTexImage2D = (TexImage2DFn)glproxy::resolve("glTexImage2D");
        s_compressedTex  = (CompressedTexImage2DFn)glproxy::resolve("glCompressedTexImage2D");

        if (s_origTexImage2D && s_compressedTex) {
            s_active = true;
            angle::log("tex_compress: active, RGBA8 -> DXT1");
        }
    }

    // called by gl_proxy when glTexImage2D is invoked
    void* getHook() { return s_active ? (void*)hookedTexImage2D : nullptr; }
}
