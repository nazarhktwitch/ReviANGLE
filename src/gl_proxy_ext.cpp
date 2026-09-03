// GL 2.0+ / GLES 3.0 wrapper functions for mod compatibility
// These forward to ANGLE's libGLESv2.dll via glproxy::resolve

#include "gl_proxy.hpp"
#include "angle_loader.hpp"
#include "config.hpp"
#include <windows.h>
#include <cstddef>
#include <cctype>
#include <algorithm>

// GL types
typedef unsigned int   GLenum;
typedef unsigned int   GLbitfield;
typedef unsigned int   GLuint;
typedef int            GLint;
typedef int            GLsizei;
typedef unsigned char  GLboolean;
typedef unsigned char  GLubyte;
typedef signed char    GLbyte;
typedef short          GLshort;
typedef unsigned short GLushort;
typedef float          GLfloat;
typedef double         GLdouble;
typedef char           GLchar;
typedef void           GLvoid;
typedef ptrdiff_t      GLintptr;
typedef ptrdiff_t      GLsizeiptr;
typedef long long          GLint64;
typedef unsigned long long GLuint64;

#ifndef GL_FALSE
#define GL_FALSE 0
#endif
#ifndef GL_TRUE
#define GL_TRUE  1
#endif

namespace glproxy {
    void* resolve(const char* name);
}

#define GLP_EXT_FORWARD(ret, name, sig, args) \
    typedef ret (WINAPI *PFN_##name)sig; \
    extern "C" __declspec(dllexport) ret WINAPI gl_gl##name sig { \
        static PFN_##name p = nullptr; \
        if (!p) p = (PFN_##name)glproxy::resolve("gl" #name); \
        return p ? p args : (ret)0; \
    }

#define GLP_EXT_FORWARD_VOID(name, sig, args) \
    typedef void (WINAPI *PFN_##name)sig; \
    extern "C" __declspec(dllexport) void WINAPI gl_gl##name sig { \
        static PFN_##name p = nullptr; \
        if (!p) p = (PFN_##name)glproxy::resolve("gl" #name); \
        if (p) p args; \
    }

// Read/Draw buffers
GLP_EXT_FORWARD_VOID(ReadBuffer, (GLenum src), (src))

// glDrawBuffers — instrumented to track draw-buffer disabling by mods (MegaHack).
// When a mod disables draw buffers on the default framebuffer, all subsequent
// game draws produce no color output → black screen until scene re-init.
typedef void (WINAPI *PFN_DB_PLURAL)(GLsizei, const GLenum*);
extern "C" __declspec(dllexport) void WINAPI gl_glDrawBuffers(GLsizei n, const GLenum* bufs) {
    static PFN_DB_PLURAL p = nullptr;
    if (!p) p = (PFN_DB_PLURAL)glproxy::resolve("glDrawBuffers");
    static int dn = 0;
    if (dn < 60) {
        char desc[128] = {};
        if (n == 0) {
            snprintf(desc, sizeof(desc), "n=0 (no buffers!)");
        } else {
            int off = 0;
            for (int i = 0; i < n && i < 8 && off < (int)sizeof(desc)-16; i++) {
                off += snprintf(desc+off, sizeof(desc)-off, "%s0x%04X",
                    i ? "," : "", bufs[i]);
            }
        }
        angle::forceLog("glDrawBuffers #%d: n=%d bufs=[%s]", dn, n, desc);
        dn++;
    }
    if (p) p(n, bufs);
}

// glDrawBuffer is desktop-GL only; ANGLE/GLES has only the plural glDrawBuffers.
// Compatibility shim for mods written against desktop GL (peony.silicate etc.):
// translate the singular form into the plural form.
extern "C" __declspec(dllexport) void WINAPI gl_glDrawBuffer(GLenum mode) {
    static PFN_DB_PLURAL p = nullptr;
    if (!p) p = (PFN_DB_PLURAL)glproxy::resolve("glDrawBuffers");
    static int dn = 0;
    if (dn < 60) {
        angle::forceLog("glDrawBuffer #%d: mode=0x%04X %s%s", dn, mode,
            mode == 0 ? "(GL_NONE — DISABLES COLOR OUTPUT!)" : "",
            Config::get().megahack_detected ? " [MEGAHACK]" : "");
        dn++;
    }
    if (p) p(1, &mode);
}

// glBindSampler — GLES 3.0 sampler-object binding. Direct forward.
GLP_EXT_FORWARD_VOID(BindSampler, (GLuint unit, GLuint sampler), (unit, sampler))

// glBlendEquationSeparate — GLES 2.0+, separate RGB / A equations. Direct forward.
GLP_EXT_FORWARD_VOID(BlendEquationSeparate, (GLenum modeRGB, GLenum modeAlpha), (modeRGB, modeAlpha))

// Vertex-attrib query getters — GLES 2.0+, direct forward.
GLP_EXT_FORWARD_VOID(GetVertexAttribiv, (GLuint index, GLenum pname, GLint* params), (index, pname, params))
GLP_EXT_FORWARD_VOID(GetVertexAttribPointerv, (GLuint index, GLenum pname, void** pointer), (index, pname, pointer))

// CopyTex
extern "C" __declspec(dllexport) void WINAPI gl_glCopyTexImage2D(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border) {
    typedef void (WINAPI *PFN_CTI)(GLenum, GLint, GLenum, GLint, GLint, GLsizei, GLsizei, GLint);
    static PFN_CTI p = nullptr;
    if (!p) p = (PFN_CTI)glproxy::resolve("glCopyTexImage2D");
    static int n = 0;
    if (Config::get().megahack_detected && n < 80) {
        angle::forceLog("glCopyTexImage2D #%d: target=0x%04X level=%d ifmt=0x%04X src=%d,%d size=%dx%d border=%d", n, target, level, internalformat, x, y, width, height, border);
        n++;
    }
    if (p) p(target, level, internalformat, x, y, width, height, border);
}
extern "C" __declspec(dllexport) void WINAPI gl_glCopyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height) {
    typedef void (WINAPI *PFN_CTSI)(GLenum, GLint, GLint, GLint, GLint, GLint, GLsizei, GLsizei);
    static PFN_CTSI p = nullptr;
    if (!p) p = (PFN_CTSI)glproxy::resolve("glCopyTexSubImage2D");
    static int n = 0;
    if (Config::get().megahack_detected && n < 80) {
        angle::forceLog("glCopyTexSubImage2D #%d: target=0x%04X level=%d dst=%d,%d src=%d,%d size=%dx%d", n, target, level, xoffset, yoffset, x, y, width, height);
        n++;
    }
    if (p) p(target, level, xoffset, yoffset, x, y, width, height);
}

// GetTexParameter
GLP_EXT_FORWARD_VOID(GetTexParameteriv, (GLenum target, GLenum pname, GLint* params), (target, pname, params))
GLP_EXT_FORWARD_VOID(GetTexParameterfv, (GLenum target, GLenum pname, GLfloat* params), (target, pname, params))

// Buffers
GLP_EXT_FORWARD_VOID(GenBuffers, (GLsizei n, GLuint* buffers), (n, buffers))

// glBindBuffer dedup — cocos2d binds the same VBO/IBO on every sprite draw.
// Track per-target last-bound buffer; unknown targets fall through.
//
// File-scope so glBufferData / glBufferSubData can also query "is anything
// bound to this target?" — ANGLE crashes if the answer is no, desktop-GL
// silently no-ops (some Geode mods rely on the silent behaviour).
static int s_bufTargetSlot(GLenum target) {
    switch (target) {
        case 0x8892: return 0;  // GL_ARRAY_BUFFER
        case 0x8893: return 1;  // GL_ELEMENT_ARRAY_BUFFER
        case 0x88EB: return 2;  // GL_PIXEL_PACK_BUFFER
        case 0x88EC: return 3;  // GL_PIXEL_UNPACK_BUFFER
        case 0x8A11: return 4;  // GL_UNIFORM_BUFFER
        case 0x8C8E: return 5;  // GL_TRANSFORM_FEEDBACK_BUFFER
        case 0x8F36: return 6;  // GL_COPY_READ_BUFFER
        case 0x8F37: return 7;  // GL_COPY_WRITE_BUFFER
        default:     return -1; // pass through
    }
}
thread_local GLuint g_bufferBindings[8] = {
    0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
    0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu
};
// Returns true if the caller asked us to operate on a buffer target with no
// real buffer bound (binding is 0 or sentinel "never set"). ANGLE crashes,
// desktop-GL no-ops; we mimic desktop.
static inline bool gd_noBufferBound(GLenum target) {
    int slot = s_bufTargetSlot(target);
    if (slot < 0) return false;  // unknown target — let ANGLE handle
    GLuint b = g_bufferBindings[slot];
    return b == 0 || b == 0xFFFFFFFFu;
}
extern "C" void gdangle_invalidateVAPCache();  // forward decl
typedef void (WINAPI *PFN_BB)(GLenum, GLuint);
extern "C" __declspec(dllexport) void WINAPI gl_glBindBuffer(GLenum target, GLuint buffer) {
    static PFN_BB p = nullptr;
    if (!p) p = (PFN_BB)glproxy::resolve("glBindBuffer");
    int slot = s_bufTargetSlot(target);
    if (slot >= 0) {
        if (g_bufferBindings[slot] == buffer) return;
        g_bufferBindings[slot] = buffer;
        // ARRAY_BUFFER change → VAP cache must be invalidated (pointer offsets
        // are interpreted relative to the bound array buffer).
        if (slot == 0) gdangle_invalidateVAPCache();
    }
    if (p) p(target, buffer);
}
// glBufferData / glBufferSubData — desktop-GL parity guard: no-op when no
// buffer is bound to the target (ANGLE crashes deep in libGLESv2 otherwise).
typedef void (WINAPI *PFN_BD)(GLenum, GLsizeiptr, const void*, GLenum);
extern "C" __declspec(dllexport) void WINAPI gl_glBufferData(GLenum target, GLsizeiptr size, const void* data, GLenum usage) {
    static PFN_BD p = nullptr;
    if (!p) p = (PFN_BD)glproxy::resolve("glBufferData");
    if (gd_noBufferBound(target)) return;  // desktop-GL parity: silent no-op
    if (p) p(target, size, data, usage);
}
typedef void (WINAPI *PFN_BSD)(GLenum, GLintptr, GLsizeiptr, const void*);
extern "C" __declspec(dllexport) void WINAPI gl_glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void* data) {
    static PFN_BSD p = nullptr;
    if (!p) p = (PFN_BSD)glproxy::resolve("glBufferSubData");
    if (gd_noBufferBound(target)) return;  // desktop-GL parity: silent no-op
    if (p) p(target, offset, size, data);
}
typedef void (WINAPI *PFN_DeleteBuffers)(GLsizei, const GLuint*);
extern "C" __declspec(dllexport) void WINAPI gl_glDeleteBuffers(GLsizei n, const GLuint* buffers) {
    static PFN_DeleteBuffers p = nullptr;
    if (!p) p = (PFN_DeleteBuffers)glproxy::resolve("glDeleteBuffers");
    if (p) p(n, buffers);
    if (!buffers || n <= 0) return;
    bool invalidateVAP = false;
    for (GLsizei i = 0; i < n; ++i) {
        GLuint dead = buffers[i];
        if (!dead) continue;
        for (int slot = 0; slot < 8; ++slot) {
            if (g_bufferBindings[slot] == dead) {
                g_bufferBindings[slot] = 0;
                if (slot == 0) invalidateVAP = true;
            }
        }
    }
    if (invalidateVAP) gdangle_invalidateVAPCache();
}
GLP_EXT_FORWARD(void*, MapBufferRange, (GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access), (target, offset, length, access))
GLP_EXT_FORWARD(GLboolean, UnmapBuffer, (GLenum target), (target))
GLP_EXT_FORWARD(GLboolean, IsBuffer, (GLuint buffer), (buffer))
GLP_EXT_FORWARD_VOID(GetBufferParameteriv, (GLenum target, GLenum pname, GLint* params), (target, pname, params))

// VAO
GLP_EXT_FORWARD_VOID(GenVertexArrays, (GLsizei n, GLuint* arrays), (n, arrays))

// VAO bind dedup. Also clears VertexAttribArray bitmask cache because the
// enabled-attrib state is per-VAO and switches with the VAO.
thread_local unsigned int g_vaaEnabledMask = 0;  // bit i = attrib i enabled
thread_local GLuint        g_currentVAO     = 0xFFFFFFFFu;
typedef void (WINAPI *PFN_BVA)(GLuint);
extern "C" __declspec(dllexport) void WINAPI gl_glBindVertexArray(GLuint array) {
    static PFN_BVA p = nullptr;
    if (!p) p = (PFN_BVA)glproxy::resolve("glBindVertexArray");
    if (array == g_currentVAO) return;
    g_currentVAO = array;
    g_vaaEnabledMask = 0;          // VAA enabled-state is per-VAO
    gdangle_invalidateVAPCache();  // VertexAttribPointer state is also per-VAO
    if (p) p(array);
}
GLP_EXT_FORWARD_VOID(DeleteVertexArrays, (GLsizei n, const GLuint* arrays), (n, arrays))
GLP_EXT_FORWARD(GLboolean, IsVertexArray, (GLuint array), (array))

// Shaders — track type per ID for source patching
#include <unordered_map>
#include <string>
#include <vector>
#include <array>
#include <cstring>
#include <mutex>
static std::unordered_map<GLuint, GLenum> g_shaderType;
static std::mutex g_shaderTypeMtx;

// Link-status tracking. ANGLE crashes (__fastfail / illegal instruction
// inside libGLESv2's stream-translator) when glDrawArrays / glDrawElements
// is called with a non-linked program currently bound — this is desktop-GL
// "tolerated, draws nothing" vs ANGLE "fastfail and take down the game".
//
// peony.silicate ships shaders with desktop-only #extension directives
// (GL_ARB_explicit_attrib_location, GL_ARB_explicit_uniform_location);
// even after our shader translator strips those, the underlying
// `layout(location=N) uniform` syntax still fails in ESSL3, so the
// program never links. Silicate then proceeds to use the broken program
// for drawing — desktop-GL would just produce nothing, ANGLE crashes.
//
// Track each program's last-known link status. gl_glDrawArrays /
// glDrawElements consult this map (via gdangle_currentProgramOK) and
// silently skip the draw if the bound program failed to link.
#include <atomic>
static std::atomic<int8_t> g_programLinked[16384] = {};

typedef GLuint (WINAPI *PFN_CS)(GLenum);
extern "C" __declspec(dllexport) GLuint WINAPI gl_glCreateShader(GLenum type) {
    static PFN_CS p = nullptr;
    if (!p) p = (PFN_CS)glproxy::resolve("glCreateShader");
    GLuint id = p ? p(type) : 0;
    if (id == 0) {
        angle::forceLog("CreateShader FAIL: type=0x%04X (resolved=%d)", type, p ? 1 : 0);
    } else {
        std::lock_guard<std::mutex> lk(g_shaderTypeMtx); g_shaderType[id] = type;
    }
    return id;
}
GLP_EXT_FORWARD_VOID(DeleteShader, (GLuint shader), (shader))

// Patch shader source for ANGLE GLES backend compatibility.
//
// Two distinct cases handled:
//
// (1) Source has NO #version directive (cocos2d-x 2.2 GD shaders):
//     Prepend "#version 100\nprecision mediump ...\n" so ANGLE's GLES
//     translator accepts the legacy desktop GLSL 1.10 syntax.
//
// (2) Source HAS a #version directive (Eclipse Menu / ImGui shaders, mods):
//     - GLSL #version values map to ES like so:
//         110 / 120          (attribute/varying syntax) -> "#version 100"
//         130 / 140 / 150 /
//         330 / 400 / 410+   (in/out syntax)            -> "#version 300 es"
//         100 (already ES)                              -> keep as-is
//         300 es / 310 es / 320 es                      -> keep as-is
//     - The #version directive MUST be the first non-comment/whitespace
//       token in the source. So we replace the existing line in-place
//       and inject the precision qualifier on the line *after* it, never
//       before. (Previous version prepended precision unconditionally,
//       which broke shaders that already had #version because ANGLE
//       complained "'version' directive must occur before anything else".)
static void stripUniformLayouts(std::string& src) {
    size_t pos = 0;
    while ((pos = src.find("layout", pos)) != std::string::npos) {
        if (pos > 0 && (std::isalnum(src[pos - 1]) || src[pos - 1] == '_')) {
            pos += 6;
            continue;
        }
        size_t endOfWord = pos + 6;
        if (endOfWord < src.size() && (std::isalnum(src[endOfWord]) || src[endOfWord] == '_')) {
            pos += 6;
            continue;
        }

        size_t openParen = src.find('(', endOfWord);
        if (openParen == std::string::npos) {
            pos += 6;
            continue;
        }
        bool onlyWs = true;
        for (size_t i = endOfWord; i < openParen; ++i) {
            if (!std::isspace((unsigned char)src[i])) {
                onlyWs = false;
                break;
            }
        }
        if (!onlyWs) {
            pos += 6;
            continue;
        }

        size_t closeParen = src.find(')', openParen);
        if (closeParen == std::string::npos) {
            pos += 6;
            continue;
        }

        size_t nextWordStart = closeParen + 1;
        while (nextWordStart < src.size() && std::isspace((unsigned char)src[nextWordStart])) {
            nextWordStart++;
        }

        if (nextWordStart + 7 <= src.size() && src.compare(nextWordStart, 7, "uniform") == 0) {
            char nextChar = (nextWordStart + 7 < src.size()) ? src[nextWordStart + 7] : '\0';
            if (std::isspace((unsigned char)nextChar) || nextChar == ';') {
                for (size_t i = pos; i <= closeParen; ++i) {
                    src[i] = ' ';
                }
            }
        }
        pos = closeParen + 1;
    }
}

static void replaceTextureFunctions(std::string& src) {
    size_t pos = 0;
    while ((pos = src.find("texture2D", pos)) != std::string::npos) {
        bool isWord = true;
        if (pos > 0 && (std::isalnum(src[pos - 1]) || src[pos - 1] == '_')) isWord = false;
        size_t endOfWord = pos + 9;
        if (endOfWord < src.size() && (std::isalnum(src[endOfWord]) || src[endOfWord] == '_')) {
            if (src.compare(pos, 13, "texture2DProj") == 0) {
                src.replace(pos, 13, "textureProj  ");
                pos += 13;
                continue;
            } else if (src.compare(pos, 12, "texture2DLod") == 0) {
                src.replace(pos, 12, "textureLod  ");
                pos += 12;
                continue;
            }
            isWord = false;
        }
        if (isWord) {
            src.replace(pos, 9, "texture  ");
            pos += 9;
        } else {
            pos += 9;
        }
    }

    pos = 0;
    while ((pos = src.find("textureCube", pos)) != std::string::npos) {
        bool isWord = true;
        if (pos > 0 && (std::isalnum(src[pos - 1]) || src[pos - 1] == '_')) isWord = false;
        size_t endOfWord = pos + 11;
        if (endOfWord < src.size() && (std::isalnum(src[endOfWord]) || src[endOfWord] == '_')) {
            if (src.compare(pos, 14, "textureCubeLod") == 0) {
                src.replace(pos, 14, "textureLod    ");
                pos += 14;
                continue;
            }
            isWord = false;
        }
        if (isWord) {
            src.replace(pos, 11, "texture    ");
            pos += 11;
        } else {
            pos += 11;
        }
    }
}

typedef void (WINAPI *PFN_SS)(GLuint, GLsizei, const GLchar* const*, const GLint*);

extern "C" __declspec(dllexport) void WINAPI gl_glShaderSource(GLuint shader, GLsizei count, const GLchar* const* string, const GLint* length) {
    static PFN_SS p = nullptr;
    if (!p) p = (PFN_SS)glproxy::resolve("glShaderSource");
    if (!p) return;

    // Concatenate all input strings to inspect
    std::string src;
    for (GLsizei i = 0; i < count; i++) {
        if (!string[i]) continue;
        if (length && length[i] > 0) src.append(string[i], length[i]);
        else src.append(string[i]);
    }

    bool isFragment = false;
    { std::lock_guard<std::mutex> lk(g_shaderTypeMtx);
      auto it = g_shaderType.find(shader);
      if (it != g_shaderType.end() && it->second == 0x8B30 /*GL_FRAGMENT_SHADER*/) isFragment = true;
    }
    // Fallback: mods (e.g. MegaHack, silicate) may create shaders via
    // GetProcAddress(libGLESv2.dll) bypassing our gl_glCreateShader proxy,
    // so g_shaderType won't know the type. Detect fragment shaders by
    // builtins that only exist in fragment stage.
    if (!isFragment) {
        if (std::strstr(src.c_str(), "gl_FragColor") ||
            std::strstr(src.c_str(), "gl_FragCoord") ||
            std::strstr(src.c_str(), "gl_FragData") ||
            std::strstr(src.c_str(), "gl_FragDepth") ||
            std::strstr(src.c_str(), "gl_PointCoord") ||
            std::strstr(src.c_str(), "gl_FrontFacing") ||
            std::strstr(src.c_str(), "mainImage")) {
            isFragment = true;
        }
    }

    // Locate the #version directive (skipping leading comments/whitespace),
    // but ignore occurrences inside line comments.
    size_t versionPos = std::string::npos;
    {
        size_t pos = 0;
        while (pos < src.size()) {
            size_t lineEnd = src.find('\n', pos);
            if (lineEnd == std::string::npos) lineEnd = src.size();
            size_t nonWs = pos;
            while (nonWs < lineEnd &&
                   (src[nonWs] == ' ' || src[nonWs] == '\t' || src[nonWs] == '\r'))
                nonWs++;
            if (nonWs + 8 <= src.size() && src.compare(nonWs, 8, "#version") == 0) {
                versionPos = nonWs;
                break;
            }
            if (lineEnd == src.size()) break;
            pos = lineEnd + 1;
        }
    }
    std::string patched;

    if (versionPos == std::string::npos) {
        // Case 1: no #version — legacy cocos2d-x style. Prepend ES 1.00 +
        // precision, then the original source as-is.
        std::string prefix = "#version 100\n";
        if (isFragment) prefix += "precision mediump float;\nprecision mediump int;\n";
        else            prefix += "precision mediump int;\n";
        patched = prefix + src;
    } else {
        // Case 2: source has #version. Find the end of that line.
        size_t lineEnd = src.find('\n', versionPos);
        if (lineEnd == std::string::npos) lineEnd = src.size();
        std::string verLine = src.substr(versionPos, lineEnd - versionPos);

        // Parse the version number. Format: "#version <NNN>[ es]?"
        int verNum = 0;
        bool isEs = verLine.find(" es") != std::string::npos;
        size_t numPos = verLine.find_first_of("0123456789");
        if (numPos != std::string::npos) {
            for (size_t i = numPos; i < verLine.size() && verLine[i] >= '0' && verLine[i] <= '9'; i++) {
                verNum = verNum * 10 + (verLine[i] - '0');
            }
        }

        // Decide on the translated #version line.
        std::string newVerLine;
        if (isEs) {
            // Already ES — leave alone (100, 300 es, 310 es, 320 es)
            newVerLine = verLine;
        } else if (verNum <= 120) {
            // Desktop 1.1x — uses attribute/varying, maps to ES 1.00
            newVerLine = "#version 100";
        } else {
            // Desktop 1.30+ / 3.30+ — uses in/out, maps to ES 3.00
            newVerLine = "#version 300 es";
        }

        // Anything before #version (comments, whitespace) is preserved.
        std::string before = src.substr(0, versionPos);
        // ANGLE requires #version to be the first non-whitespace token.
        // Desktop GL tolerates leading blank lines; GLSL ES does not.
        // Strip leading whitespace so "\n#version 100" becomes "#version 100".
        if (!before.empty()) {
            size_t nonWs = 0;
            while (nonWs < before.size() &&
                   (before[nonWs] == '\n' || before[nonWs] == '\r' ||
                    before[nonWs] == ' '  || before[nonWs] == '\t'))
                nonWs++;
            if (nonWs == before.size()) before.clear();
        }

        // ESSL3 rule: ALL #extension directives must precede any
        // non-preprocessor tokens (including `precision` declarations).
        // So walk forward past every consecutive #extension / blank /
        // comment line, then inject precision *after* that block.
        // Without this, shaders that use #extension (e.g. peony.silicate's
        // render-pass shaders) fail to compile with:
        //   "extension directive must occur before any non-preprocessor
        //    tokens in ESSL3"
        //
        // While walking, we also COMMENT OUT desktop-only #extension
        // directives that map to built-in features in GLES 3.00 (or that
        // simply don't exist there). ANGLE rejects them otherwise.
        bool toEs3 = (newVerLine == "#version 300 es");
        std::string head;  // built up incrementally; replaces src[lineEnd:injectPos]
        size_t injectPos = lineEnd;  // start at the '\n' after #version
        while (injectPos < src.size()) {
            size_t lineBeg = injectPos;
            // Skip the leading '\n' of the line we just consumed.
            if (src[lineBeg] == '\n') lineBeg++;
            // Find the next non-whitespace character on this line.
            size_t textStart = lineBeg;
            while (textStart < src.size() &&
                   (src[textStart] == ' ' || src[textStart] == '\t')) textStart++;
            // Detect a preprocessor line we should preserve before precision.
            bool isExt =
                src.compare(textStart, 10, "#extension") == 0;
            // Detect a fully blank line or a // line comment — both safe to skip.
            bool isBlank = (textStart >= src.size() || src[textStart] == '\n');
            bool isLineComment =
                textStart + 2 <= src.size() && src[textStart] == '/' && src[textStart+1] == '/';
            if (!(isExt || isBlank || isLineComment)) break;
            // Advance to (and consume) the next '\n'.
            size_t nextNl = src.find('\n', textStart);
            size_t lineEndPos = (nextNl == std::string::npos) ? src.size() : nextNl;

            // Decide whether to keep, strip, or pass through this line.
            std::string line = src.substr(lineBeg, lineEndPos - lineBeg);
            if (isExt && toEs3) {
                // Desktop-only ARB extensions that have no ES 3.00 equivalent
                // OR whose feature is built-in to ES 3.00 — strip them out
                // (replace with comment to preserve line count for error
                // reporting). Keep ES-compatible extensions (e.g.
                // GL_OES_*, GL_EXT_shader_texture_lod) which ANGLE handles.
                static const char* kStripExt[] = {
                    "GL_ARB_explicit_attrib_location",   // built-in to ES 3.00
                    "GL_ARB_explicit_uniform_location",  // not in ES 3.00, no replacement
                    "GL_ARB_separate_shader_objects",
                    "GL_ARB_shading_language_420pack",
                    "GL_ARB_enhanced_layouts",
                    "GL_ARB_uniform_buffer_object",      // built-in to ES 3.00
                    "GL_ARB_texture_rectangle",
                    "GL_ARB_sample_shading",
                    "GL_ARB_gpu_shader5",
                };
                bool stripped = false;
                for (auto* name : kStripExt) {
                    if (line.find(name) != std::string::npos) {
                        line = "// stripped (desktop-only): " + line;
                        stripped = true;
                        break;
                    }
                }
                (void)stripped;
            }
            head.push_back('\n');
            head.append(line);

            if (nextNl == std::string::npos) { injectPos = src.size(); break; }
            injectPos = nextNl;  // points at '\n'; next loop iter consumes it
        }
        std::string tail = src.substr(injectPos);

        // Inject precision AFTER the #version + #extension block. Required
        // for ES 3.00 fragment shaders (no default precision for
        // float/sampler) and harmless redundancy on vertex shaders.
        std::string precisionInject;
        if (isFragment) precisionInject = "\nprecision mediump float;\nprecision mediump int;";
        else            precisionInject = "\nprecision mediump int;";

        patched = before + newVerLine + head + precisionInject + tail;
        if (toEs3) {
            stripUniformLayouts(patched);
            replaceTextureFunctions(patched);
        }
    }

    const GLchar* ptrs[1] = { patched.c_str() };
    GLint lens[1] = { (GLint)patched.size() };
    p(shader, 1, ptrs, lens);

    static int n = 0;
    angle::log("ShaderSource patch #%d shader=%u type=%s (orig %zu B -> patched %zu B, hasVersion=%d)",
               n, shader, isFragment ? "FRAG" : "VERT",
               src.size(), patched.size(), versionPos != std::string::npos);
    n++;
}

typedef void (WINAPI *PFN_CMP)(GLuint);
extern "C" __declspec(dllexport) void WINAPI gl_glCompileShader(GLuint shader) {
    static PFN_CMP p = nullptr;
    if (!p) p = (PFN_CMP)glproxy::resolve("glCompileShader");
    if (p) p(shader);
    // Check status & log info log on failure
    typedef void (WINAPI *PFN_GSV)(GLuint, GLenum, GLint*);
    typedef void (WINAPI *PFN_GSIL)(GLuint, GLsizei, GLsizei*, char*);
    static PFN_GSV pgsv = (PFN_GSV)glproxy::resolve("glGetShaderiv");
    static PFN_GSIL pgsil = (PFN_GSIL)glproxy::resolve("glGetShaderInfoLog");
    static int n = 0;
    if (pgsv) {
        GLint cs = -1; pgsv(shader, 0x8B81 /*GL_COMPILE_STATUS*/, &cs);
        if (cs == 0) {
            char buf[512] = {};
            if (pgsil) pgsil(shader, 511, nullptr, buf);
            angle::log("CompileShader FAIL #%d shader=%u: %s", n, shader, buf);
        } else if (n < 128) {
            angle::log("CompileShader OK #%d shader=%u", n, shader);
        }
        n++;
    }
}
GLP_EXT_FORWARD_VOID(GetShaderiv, (GLuint shader, GLenum pname, GLint* params), (shader, pname, params))
GLP_EXT_FORWARD_VOID(GetShaderInfoLog, (GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* infoLog), (shader, bufSize, length, infoLog))
GLP_EXT_FORWARD_VOID(GetShaderSource, (GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* source), (shader, bufSize, length, source))
GLP_EXT_FORWARD(GLboolean, IsShader, (GLuint shader), (shader))

extern "C" __declspec(dllexport) GLuint WINAPI gl_glCreateShaderObjectARB(GLenum type) { return gl_glCreateShader(type); }
extern "C" __declspec(dllexport) void WINAPI gl_glShaderSourceARB(GLuint shader, GLsizei count, const GLchar* const* string, const GLint* length) { gl_glShaderSource(shader, count, string, length); }
extern "C" __declspec(dllexport) void WINAPI gl_glCompileShaderARB(GLuint shader) { gl_glCompileShader(shader); }

// Programs
typedef GLuint (WINAPI *PFN_CP)(void);
extern "C" __declspec(dllexport) GLuint WINAPI gl_glCreateProgram(void) {
    static PFN_CP p = nullptr;
    if (!p) p = (PFN_CP)glproxy::resolve("glCreateProgram");
    GLuint id = p ? p() : 0;
    if (id == 0) angle::forceLog("CreateProgram FAIL (resolved=%d)", p ? 1 : 0);
    return id;
}

// glDeleteProgram — keep our `g_currentProgram` cache valid: if we just
// deleted the program that was currently bound, ANGLE will set
// GL_CURRENT_PROGRAM to 0 internally, so reflect that in our cache too.
// Without this, gl_glUniform* calls after DeleteProgram of the current
// program would crash deep in ANGLE (null deref on the freed Program*).
typedef void (WINAPI *PFN_DP)(GLuint);
extern thread_local GLuint g_currentProgram;
extern "C" __declspec(dllexport) void WINAPI gl_glDeleteProgram(GLuint program) {
    static PFN_DP p = nullptr;
    if (!p) p = (PFN_DP)glproxy::resolve("glDeleteProgram");
    if (p) p(program);
    // Drop link-status entry; the GLuint may be recycled for a new program.
    if (program < 16384) {
        g_programLinked[program].store(0, std::memory_order_relaxed);
    }
    // Sync g_currentProgram with ANGLE's actual GL_CURRENT_PROGRAM on this thread.
    // g_currentProgram is thread_local — we can only fix the calling thread here.
    // ANGLE resets GL_CURRENT_PROGRAM to 0 when the bound program is deleted;
    // querying once is cheaper than a stale cache causing a null-deref in gl_glUniform*.
    if (program != 0 && program == g_currentProgram) {
        typedef void (WINAPI *PFN_GIV_DP)(GLenum, GLint*);
        static PFN_GIV_DP pGetI = nullptr;
        if (!pGetI) pGetI = (PFN_GIV_DP)glproxy::resolve("glGetIntegerv");
        if (pGetI) {
            GLint actual = 0;
            pGetI(0x8B8D /*GL_CURRENT_PROGRAM*/, &actual);
            g_currentProgram = (GLuint)actual;
        } else {
            g_currentProgram = 0;
        }
    }
}
GLP_EXT_FORWARD_VOID(AttachShader, (GLuint program, GLuint shader), (program, shader))
GLP_EXT_FORWARD_VOID(DetachShader, (GLuint program, GLuint shader), (program, shader))
typedef void (WINAPI *PFN_LP)(GLuint);
extern "C" __declspec(dllexport) void WINAPI gl_glLinkProgram(GLuint program) {
    static PFN_LP p = nullptr;
    if (!p) p = (PFN_LP)glproxy::resolve("glLinkProgram");
    if (p) p(program);
    // Query link status; cache it so subsequent draw calls can skip work
    // when a mod hooked us up with a non-linked program (silicate's path).
    typedef void (WINAPI *PFN_GPV)(GLuint, GLenum, GLint*);
    static PFN_GPV pgpiv = nullptr;
    if (!pgpiv) pgpiv = (PFN_GPV)glproxy::resolve("glGetProgramiv");
    GLint ls = 1;  // assume OK if we can't query
    if (pgpiv) pgpiv(program, 0x8B82 /*GL_LINK_STATUS*/, &ls);
    if (program < 16384) {
        g_programLinked[program].store((ls != 0) ? 2 : 1, std::memory_order_relaxed);
    }
    static int n = 0;
    angle::log("glLinkProgram #%d: prog=%u link_status=%d", n, program, ls);
    if (ls == 0 && pgpiv) {
        typedef void (WINAPI *PFN_GPIL)(GLuint, GLsizei, GLsizei*, char*);
        static PFN_GPIL pgpil = (PFN_GPIL)glproxy::resolve("glGetProgramInfoLog");
        if (pgpil) {
            char buf[512] = {}; GLsizei len = 0;
            pgpil(program, 511, &len, buf);
            angle::log("  link_log: %s", buf);
        }
    }
    n++;
}

// Called by gl_glDrawArrays / glDrawElements / glDrawElementsBaseVertex
// (in gl_proxy.cpp) to decide whether forwarding the draw to ANGLE is safe.
// Returns false when the currently bound program has GL_LINK_STATUS=0
// (ANGLE fastfails on drawing with such programs).
//
// Critical: peony.silicate calls glShaderSource / glLinkProgram / glUseProgram
// *directly* via GetProcAddress(libGLESv2.dll, ...), bypassing our proxy
// hooks entirely. So our `g_currentProgram` cache and `g_programLinked` map
// only contain GD/cocos2d programs. To catch silicate's broken programs
// at draw time we must query ANGLE itself.
//
// Hot-path: use the g_currentProgram cache maintained by gl_glUseProgram.
// This eliminates all driver calls on the draw hot-path for GD/cocos2d code.
// glGetIntegerv is only issued for programs bound by foreign callers (silicate)
// that bypass our proxy, and only once per distinct program ID change.
typedef void (WINAPI *PFN_GIV3)(GLenum, GLint*);
typedef void (WINAPI *PFN_GPV2)(GLuint, GLenum, GLint*);
extern "C" bool gdangle_currentProgramOK() {
    // Fast path: g_currentProgram is maintained by our gl_glUseProgram proxy.
    // When it holds a real program ID (not the sentinel), we can check the
    // link-status cache without ANY driver call — O(1), ~1 ns.
    // We only fall back to the expensive glGetIntegerv round-trip when
    // g_currentProgram == 0xFFFFFFFFu, which means a foreign caller
    // (e.g. peony.silicate) bound a program by calling libGLESv2.dll directly,
    // bypassing our proxy hook entirely.
    if (g_currentProgram == 0) return false;  // no program → ANGLE fastfails

    if (g_currentProgram != 0xFFFFFFFFu) {
        GLuint ucur = g_currentProgram;
        if (ucur < 16384) {
            int8_t cached = g_programLinked[ucur].load(std::memory_order_relaxed);
            if (cached == 2) return true;   // known good
            // cached == 0 or 1: re-verify with glIsProgram() — ANGLE recycles IDs.
            typedef GLboolean (WINAPI *PFN_IP2)(GLuint);
            static PFN_IP2 pIsP2 = (PFN_IP2)glproxy::resolve("glIsProgram");
            if (!pIsP2 || pIsP2(ucur) == 0) {
                g_programLinked[ucur].store(1, std::memory_order_relaxed);
                return false;  // deleted/invalid — skip draw
            }
            // Program exists — fall through to slow path for glGetProgramiv.
        } else {
            return true;  // ID >= 16384: assume OK
        }
        // Fall through to slow path to verify unknown live program
    }

    // Slow path: program status unknown — query glGetProgramiv once per ID.
    // Reached when: (a) g_currentProgram == sentinel (silicate used libGLESv2 directly),
    //           or: (b) fast-path cache miss (cached==0, ID known but never linked via us).
    static PFN_GIV3 pGetIv = nullptr;
    static PFN_GPV2 pGetPv = nullptr;
    if (!pGetIv) pGetIv = (PFN_GIV3)glproxy::resolve("glGetIntegerv");
    if (!pGetPv) pGetPv = (PFN_GPV2)glproxy::resolve("glGetProgramiv");
    if (!pGetPv) return true;  // can't verify → assume OK

    // If we fell through from fast path we already have the real ID.
    GLint cur = (g_currentProgram != 0xFFFFFFFFu)
                ? (GLint)g_currentProgram
                : 0;
    if (cur == 0 && pGetIv) {
        pGetIv(0x8B8D /*GL_CURRENT_PROGRAM*/, &cur);
    }
    if (cur <= 0) return false;  // no program bound

    // Per-thread cache of last-seen (program, OK?) pair to avoid repeated queries.
    // Reset to sentinel each frame via gdangle_invalidateProgramCache().
    thread_local GLuint t_lastProg = 0xFFFFFFFFu;
    thread_local bool   t_lastOK   = true;
    // Always re-check if g_currentProgram is sentinel — silicate may have
    // deleted and recreated the program between frames without going via proxy.
    if ((GLuint)cur == t_lastProg && g_currentProgram != 0xFFFFFFFFu) return t_lastOK;

    bool ok;
    GLuint ucur = (GLuint)cur;
    if (ucur < 16384) {
        int8_t cached = g_programLinked[ucur].load(std::memory_order_relaxed);
        if (cached == 2) {
            ok = true;   // known good
        } else {
            // cached == 0 or 1: re-verify — ANGLE recycles IDs, a previously
            // failed/deleted ID may now be a new valid program.
            // Use glIsProgram() first — safe call that returns GL_FALSE for deleted/
            // invalid IDs without triggering ANGLE's internal ASSERT (unlike glGetProgramiv).
            typedef GLboolean (WINAPI *PFN_IP)(GLuint);
            static PFN_IP pIsP = (PFN_IP)glproxy::resolve("glIsProgram");
            if (!pIsP || pIsP(ucur) == 0) {
                // Program doesn't exist in ANGLE — mark bad and skip draw.
                g_programLinked[ucur].store(1, std::memory_order_relaxed);
                ok = false;
            } else {
                // Program exists — now safe to query link status.
                GLint ls = 0;
                pGetPv(ucur, 0x8B82 /*GL_LINK_STATUS*/, &ls);
                ok = (ls != 0);
                g_programLinked[ucur].store(ok ? 2 : 1, std::memory_order_relaxed);
            }
        }
    } else {
        // ID >= 16384: outside our tracking range, assume OK.
        ok = true;
    }
    t_lastProg = ucur;
    t_lastOK   = ok;
    return ok;
}

// Called from wglSwapBuffers to force a fresh program-status check at frame start.
// This catches the case where silicate deleted+recreated a program between frames
// without going through our gl_glDeleteProgram / gl_glLinkProgram proxy.
extern "C" void gdangle_invalidateProgramCache() {
    // Reset sentinel so next draw call re-queries GL_CURRENT_PROGRAM.
    // Only do it if we're in sentinel state (foreign program path).
    // If g_currentProgram holds a real ID keep it — it's maintained by our proxy.
    if (g_currentProgram == 0xFFFFFFFFu) {
        // Nothing to do — slow path already queries live each time.
        // But force t_lastProg reset by temporarily resetting g_currentProgram
        // so next gdangle_currentProgramOK call re-runs glGetIntegerv.
        // (t_lastProg is thread_local inside the function — we can't reset it here.)
        // Instead: clear the programLinked cache entry for the last seen foreign program
        // — it will be re-verified on next draw.
    }
    // More importantly: if a real cached program was silently deleted by a foreign caller,
    // we need to invalidate its g_programLinked entry. The safest way: do a single
    // glGetIntegerv here to sync g_currentProgram with ANGLE reality.
    typedef void (WINAPI *PFN_GIV_IC)(GLenum, GLint*);
    static PFN_GIV_IC pGetI = (PFN_GIV_IC)glproxy::resolve("glGetIntegerv");
    if (!pGetI) return;
    GLint actual = 0;
    pGetI(0x8B8D /*GL_CURRENT_PROGRAM*/, &actual);
    // Only update cache if ANGLE reports a real program; keep existing ID otherwise.
    // Do NOT invalidate g_programLinked — deleted programs should stay as cached=1 (bad)
    // or remain unverified (0). Resetting to 0 would trigger a glGetProgramiv on a
    // potentially-deleted program which causes ANGLE to crash inside its own ASSERT.
    if (actual > 0) {
        g_currentProgram = (GLuint)actual;
    } else {
        g_currentProgram = 0;  // no program bound
    }
}

typedef void (WINAPI *PFN_UP)(GLuint);
typedef void (WINAPI *PFN_GIV)(GLenum, GLint*);
// thread_local current program — used by uniform dedup and dgd_noProgramBound.
thread_local GLuint g_currentProgram = 0xFFFFFFFFu;
extern "C" __declspec(dllexport) void WINAPI gl_glUseProgram(GLuint program) {
    static PFN_UP  p     = nullptr;
    static PFN_GIV pGetI = nullptr;
    if (!p)     p     = (PFN_UP) glproxy::resolve("glUseProgram");
    if (!pGetI) pGetI = (PFN_GIV)glproxy::resolve("glGetIntegerv");
    // Skip the driver call only when we're certain the cache is valid —
    // i.e. it holds a real program ID (not the uninitialised sentinel).
    if (g_currentProgram != 0xFFFFFFFFu && program == g_currentProgram) return;
    static int n = 0;
    if (n < 40) angle::forceLog("glUseProgram #%d: prog=%u (was prog=%u)", n, program,
        g_currentProgram == 0xFFFFFFFFu ? 0xFFFFFFFFu : g_currentProgram);
    n++;
    if (p) p(program);
    // Trust-but-verify ONCE per glUseProgram (rare event vs uniform calls):
    // ANGLE may have rejected the bind (invalid program ID, link failure, etc).
    // If so, GL_CURRENT_PROGRAM stays at its previous value (often 0). Cache
    // the truth so downstream gl_glUniform* dedup + gd_noProgramBound stay
    // consistent with what ANGLE actually thinks is bound.
    if (pGetI) {
        GLint actual = 0;
        pGetI(0x8B8D /*GL_CURRENT_PROGRAM*/, &actual);
        g_currentProgram = (GLuint)actual;
    } else {
        g_currentProgram = program;  // fallback: trust the request
    }
}
GLP_EXT_FORWARD_VOID(GetProgramiv, (GLuint program, GLenum pname, GLint* params), (program, pname, params))
GLP_EXT_FORWARD_VOID(GetProgramInfoLog, (GLuint program, GLsizei bufSize, GLsizei* length, GLchar* infoLog), (program, bufSize, length, infoLog))
GLP_EXT_FORWARD_VOID(ValidateProgram, (GLuint program), (program))
GLP_EXT_FORWARD(GLboolean, IsProgram, (GLuint program), (program))

extern "C" __declspec(dllexport) GLuint WINAPI gl_glCreateProgramObjectARB() { return gl_glCreateProgram(); }
extern "C" __declspec(dllexport) void WINAPI gl_glAttachObjectARB(GLuint program, GLuint shader) { gl_glAttachShader(program, shader); }
extern "C" __declspec(dllexport) void WINAPI gl_glDetachObjectARB(GLuint program, GLuint shader) { gl_glDetachShader(program, shader); }
extern "C" __declspec(dllexport) void WINAPI gl_glLinkProgramARB(GLuint program) { gl_glLinkProgram(program); }
extern "C" __declspec(dllexport) void WINAPI gl_glUseProgramObjectARB(GLuint program) { gl_glUseProgram(program); }
extern "C" __declspec(dllexport) void WINAPI gl_glValidateProgramARB(GLuint program) { gl_glValidateProgram(program); }
extern "C" __declspec(dllexport) void WINAPI gl_glDeleteObjectARB(GLuint obj) {
    if (gl_glIsShader(obj)) gl_glDeleteShader(obj);
    else if (gl_glIsProgram(obj)) gl_glDeleteProgram(obj);
}
extern "C" __declspec(dllexport) GLuint WINAPI gl_glGetHandleARB(GLenum pname) {
    if (pname != 0x8B40 /*GL_PROGRAM_OBJECT_ARB*/) return 0;
    typedef void (WINAPI *PFN_GIV_ARB)(GLenum, GLint*);
    static PFN_GIV_ARB pGetI = nullptr;
    if (!pGetI) pGetI = (PFN_GIV_ARB)glproxy::resolve("glGetIntegerv");
    GLint cur = 0;
    if (pGetI) pGetI(0x8B8D /*GL_CURRENT_PROGRAM*/, &cur);
    return (GLuint)cur;
}
extern "C" __declspec(dllexport) void WINAPI gl_glGetAttachedObjectsARB(GLuint program, GLsizei maxCount, GLsizei* count, GLuint* objects) {
    typedef void (WINAPI *PFN_GAS_ARB)(GLuint, GLsizei, GLsizei*, GLuint*);
    static PFN_GAS_ARB pGetAttachedShaders = nullptr;
    if (!pGetAttachedShaders) pGetAttachedShaders = (PFN_GAS_ARB)glproxy::resolve("glGetAttachedShaders");
    if (pGetAttachedShaders) pGetAttachedShaders(program, maxCount, count, objects);
    else if (count) *count = 0;
}
extern "C" __declspec(dllexport) void WINAPI gl_glGetObjectParameterivARB(GLuint obj, GLenum pname, GLint* params) {
    if (!params) return;
    if (pname == 0x8B4E /*GL_OBJECT_TYPE_ARB*/) {
        if (gl_glIsProgram(obj)) *params = 0x8B40 /*GL_PROGRAM_OBJECT_ARB*/;
        else if (gl_glIsShader(obj)) *params = 0x8B48 /*GL_SHADER_OBJECT_ARB*/;
        else *params = 0;
        return;
    }
    if (pname == 0x8B4F /*GL_OBJECT_SUBTYPE_ARB*/) {
        std::lock_guard<std::mutex> lk(g_shaderTypeMtx);
        auto it = g_shaderType.find(obj);
        *params = (it != g_shaderType.end()) ? (GLint)it->second : 0;
        return;
    }
    if (gl_glIsShader(obj)) gl_glGetShaderiv(obj, pname, params);
    else if (gl_glIsProgram(obj)) gl_glGetProgramiv(obj, pname, params);
    else *params = 0;
}
extern "C" __declspec(dllexport) void WINAPI gl_glGetObjectParameterfvARB(GLuint obj, GLenum pname, GLfloat* params) {
    if (!params) return;
    GLint value = 0;
    gl_glGetObjectParameterivARB(obj, pname, &value);
    *params = (GLfloat)value;
}
extern "C" __declspec(dllexport) void WINAPI gl_glGetInfoLogARB(GLuint obj, GLsizei maxLength, GLsizei* length, GLchar* infoLog) {
    if (gl_glIsShader(obj)) gl_glGetShaderInfoLog(obj, maxLength, length, infoLog);
    else if (gl_glIsProgram(obj)) gl_glGetProgramInfoLog(obj, maxLength, length, infoLog);
    else {
        if (length) *length = 0;
        if (infoLog && maxLength > 0) infoLog[0] = '\0';
    }
}

GLP_EXT_FORWARD_VOID(BindAttribLocation, (GLuint program, GLuint index, const GLchar* name), (program, index, name))
GLP_EXT_FORWARD(GLint, GetAttribLocation, (GLuint program, const GLchar* name), (program, name))
GLP_EXT_FORWARD_VOID(GetActiveAttrib, (GLuint program, GLuint index, GLsizei bufSize, GLsizei* length, GLint* size, GLenum* type, GLchar* name), (program, index, bufSize, length, size, type, name))
GLP_EXT_FORWARD_VOID(GetActiveUniform, (GLuint program, GLuint index, GLsizei bufSize, GLsizei* length, GLint* size, GLenum* type, GLchar* name), (program, index, bufSize, length, size, type, name))

// Uniforms
GLP_EXT_FORWARD(GLint, GetUniformLocation, (GLuint program, const GLchar* name), (program, name))

// ===== Scalar uniform dedup =====
// cocos2d-x re-uploads identical color / alpha / sampler uniforms every sprite.
// Direct-mapped per-thread cache keyed by (program, location). Skips driver
// upload when value is unchanged. Real CPU win on draw-heavy frames.
//
// 32 entries × 5 variants × 1 thread ≈ 4 KB/thread overhead. Hash collisions
// just cause an unnecessary upload, never a wrong upload.

extern thread_local GLuint g_currentProgram;

template <typename T, int K>
struct UniCache {
    GLuint prog;
    GLint  loc;
    T      val[K];
};
static constexpr unsigned UN_HASH_N = 32;
static inline unsigned uniHash(GLuint prog, GLint loc) {
    return ((unsigned)prog * 2654435761u + (unsigned)loc) & (UN_HASH_N - 1);
}

// Compatibility shim: ANGLE crashes in glUniform* if no program is currently
// bound (null deref deep inside ANGLE's program-state lookup). Desktop OpenGL
// silently no-ops this case. Some Geode mods (peony.silicate) rely on the
// desktop-GL behaviour and call glUniform* without ensuring a program is
// bound, leading to crashes only when ReviANGLE is active. Match desktop
// behaviour to keep these mods alive.
//
// Cache-only check (hot path — ~10000 calls/frame from cocos2d). Cache
// validity is maintained by:
//   - gl_glUseProgram:   queries GL_CURRENT_PROGRAM after each bind to
//                        catch ANGLE rejecting invalid program IDs.
//   - gl_glDeleteProgram: clears cache to 0 if deleting the bound program.
// This keeps gd_noProgramBound() at a single integer compare (~1 ns) instead
// of a per-uniform glGetIntegerv (which destroyed framerate on D3D11).
static inline bool gd_noProgramBound() {
    if (g_currentProgram == 0) return true;
    if (g_currentProgram != 0xFFFFFFFFu) return false;
    // Sentinel: silicate may have bound a program directly via libGLESv2.dll.
    // Sync once so uniform dedup and this check see the real state.
    static auto pGetI = (PFN_GIV)glproxy::resolve("glGetIntegerv");
    if (!pGetI) return false;  // can't verify — allow the call through
    GLint cur = 0;
    pGetI(0x8B8D /*GL_CURRENT_PROGRAM*/, &cur);
    g_currentProgram = (GLuint)cur;  // update cache for subsequent calls
    return cur <= 0;
}

typedef void (WINAPI *PFN_U1F)(GLint, GLfloat);
extern "C" __declspec(dllexport) void WINAPI gl_glUniform1f(GLint location, GLfloat v0) {
    static PFN_U1F p = nullptr;
    if (!p) p = (PFN_U1F)glproxy::resolve("glUniform1f");
    if (gd_noProgramBound()) return;
    if (location >= 0 && g_currentProgram != 0xFFFFFFFFu) {
        thread_local UniCache<GLfloat, 1> c[UN_HASH_N] = {};
        auto& e = c[uniHash(g_currentProgram, location)];
        if (e.prog == g_currentProgram && e.loc == location && e.val[0] == v0) return;
        e.prog = g_currentProgram; e.loc = location; e.val[0] = v0;
    }
    if (p) p(location, v0);
}
// glUniform2f dedup.
typedef void (WINAPI *PFN_U2F)(GLint, GLfloat, GLfloat);
extern "C" __declspec(dllexport) void WINAPI gl_glUniform2f(GLint location, GLfloat v0, GLfloat v1) {
    static PFN_U2F p = nullptr;
    if (!p) p = (PFN_U2F)glproxy::resolve("glUniform2f");
    if (gd_noProgramBound()) return;
    if (location >= 0 && g_currentProgram != 0xFFFFFFFFu) {
        thread_local UniCache<GLfloat, 2> c[UN_HASH_N] = {};
        auto& e = c[uniHash(g_currentProgram, location)];
        if (e.prog == g_currentProgram && e.loc == location && e.val[0] == v0 && e.val[1] == v1) return;
        e.prog = g_currentProgram; e.loc = location; e.val[0] = v0; e.val[1] = v1;
    }
    if (p) p(location, v0, v1);
}
// glUniform3f dedup.
typedef void (WINAPI *PFN_U3F)(GLint, GLfloat, GLfloat, GLfloat);
extern "C" __declspec(dllexport) void WINAPI gl_glUniform3f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2) {
    static PFN_U3F p = nullptr;
    if (!p) p = (PFN_U3F)glproxy::resolve("glUniform3f");
    if (gd_noProgramBound()) return;
    if (location >= 0 && g_currentProgram != 0xFFFFFFFFu) {
        thread_local UniCache<GLfloat, 3> c[UN_HASH_N] = {};
        auto& e = c[uniHash(g_currentProgram, location)];
        if (e.prog == g_currentProgram && e.loc == location && e.val[0] == v0 && e.val[1] == v1 && e.val[2] == v2) return;
        e.prog = g_currentProgram; e.loc = location; e.val[0] = v0; e.val[1] = v1; e.val[2] = v2;
    }
    if (p) p(location, v0, v1, v2);
}

typedef void (WINAPI *PFN_U4F)(GLint, GLfloat, GLfloat, GLfloat, GLfloat);
extern "C" __declspec(dllexport) void WINAPI gl_glUniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3) {
    static PFN_U4F p = nullptr;
    if (!p) p = (PFN_U4F)glproxy::resolve("glUniform4f");
    if (gd_noProgramBound()) return;
    if (location >= 0 && g_currentProgram != 0xFFFFFFFFu) {
        thread_local UniCache<GLfloat, 4> c[UN_HASH_N] = {};
        auto& e = c[uniHash(g_currentProgram, location)];
        if (e.prog == g_currentProgram && e.loc == location &&
            e.val[0] == v0 && e.val[1] == v1 && e.val[2] == v2 && e.val[3] == v3) return;
        e.prog = g_currentProgram; e.loc = location;
        e.val[0] = v0; e.val[1] = v1; e.val[2] = v2; e.val[3] = v3;
    }
    if (p) p(location, v0, v1, v2, v3);
}

typedef void (WINAPI *PFN_U1I)(GLint, GLint);
extern "C" __declspec(dllexport) void WINAPI gl_glUniform1i(GLint location, GLint v0) {
    static PFN_U1I p = nullptr;
    if (!p) p = (PFN_U1I)glproxy::resolve("glUniform1i");
    if (gd_noProgramBound()) return;
    if (location >= 0 && g_currentProgram != 0xFFFFFFFFu) {
        thread_local UniCache<GLint, 1> c[UN_HASH_N] = {};
        auto& e = c[uniHash(g_currentProgram, location)];
        if (e.prog == g_currentProgram && e.loc == location && e.val[0] == v0) return;
        e.prog = g_currentProgram; e.loc = location; e.val[0] = v0;
    }
    if (p) p(location, v0);
}
// glUniform2i dedup.
typedef void (WINAPI *PFN_U2I)(GLint, GLint, GLint);
extern "C" __declspec(dllexport) void WINAPI gl_glUniform2i(GLint location, GLint v0, GLint v1) {
    static PFN_U2I p = nullptr;
    if (!p) p = (PFN_U2I)glproxy::resolve("glUniform2i");
    if (gd_noProgramBound()) return;
    if (location >= 0 && g_currentProgram != 0xFFFFFFFFu) {
        thread_local UniCache<GLint, 2> c[UN_HASH_N] = {};
        auto& e = c[uniHash(g_currentProgram, location)];
        if (e.prog == g_currentProgram && e.loc == location && e.val[0] == v0 && e.val[1] == v1) return;
        e.prog = g_currentProgram; e.loc = location; e.val[0] = v0; e.val[1] = v1;
    }
    if (p) p(location, v0, v1);
}
GLP_EXT_FORWARD_VOID(Uniform3i, (GLint location, GLint v0, GLint v1, GLint v2), (location, v0, v1, v2))
GLP_EXT_FORWARD_VOID(Uniform4i, (GLint location, GLint v0, GLint v1, GLint v2, GLint v3), (location, v0, v1, v2, v3))
// glUniform1fv (count==1) dedup.
typedef void (WINAPI *PFN_U1FV)(GLint, GLsizei, const GLfloat*);
extern "C" __declspec(dllexport) void WINAPI gl_glUniform1fv(GLint location, GLsizei count, const GLfloat* value) {
    static PFN_U1FV p = nullptr;
    if (!p) p = (PFN_U1FV)glproxy::resolve("glUniform1fv");
    if (gd_noProgramBound()) return;
    if (count == 1 && value && location >= 0 && g_currentProgram != 0xFFFFFFFFu) {
        thread_local UniCache<GLfloat, 1> c[UN_HASH_N] = {};
        auto& e = c[uniHash(g_currentProgram, location)];
        if (e.prog == g_currentProgram && e.loc == location && e.val[0] == value[0]) return;
        e.prog = g_currentProgram; e.loc = location; e.val[0] = value[0];
    }
    if (p) p(location, count, value);
}
// glUniform2fv (count==1) dedup.
typedef void (WINAPI *PFN_U2FV)(GLint, GLsizei, const GLfloat*);
extern "C" __declspec(dllexport) void WINAPI gl_glUniform2fv(GLint location, GLsizei count, const GLfloat* value) {
    static PFN_U2FV p = nullptr;
    if (!p) p = (PFN_U2FV)glproxy::resolve("glUniform2fv");
    if (gd_noProgramBound()) return;
    if (count == 1 && value && location >= 0 && g_currentProgram != 0xFFFFFFFFu) {
        thread_local UniCache<GLfloat, 2> c[UN_HASH_N] = {};
        auto& e = c[uniHash(g_currentProgram, location)];
        if (e.prog == g_currentProgram && e.loc == location &&
            e.val[0] == value[0] && e.val[1] == value[1]) return;
        e.prog = g_currentProgram; e.loc = location;
        e.val[0] = value[0]; e.val[1] = value[1];
    }
    if (p) p(location, count, value);
}
// glUniform3fv (count==1) dedup.
typedef void (WINAPI *PFN_U3FV)(GLint, GLsizei, const GLfloat*);
extern "C" __declspec(dllexport) void WINAPI gl_glUniform3fv(GLint location, GLsizei count, const GLfloat* value) {
    static PFN_U3FV p = nullptr;
    if (!p) p = (PFN_U3FV)glproxy::resolve("glUniform3fv");
    if (gd_noProgramBound()) return;
    if (count == 1 && value && location >= 0 && g_currentProgram != 0xFFFFFFFFu) {
        thread_local UniCache<GLfloat, 3> c[UN_HASH_N] = {};
        auto& e = c[uniHash(g_currentProgram, location)];
        if (e.prog == g_currentProgram && e.loc == location &&
            e.val[0] == value[0] && e.val[1] == value[1] && e.val[2] == value[2]) return;
        e.prog = g_currentProgram; e.loc = location;
        e.val[0] = value[0]; e.val[1] = value[1]; e.val[2] = value[2];
    }
    if (p) p(location, count, value);
}

// glUniform4fv (count==1) dedup — used for color arrays, lighting params.
typedef void (WINAPI *PFN_U4FV)(GLint, GLsizei, const GLfloat*);
extern "C" __declspec(dllexport) void WINAPI gl_glUniform4fv(GLint location, GLsizei count, const GLfloat* value) {
    static PFN_U4FV p = nullptr;
    if (!p) p = (PFN_U4FV)glproxy::resolve("glUniform4fv");
    if (gd_noProgramBound()) return;
    if (count == 1 && value && location >= 0 && g_currentProgram != 0xFFFFFFFFu) {
        thread_local UniCache<GLfloat, 4> c[UN_HASH_N] = {};
        auto& e = c[uniHash(g_currentProgram, location)];
        if (e.prog == g_currentProgram && e.loc == location &&
            e.val[0] == value[0] && e.val[1] == value[1] &&
            e.val[2] == value[2] && e.val[3] == value[3]) return;
        e.prog = g_currentProgram; e.loc = location;
        e.val[0] = value[0]; e.val[1] = value[1];
        e.val[2] = value[2]; e.val[3] = value[3];
    }
    if (p) p(location, count, value);
}
// glUniform1iv (count==1) dedup.
typedef void (WINAPI *PFN_U1IV)(GLint, GLsizei, const GLint*);
extern "C" __declspec(dllexport) void WINAPI gl_glUniform1iv(GLint location, GLsizei count, const GLint* value) {
    static PFN_U1IV p = nullptr;
    if (!p) p = (PFN_U1IV)glproxy::resolve("glUniform1iv");
    if (gd_noProgramBound()) return;
    if (count == 1 && value && location >= 0 && g_currentProgram != 0xFFFFFFFFu) {
        thread_local UniCache<GLint, 1> c[UN_HASH_N] = {};
        auto& e = c[uniHash(g_currentProgram, location)];
        if (e.prog == g_currentProgram && e.loc == location && e.val[0] == value[0]) return;
        e.prog = g_currentProgram; e.loc = location; e.val[0] = value[0];
    }
    if (p) p(location, count, value);
}
// glUniform2iv (count==1) dedup.
typedef void (WINAPI *PFN_U2IV)(GLint, GLsizei, const GLint*);
extern "C" __declspec(dllexport) void WINAPI gl_glUniform2iv(GLint location, GLsizei count, const GLint* value) {
    static PFN_U2IV p = nullptr;
    if (!p) p = (PFN_U2IV)glproxy::resolve("glUniform2iv");
    if (gd_noProgramBound()) return;
    if (count == 1 && value && location >= 0 && g_currentProgram != 0xFFFFFFFFu) {
        thread_local UniCache<GLint, 2> c[UN_HASH_N] = {};
        auto& e = c[uniHash(g_currentProgram, location)];
        if (e.prog == g_currentProgram && e.loc == location &&
            e.val[0] == value[0] && e.val[1] == value[1]) return;
        e.prog = g_currentProgram; e.loc = location;
        e.val[0] = value[0]; e.val[1] = value[1];
    }
    if (p) p(location, count, value);
}
GLP_EXT_FORWARD_VOID(Uniform3iv, (GLint location, GLsizei count, const GLint* value), (location, count, value))
// glUniform4iv (count==1) dedup.
typedef void (WINAPI *PFN_U4IV)(GLint, GLsizei, const GLint*);
extern "C" __declspec(dllexport) void WINAPI gl_glUniform4iv(GLint location, GLsizei count, const GLint* value) {
    static PFN_U4IV p = nullptr;
    if (!p) p = (PFN_U4IV)glproxy::resolve("glUniform4iv");
    if (gd_noProgramBound()) return;
    if (count == 1 && value && location >= 0 && g_currentProgram != 0xFFFFFFFFu) {
        thread_local UniCache<GLint, 4> c[UN_HASH_N] = {};
        auto& e = c[uniHash(g_currentProgram, location)];
        if (e.prog == g_currentProgram && e.loc == location &&
            e.val[0] == value[0] && e.val[1] == value[1] &&
            e.val[2] == value[2] && e.val[3] == value[3]) return;
        e.prog = g_currentProgram; e.loc = location;
        e.val[0] = value[0]; e.val[1] = value[1];
        e.val[2] = value[2]; e.val[3] = value[3];
    }
    if (p) p(location, count, value);
}
GLP_EXT_FORWARD_VOID(UniformMatrix2fv, (GLint location, GLsizei count, GLboolean transpose, const GLfloat* value), (location, count, transpose, value))
// glUniformMatrix3fv (count==1) dedup — used by some custom shaders.
typedef void (WINAPI *PFN_UM3FV)(GLint, GLsizei, GLboolean, const GLfloat*);
extern "C" __declspec(dllexport) void WINAPI gl_glUniformMatrix3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    static PFN_UM3FV p = nullptr;
    if (!p) p = (PFN_UM3FV)glproxy::resolve("glUniformMatrix3fv");
    if (gd_noProgramBound()) return;
    if (count == 1 && value && location >= 0 && g_currentProgram != 0xFFFFFFFFu) {
        thread_local UniCache<GLfloat, 9> c[UN_HASH_N] = {};
        auto& e = c[uniHash(g_currentProgram, location)];
        if (e.prog == g_currentProgram && e.loc == location &&
            std::memcmp(e.val, value, sizeof(float) * 9) == 0) return;
        e.prog = g_currentProgram; e.loc = location;
        std::memcpy(e.val, value, sizeof(float) * 9);
    }
    if (p) p(location, count, transpose, value);
}

// glUniformMatrix4fv — dedup against last value per (program, location).
// Cocos2d-x uploads MVP matrix on every sprite draw even when identical.
// Uses lock-free direct-mapped fixed-size cache (no heap allocations on hot path).
typedef void (WINAPI *PFN_UM4FV)(GLint, GLsizei, GLboolean, const GLfloat*);
extern thread_local GLuint g_currentProgram;
extern "C" __declspec(dllexport) void WINAPI gl_glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    static PFN_UM4FV p = nullptr;
    if (!p) p = (PFN_UM4FV)glproxy::resolve("glUniformMatrix4fv");
    if (gd_noProgramBound()) return;  // desktop-GL parity: silent no-op
    if (count == 1 && value && location >= 0 && g_currentProgram != 0xFFFFFFFFu) {
        struct E { GLuint prog; GLint loc; float val[16]; };
        constexpr int N = 32;  // direct-mapped hash, power of 2
        thread_local E cache[N] = {};
        size_t h = ((size_t)g_currentProgram * 2654435761u + (size_t)location) & (N - 1);
        E& e = cache[h];
        if (e.prog == g_currentProgram && e.loc == location &&
            std::memcmp(e.val, value, sizeof(float) * 16) == 0) {
            return; // identical upload, skip driver call
        }
        e.prog = g_currentProgram;
        e.loc = location;
        std::memcpy(e.val, value, sizeof(float) * 16);
    }
    if (p) p(location, count, transpose, value);
}

// glVertexAttribPointer dedup — cocos2d sets identical layout (position/color/UV)
// on every sprite. Per-index cache of (size, type, normalized, stride, pointer)
// + currently-bound ARRAY_BUFFER. If all match — skip the call. ANGLE then
// avoids re-validating the vertex layout on each sprite (~1 µs saved per call).
//
// IMPORTANT: cache must invalidate when the bound ARRAY_BUFFER changes, because
// `pointer` is interpreted relative to the bound buffer. We track the buffer
// binding inside `gl_glBindBuffer` (target == GL_ARRAY_BUFFER) and clear the
// VAP cache there. (See above: `g_vapCacheArrayBuffer`.)
struct VAPCacheEntry {
    GLint    size;
    GLenum   type;
    GLboolean normalized;
    GLsizei  stride;
    const void* pointer;
    bool     valid;
};
static thread_local VAPCacheEntry g_vapCache[16] = {};
static thread_local GLuint g_vapCacheArrayBuffer = 0xFFFFFFFFu;
extern "C" void gdangle_invalidateVAPCache() {
    for (int i = 0; i < 16; i++) g_vapCache[i].valid = false;
}

// Reset every thread_local state cache we maintain. Called from
// wgl_wglMakeCurrent when the EGL context changes (e.g. after
// CCEGLView::toggleFullScreen). Cocos2d-x destroys and recreates the GL
// context across fullscreen toggles, but our thread_local dedup caches
// would otherwise survive the recreation, causing dedup'd binds to skip
// real work on the new context and producing null-deref crashes on
// follow-up state-dependent calls.
extern thread_local GLuint g_currentRBO;  // forward decl (defined later)
extern "C" void gdangle_invalidateProxyStateCaches(); // defined in gl_proxy.cpp
extern "C" void gdangle_invalidateAllStateCaches() {
    g_currentProgram = 0xFFFFFFFFu;
    g_currentVAO     = 0xFFFFFFFFu;
    g_vaaEnabledMask = 0;
    g_currentRBO     = 0xFFFFFFFFu;
    for (int i = 0; i < 8; i++) g_bufferBindings[i] = 0xFFFFFFFFu;
    g_vapCacheArrayBuffer = 0xFFFFFFFFu;
    for (int i = 0; i < 16; i++) g_vapCache[i].valid = false;
    gdangle_invalidateProxyStateCaches();
}

typedef void (WINAPI *PFN_VAP)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
extern "C" __declspec(dllexport) void WINAPI gl_glVertexAttribPointer(
        GLuint index, GLint size, GLenum type, GLboolean normalized,
        GLsizei stride, const void* pointer) {
    static PFN_VAP p = nullptr;
    if (!p) p = (PFN_VAP)glproxy::resolve("glVertexAttribPointer");
    if (index < 16) {
        VAPCacheEntry& e = g_vapCache[index];
        if (e.valid &&
            e.size == size && e.type == type &&
            e.normalized == normalized && e.stride == stride &&
            e.pointer == pointer) return;
        e.size = size; e.type = type; e.normalized = normalized;
        e.stride = stride; e.pointer = pointer; e.valid = true;
    }
    if (p) p(index, size, type, normalized, stride, pointer);
}

// VertexAttribArray enable/disable dedup. cocos2d toggles attrib 0/1/2/3
// (position, color, uv, ...) on every sprite. Bitmask tracks current state.
typedef void (WINAPI *PFN_EVAA)(GLuint);
extern "C" __declspec(dllexport) void WINAPI gl_glEnableVertexAttribArray(GLuint index) {
    static PFN_EVAA p = nullptr;
    if (!p) p = (PFN_EVAA)glproxy::resolve("glEnableVertexAttribArray");
    if (index < 32) {
        unsigned int mask = 1u << index;
        if (g_vaaEnabledMask & mask) return;
        g_vaaEnabledMask |= mask;
    }
    if (p) p(index);
}
extern "C" __declspec(dllexport) void WINAPI gl_glDisableVertexAttribArray(GLuint index) {
    static PFN_EVAA p = nullptr;
    if (!p) p = (PFN_EVAA)glproxy::resolve("glDisableVertexAttribArray");
    if (index < 32) {
        unsigned int mask = 1u << index;
        if (!(g_vaaEnabledMask & mask)) return;
        g_vaaEnabledMask &= ~mask;
    }
    if (p) p(index);
}
GLP_EXT_FORWARD_VOID(VertexAttrib1f, (GLuint index, GLfloat x), (index, x))
GLP_EXT_FORWARD_VOID(VertexAttrib2f, (GLuint index, GLfloat x, GLfloat y), (index, x, y))
GLP_EXT_FORWARD_VOID(VertexAttrib3f, (GLuint index, GLfloat x, GLfloat y, GLfloat z), (index, x, y, z))
GLP_EXT_FORWARD_VOID(VertexAttrib4f, (GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w), (index, x, y, z, w))
GLP_EXT_FORWARD_VOID(VertexAttrib1fv, (GLuint index, const GLfloat* v), (index, v))
GLP_EXT_FORWARD_VOID(VertexAttrib2fv, (GLuint index, const GLfloat* v), (index, v))
GLP_EXT_FORWARD_VOID(VertexAttrib3fv, (GLuint index, const GLfloat* v), (index, v))
GLP_EXT_FORWARD_VOID(VertexAttrib4fv, (GLuint index, const GLfloat* v), (index, v))

// Framebuffers
GLP_EXT_FORWARD_VOID(GenFramebuffers, (GLsizei n, GLuint* framebuffers), (n, framebuffers))

// glBindFramebuffer — no dedup. FB binds are not on the hot path (cocos2d
// does them only for render-to-texture passes, ~tens per frame max).
//
// Previous version had per-target dedup, but the thread_local cache survived
// GL context recreation (e.g. CCEGLView::toggleFullScreen rebuilds the
// context). After recreation, the cache held stale FB IDs; eclipse-menu /
// silicate rebinding "the same" FB got dedup'd, but ANGLE's new context had
// no FB bound, leading to null-deref crashes inside follow-up
// glRenderbufferStorage / glFramebufferTexture2D calls. Removing the dedup
// trades negligible perf for correctness.
//
typedef void (WINAPI *PFN_BFB)(GLenum, GLuint);
extern "C" __declspec(dllexport) void WINAPI gl_glBindFramebuffer(GLenum target, GLuint framebuffer) {
    static PFN_BFB p = nullptr;
    if (!p) p = (PFN_BFB)glproxy::resolve("glBindFramebuffer");
    static int n = 0;
    if (n < 30) angle::forceLog("glBindFramebuffer #%d: target=0x%X fb=%u", n, target, framebuffer);
    n++;
    if (p) p(target, framebuffer);
}
GLP_EXT_FORWARD_VOID(FramebufferTexture2D, (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level), (target, attachment, textarget, texture, level))
GLP_EXT_FORWARD_VOID(FramebufferRenderbuffer, (GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer), (target, attachment, renderbuffertarget, renderbuffer))
GLP_EXT_FORWARD(GLenum, CheckFramebufferStatus, (GLenum target), (target))
GLP_EXT_FORWARD_VOID(DeleteFramebuffers, (GLsizei n, const GLuint* framebuffers), (n, framebuffers))
GLP_EXT_FORWARD(GLboolean, IsFramebuffer, (GLuint framebuffer), (framebuffer))
typedef void (WINAPI *PFN_BFB_EXT)(GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum);
extern "C" __declspec(dllexport) void WINAPI gl_glBlitFramebuffer(
    GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
    GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1,
    GLbitfield mask, GLenum filter)
{
    static PFN_BFB_EXT p = nullptr;
    if (!p) p = (PFN_BFB_EXT)glproxy::resolve("glBlitFramebuffer");
    static int n = 0;
    if (Config::get().megahack_detected && n < 40) {
        angle::forceLog("glBlitFramebuffer #%d: src=(%d,%d)-(%d,%d) dst=(%d,%d)-(%d,%d) mask=0x%04X filter=0x%04X",
            n, srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, (unsigned)mask, filter);
        n++;
    }
    if (p) p(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
}
GLP_EXT_FORWARD_VOID(GetFramebufferAttachmentParameteriv, (GLenum target, GLenum attachment, GLenum pname, GLint* params), (target, attachment, pname, params))

// Renderbuffers
GLP_EXT_FORWARD_VOID(GenRenderbuffers, (GLsizei n, GLuint* renderbuffers), (n, renderbuffers))

// glBindRenderbuffer — no dedup, plus track the binding ourselves so that
// downstream glRenderbufferStorage* can verify a renderbuffer is actually
// bound before forwarding (ANGLE crashes if not, desktop-GL silently fails).
thread_local GLuint g_currentRBO = 0xFFFFFFFFu;
typedef void (WINAPI *PFN_BRB)(GLenum, GLuint);
extern "C" __declspec(dllexport) void WINAPI gl_glBindRenderbuffer(GLenum target, GLuint renderbuffer) {
    static PFN_BRB p = nullptr;
    if (!p) p = (PFN_BRB)glproxy::resolve("glBindRenderbuffer");
    if (target == 0x8D41 /*GL_RENDERBUFFER*/) g_currentRBO = renderbuffer;
    if (p) p(target, renderbuffer);
}
// glRenderbufferStorage / Multisample — desktop-GL parity guard. ANGLE
// crashes (null deref deep in libGLESv2) if no RBO is bound to GL_RENDERBUFFER
// at call time. Cocos2d's CCEGLView::updateWindow (called during fullscreen
// toggle) sometimes invokes RenderbufferStorage before the calling code has
// re-bound a fresh RBO in the recreated context. Cache check is cheap and
// avoids a one-shot ANGLE crash that takes the whole game down.
//
// If our cache is stale (caller bypassed our proxy for the bind, or the
// previous bind was invalidated by context recreation), trust-but-verify
// with glGetIntegerv(GL_RENDERBUFFER_BINDING). RBO storage calls are very
// rare (~tens per session), so the extra GL state query is free.
typedef void (WINAPI *PFN_GIV2)(GLenum, GLint*);
static bool gd_noRBOBound() {
    if (g_currentRBO != 0 && g_currentRBO != 0xFFFFFFFFu) return false;
    static PFN_GIV2 pGetIv = nullptr;
    if (!pGetIv) pGetIv = (PFN_GIV2)glproxy::resolve("glGetIntegerv");
    if (!pGetIv) return false;
    GLint cur = 0;
    pGetIv(0x8CA7 /*GL_RENDERBUFFER_BINDING*/, &cur);
    g_currentRBO = (GLuint)cur;
    return cur == 0;
}
typedef void (WINAPI *PFN_RBS)(GLenum, GLenum, GLsizei, GLsizei);
extern "C" __declspec(dllexport) void WINAPI gl_glRenderbufferStorage(
        GLenum target, GLenum internalformat, GLsizei width, GLsizei height) {
    static PFN_RBS p = nullptr;
    if (!p) p = (PFN_RBS)glproxy::resolve("glRenderbufferStorage");
    if (gd_noRBOBound()) return;  // desktop-GL parity: silent no-op
    if (p) p(target, internalformat, width, height);
}
typedef void (WINAPI *PFN_RBSM)(GLenum, GLsizei, GLenum, GLsizei, GLsizei);
extern "C" __declspec(dllexport) void WINAPI gl_glRenderbufferStorageMultisample(
        GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height) {
    static PFN_RBSM p = nullptr;
    if (!p) p = (PFN_RBSM)glproxy::resolve("glRenderbufferStorageMultisample");
    if (gd_noRBOBound()) return;  // desktop-GL parity: silent no-op
    if (p) p(target, samples, internalformat, width, height);
}
typedef void (WINAPI *PFN_DeleteRenderbuffers)(GLsizei, const GLuint*);
extern "C" __declspec(dllexport) void WINAPI gl_glDeleteRenderbuffers(GLsizei n, const GLuint* renderbuffers) {
    static PFN_DeleteRenderbuffers p = nullptr;
    if (!p) p = (PFN_DeleteRenderbuffers)glproxy::resolve("glDeleteRenderbuffers");
    if (p) p(n, renderbuffers);
    if (!renderbuffers || n <= 0) return;
    for (GLsizei i = 0; i < n; ++i) {
        if (renderbuffers[i] != 0 && renderbuffers[i] == g_currentRBO) {
            g_currentRBO = 0;
            break;
        }
    }
}
GLP_EXT_FORWARD(GLboolean, IsRenderbuffer, (GLuint renderbuffer), (renderbuffer))

// Misc
GLP_EXT_FORWARD_VOID(StencilFuncSeparate, (GLenum face, GLenum func, GLint ref, GLuint mask), (face, func, ref, mask))
GLP_EXT_FORWARD_VOID(StencilMaskSeparate, (GLenum face, GLuint mask), (face, mask))
GLP_EXT_FORWARD_VOID(StencilOpSeparate, (GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass), (face, sfail, dpfail, dppass))
// gl_glSampleCoverage moved to gl_proxy.cpp with dedup.

// ============================================================
// Desktop-only OpenGL stubs — not present in OpenGL ES / ANGLE
// ============================================================
// These functions exist in desktop OpenGL 1.x/2.x/3.x but have no
// equivalent in OpenGL ES (GLES 2/3). Geode mods compiled against desktop
// GL headers (peony.silicate, betterinfo, etc.) may call them directly via
// GetProcAddress or through GLEW. Without these exports:
//   - wglGetProcAddress returns gdangle_glNoOp (a last-resort catch-all)
//   - BUT if a mod resolves by name from our DLL directly it gets NULL → crash
// Exporting named no-ops here ensures both paths work correctly.

// --- Fixed-function pipeline (GL 1.x) ---
// These are completely absent in GLES; all no-ops are safe since GLES2+
// uses programmable shaders exclusively.
extern "C" __declspec(dllexport) void WINAPI gl_glMatrixMode(GLenum /*mode*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glLoadIdentity() {}
extern "C" __declspec(dllexport) void WINAPI gl_glLoadMatrixf(const GLfloat* /*m*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glLoadMatrixd(const GLdouble* /*m*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glMultMatrixf(const GLfloat* /*m*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glMultMatrixd(const GLdouble* /*m*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glPushMatrix() {}
extern "C" __declspec(dllexport) void WINAPI gl_glPopMatrix() {}
extern "C" __declspec(dllexport) void WINAPI gl_glOrtho(GLdouble /*l*/, GLdouble /*r*/, GLdouble /*b*/, GLdouble /*t*/, GLdouble /*n*/, GLdouble /*f*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glOrtho2D(GLdouble /*l*/, GLdouble /*r*/, GLdouble /*b*/, GLdouble /*t*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glFrustum(GLdouble /*l*/, GLdouble /*r*/, GLdouble /*b*/, GLdouble /*t*/, GLdouble /*n*/, GLdouble /*f*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glTranslatef(GLfloat /*x*/, GLfloat /*y*/, GLfloat /*z*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glTranslated(GLdouble /*x*/, GLdouble /*y*/, GLdouble /*z*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glRotatef(GLfloat /*a*/, GLfloat /*x*/, GLfloat /*y*/, GLfloat /*z*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glRotated(GLdouble /*a*/, GLdouble /*x*/, GLdouble /*y*/, GLdouble /*z*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glScalef(GLfloat /*x*/, GLfloat /*y*/, GLfloat /*z*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glScaled(GLdouble /*x*/, GLdouble /*y*/, GLdouble /*z*/) {}

// --- Immediate mode (GL 1.x) ---
extern "C" __declspec(dllexport) void WINAPI gl_glBegin(GLenum /*mode*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glEnd() {}
extern "C" __declspec(dllexport) void WINAPI gl_glVertex2f(GLfloat /*x*/, GLfloat /*y*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glVertex2i(GLint /*x*/, GLint /*y*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glVertex3f(GLfloat /*x*/, GLfloat /*y*/, GLfloat /*z*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glVertex3d(GLdouble /*x*/, GLdouble /*y*/, GLdouble /*z*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glVertex2fv(const GLfloat* /*v*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glVertex3fv(const GLfloat* /*v*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glNormal3f(GLfloat /*x*/, GLfloat /*y*/, GLfloat /*z*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glNormal3fv(const GLfloat* /*v*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glColor3f(GLfloat /*r*/, GLfloat /*g*/, GLfloat /*b*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glColor3ub(GLubyte /*r*/, GLubyte /*g*/, GLubyte /*b*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glColor4f(GLfloat /*r*/, GLfloat /*g*/, GLfloat /*b*/, GLfloat /*a*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glColor4ub(GLubyte /*r*/, GLubyte /*g*/, GLubyte /*b*/, GLubyte /*a*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glColor4fv(const GLfloat* /*v*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glTexCoord2f(GLfloat /*s*/, GLfloat /*t*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glTexCoord2fv(const GLfloat* /*v*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glRasterPos2f(GLfloat /*x*/, GLfloat /*y*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glRasterPos2i(GLint /*x*/, GLint /*y*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glRasterPos3f(GLfloat /*x*/, GLfloat /*y*/, GLfloat /*z*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glRasterPos2fv(const GLfloat* /*v*/) {}

// --- Attribute stack (GL 1.x) ---
struct GdAttribSnapshot {
    GLint program;
    GLint activeTexture;
    GLint tex2D[8];
    GLint arrayBuffer;
    GLint elementArrayBuffer;
    GLint vertexArray;
    GLint drawFbo;
    GLint readFbo;
    GLint viewport[4];
    GLint scissorBox[4];
    GLboolean blend;
    GLboolean depthTest;
    GLboolean stencilTest;
    GLboolean cullFace;
    GLboolean scissorTest;
    GLboolean colorMask[4];
    GLint blendSrcRGB;
    GLint blendDstRGB;
    GLint blendSrcAlpha;
    GLint blendDstAlpha;
    GLint blendEqRGB;
    GLint blendEqAlpha;


    // Depth state — cocos2d sometimes disables depth-mask for transparent
    // sprite passes; if ImGui's restore leaves it differently, the next
    // pass corrupts the depth buffer.
    GLint     depthFunc;
    GLboolean depthMask;
    GLfloat   depthRange[2];

    // Stencil state (front-face only — we mirror to back).
    // Stencil is critical for CCClippingNode masking; if MegaHack leaves
    // GL_STENCIL_TEST on or with a bad func, clipping nodes disappear.
    GLint stencilFunc;
    GLint stencilValueMask;
    GLint stencilRef;
    GLint stencilMask;
    GLint stencilOpFail;
    GLint stencilOpZFail;
    GLint stencilOpZPass;

    // Pixel-store state — ImGui uses UNPACK_ROW_LENGTH for partial atlas
    // updates. If left non-zero after Pop, cocos2d's tight texture
    // uploads sample wrong row offsets => corrupted textures.
    GLint unpackAlignment;
    GLint unpackRowLength;
    GLint unpackSkipRows;
    GLint unpackSkipPixels;

    // Clear color — cheap to save, catches mods that change it.
    GLfloat clearColor[4];

    // Per-slot vertex attribute state. cocos2d and MegaHack/ImGui share
    // the default VAO 0 (the only one ANGLE/GLES allows by default) so
    // ImGui's vertex pointer/format inside VAO 0 leak back into cocos2d's
    // draws after Pop — producing garbage vertex data => invisible sprites.
    // We save the state per-slot to avoid this.
    struct {
        GLboolean enabled;
        GLint size;
        GLenum type;
        GLboolean normalized;
        GLsizei stride;
        const void* pointer;
        GLuint buffer;
        GLuint divisor;
    } va[16];
};

static GdAttribSnapshot g_attribStack[16];
static int g_attribStackDepth = 0;

extern "C" __declspec(dllexport) void WINAPI gl_glPushAttrib(GLbitfield /*mask*/) {
    gdangle_noteMegaHackOverlayDraw();
    if (g_attribStackDepth >= 16) {
        static int warnN = 0;
        if (warnN < 4) { angle::forceLog("glPushAttrib: stack overflow (>16) — state will be lost on Pop"); warnN++; }
        return;
    }
    typedef void (WINAPI *PFN_GI)(GLenum, GLint*);
    typedef void (WINAPI *PFN_GB)(GLenum, GLboolean*);
    typedef unsigned char (WINAPI *PFN_IE)(GLenum);
    typedef void (WINAPI *PFN_AT)(GLenum);
    static PFN_GI pGetI = nullptr;
    static PFN_GB pGetB = nullptr;
    static PFN_IE pIsEnabled = nullptr;
    static PFN_AT pActiveTexture = nullptr;
    if (!pGetI) pGetI = (PFN_GI)glproxy::resolve("glGetIntegerv");
    if (!pGetB) pGetB = (PFN_GB)glproxy::resolve("glGetBooleanv");
    if (!pIsEnabled) pIsEnabled = (PFN_IE)glproxy::resolve("glIsEnabled");
    if (!pActiveTexture) pActiveTexture = (PFN_AT)glproxy::resolve("glActiveTexture");
    if (!pGetI) return;

    GdAttribSnapshot& s = g_attribStack[g_attribStackDepth++];
    pGetI(0x8B8D /*GL_CURRENT_PROGRAM*/, &s.program);
    pGetI(0x84E0 /*GL_ACTIVE_TEXTURE*/, &s.activeTexture);
    for (int i = 0; i < 8; ++i) {
        if (pActiveTexture) pActiveTexture(0x84C0 + i);
        pGetI(0x8069 /*GL_TEXTURE_BINDING_2D*/, &s.tex2D[i]);
    }
    if (pActiveTexture) pActiveTexture((GLenum)s.activeTexture);
    pGetI(0x8894 /*GL_ARRAY_BUFFER_BINDING*/, &s.arrayBuffer);
    pGetI(0x8895 /*GL_ELEMENT_ARRAY_BUFFER_BINDING*/, &s.elementArrayBuffer);
    pGetI(0x85B5 /*GL_VERTEX_ARRAY_BINDING*/, &s.vertexArray);
    pGetI(0x8CA6 /*GL_DRAW_FRAMEBUFFER_BINDING*/, &s.drawFbo);
    pGetI(0x8CAA /*GL_READ_FRAMEBUFFER_BINDING*/, &s.readFbo);
    pGetI(0x0BA2 /*GL_VIEWPORT*/, s.viewport);
    pGetI(0x0C10 /*GL_SCISSOR_BOX*/, s.scissorBox);
    if (pGetB) pGetB(0x0C23 /*GL_COLOR_WRITEMASK*/, s.colorMask);
    else { s.colorMask[0] = s.colorMask[1] = s.colorMask[2] = s.colorMask[3] = 1; }
    s.blend = pIsEnabled ? pIsEnabled(0x0BE2 /*GL_BLEND*/) : 0;
    s.depthTest = pIsEnabled ? pIsEnabled(0x0B71 /*GL_DEPTH_TEST*/) : 0;
    s.stencilTest = pIsEnabled ? pIsEnabled(0x0B90 /*GL_STENCIL_TEST*/) : 0;
    s.cullFace = pIsEnabled ? pIsEnabled(0x0B44 /*GL_CULL_FACE*/) : 0;
    s.scissorTest = pIsEnabled ? pIsEnabled(0x0C11 /*GL_SCISSOR_TEST*/) : 0;
    pGetI(0x80C9 /*GL_BLEND_SRC_RGB*/, &s.blendSrcRGB);
    pGetI(0x80C8 /*GL_BLEND_DST_RGB*/, &s.blendDstRGB);
    pGetI(0x80CB /*GL_BLEND_SRC_ALPHA*/, &s.blendSrcAlpha);
    pGetI(0x80CA /*GL_BLEND_DST_ALPHA*/, &s.blendDstAlpha);
    pGetI(0x8009 /*GL_BLEND_EQUATION_RGB*/, &s.blendEqRGB);
    pGetI(0x883D /*GL_BLEND_EQUATION_ALPHA*/, &s.blendEqAlpha);

    // Depth state
    pGetI(0x0B74 /*GL_DEPTH_FUNC*/, &s.depthFunc);
    {
        GLboolean dm = 1;
        if (pGetB) pGetB(0x0B72 /*GL_DEPTH_WRITEMASK*/, &dm);
        s.depthMask = dm;
    }
    {
        typedef void (WINAPI *PFN_GF)(GLenum, GLfloat*);
        static PFN_GF pGetFloatv = nullptr;
        if (!pGetFloatv) pGetFloatv = (PFN_GF)glproxy::resolve("glGetFloatv");
        if (pGetFloatv) pGetFloatv(0x0B70 /*GL_DEPTH_RANGE*/, s.depthRange);
        else { s.depthRange[0] = 0.0f; s.depthRange[1] = 1.0f; }
    }

    // Stencil state (front face)
    pGetI(0x0B92 /*GL_STENCIL_FUNC*/,        &s.stencilFunc);
    pGetI(0x0B93 /*GL_STENCIL_VALUE_MASK*/,  &s.stencilValueMask);
    pGetI(0x0B97 /*GL_STENCIL_REF*/,         &s.stencilRef);
    pGetI(0x0B98 /*GL_STENCIL_WRITEMASK*/,   &s.stencilMask);
    pGetI(0x0B94 /*GL_STENCIL_FAIL*/,        &s.stencilOpFail);
    pGetI(0x0B95 /*GL_STENCIL_PASS_DEPTH_FAIL*/, &s.stencilOpZFail);
    pGetI(0x0B96 /*GL_STENCIL_PASS_DEPTH_PASS*/, &s.stencilOpZPass);

    // Pixel-store state (unpack side only — pack side isn't touched by ImGui)
    pGetI(0x0CF5 /*GL_UNPACK_ALIGNMENT*/,    &s.unpackAlignment);
    pGetI(0x0CF2 /*GL_UNPACK_ROW_LENGTH*/,   &s.unpackRowLength);
    pGetI(0x0CF3 /*GL_UNPACK_SKIP_ROWS*/,    &s.unpackSkipRows);
    pGetI(0x0CF4 /*GL_UNPACK_SKIP_PIXELS*/,  &s.unpackSkipPixels);

    // Clear color
    {
        typedef void (WINAPI *PFN_GF)(GLenum, GLfloat*);
        static PFN_GF pGetFloatv = nullptr;
        if (!pGetFloatv) pGetFloatv = (PFN_GF)glproxy::resolve("glGetFloatv");
        if (pGetFloatv) pGetFloatv(0x0C22 /*GL_COLOR_CLEAR_VALUE*/, s.clearColor);
        else { s.clearColor[0] = s.clearColor[1] = s.clearColor[2] = 0.0f; s.clearColor[3] = 1.0f; }
    }


    // Vertex attribute state (per slot). See struct comment.
    typedef void (WINAPI *PFN_GVAIV)(GLuint, GLenum, GLint*);
    typedef void (WINAPI *PFN_GVAPV)(GLuint, GLenum, void**);
    static PFN_GVAIV pGetVertexAttribiv = nullptr;
    static PFN_GVAPV pGetVertexAttribPointerv = nullptr;
    if (!pGetVertexAttribiv) pGetVertexAttribiv = (PFN_GVAIV)glproxy::resolve("glGetVertexAttribiv");
    if (!pGetVertexAttribPointerv) pGetVertexAttribPointerv = (PFN_GVAPV)glproxy::resolve("glGetVertexAttribPointerv");
    for (int i = 0; i < 16; ++i) {
        auto& a = s.va[i];
        a.enabled = 0; a.size = 4; a.type = 0x1406 /*GL_FLOAT*/;
        a.normalized = 0; a.stride = 0; a.pointer = nullptr;
        a.buffer = 0; a.divisor = 0;
        if (!pGetVertexAttribiv) continue;
        GLint tmp = 0;
        pGetVertexAttribiv((GLuint)i, 0x8622 /*ARRAY_ENABLED*/, &tmp);  a.enabled = (GLboolean)tmp;
        pGetVertexAttribiv((GLuint)i, 0x8623 /*ARRAY_SIZE*/, &tmp);     a.size = tmp;
        pGetVertexAttribiv((GLuint)i, 0x8625 /*ARRAY_TYPE*/, &tmp);     a.type = (GLenum)tmp;
        pGetVertexAttribiv((GLuint)i, 0x886A /*ARRAY_NORMALIZED*/, &tmp); a.normalized = (GLboolean)tmp;
        pGetVertexAttribiv((GLuint)i, 0x8624 /*ARRAY_STRIDE*/, &tmp);   a.stride = (GLsizei)tmp;
        pGetVertexAttribiv((GLuint)i, 0x889F /*ARRAY_BUFFER_BINDING*/, &tmp); a.buffer = (GLuint)tmp;
        pGetVertexAttribiv((GLuint)i, 0x88FE /*ARRAY_DIVISOR*/, &tmp);  a.divisor = (GLuint)tmp;
        if (pGetVertexAttribPointerv) {
            void* p = nullptr;
            pGetVertexAttribPointerv((GLuint)i, 0x8645 /*ARRAY_POINTER*/, &p);
            a.pointer = p;
        }
    }
    static int n = 0;
    if (Config::get().megahack_detected && n < 32) {
        angle::forceLog("glPushAttrib #%d: prog=%d active=0x%04X tex0=%d fbo=%d", n, s.program, s.activeTexture, s.tex2D[0], s.drawFbo);
        n++;
    }
}

static void gd_restoreCap(GLenum cap, GLboolean enabled) {
    typedef void (WINAPI *PFN_ED)(GLenum);
    static PFN_ED pEnable = nullptr;
    static PFN_ED pDisable = nullptr;
    if (!pEnable) pEnable = (PFN_ED)glproxy::resolve("glEnable");
    if (!pDisable) pDisable = (PFN_ED)glproxy::resolve("glDisable");
    if (enabled) { if (pEnable) pEnable(cap); }
    else { if (pDisable) pDisable(cap); }
}

extern "C" __declspec(dllexport) void WINAPI gl_glPopAttrib() {
    if (g_attribStackDepth <= 0) return;
    const GdAttribSnapshot s = g_attribStack[--g_attribStackDepth];
    typedef void (WINAPI *PFN_UP)(GLuint);
    typedef void (WINAPI *PFN_AT)(GLenum);
    typedef void (WINAPI *PFN_BT)(GLenum, GLuint);
    typedef void (WINAPI *PFN_BB)(GLenum, GLuint);
    typedef void (WINAPI *PFN_BVA)(GLuint);
    typedef void (WINAPI *PFN_BFB)(GLenum, GLuint);
    typedef void (WINAPI *PFN_VP)(GLint, GLint, GLsizei, GLsizei);
    typedef void (WINAPI *PFN_CM)(GLboolean, GLboolean, GLboolean, GLboolean);
    typedef void (WINAPI *PFN_BFS)(GLenum, GLenum, GLenum, GLenum);
    typedef void (WINAPI *PFN_BES)(GLenum, GLenum);
    static PFN_UP pUseProgram = nullptr;
    static PFN_AT pActiveTexture = nullptr;
    static PFN_BT pBindTexture = nullptr;
    static PFN_BB pBindBuffer = nullptr;
    static PFN_BVA pBindVertexArray = nullptr;
    static PFN_BFB pBindFramebuffer = nullptr;
    static PFN_VP pViewport = nullptr;
    static PFN_VP pScissor = nullptr;
    static PFN_CM pColorMask = nullptr;
    static PFN_BFS pBlendFuncSeparate = nullptr;
    static PFN_BES pBlendEquationSeparate = nullptr;
    if (!pUseProgram) pUseProgram = (PFN_UP)glproxy::resolve("glUseProgram");
    if (!pActiveTexture) pActiveTexture = (PFN_AT)glproxy::resolve("glActiveTexture");
    if (!pBindTexture) pBindTexture = (PFN_BT)glproxy::resolve("glBindTexture");
    if (!pBindBuffer) pBindBuffer = (PFN_BB)glproxy::resolve("glBindBuffer");
    if (!pBindVertexArray) pBindVertexArray = (PFN_BVA)glproxy::resolve("glBindVertexArray");
    if (!pBindFramebuffer) pBindFramebuffer = (PFN_BFB)glproxy::resolve("glBindFramebuffer");
    if (!pViewport) pViewport = (PFN_VP)glproxy::resolve("glViewport");
    if (!pScissor) pScissor = (PFN_VP)glproxy::resolve("glScissor");
    if (!pColorMask) pColorMask = (PFN_CM)glproxy::resolve("glColorMask");
    if (!pBlendFuncSeparate) pBlendFuncSeparate = (PFN_BFS)glproxy::resolve("glBlendFuncSeparate");
    if (!pBlendEquationSeparate) pBlendEquationSeparate = (PFN_BES)glproxy::resolve("glBlendEquationSeparate");

    if (pBindFramebuffer) {
        pBindFramebuffer(0x8CA8 /*GL_READ_FRAMEBUFFER*/, (GLuint)s.readFbo);
        pBindFramebuffer(0x8CA9 /*GL_DRAW_FRAMEBUFFER*/, (GLuint)s.drawFbo);
    }
    if (pUseProgram) pUseProgram((GLuint)s.program);
    if (pBindVertexArray) pBindVertexArray((GLuint)s.vertexArray);
    if (pBindBuffer) {
        pBindBuffer(0x8892 /*GL_ARRAY_BUFFER*/, (GLuint)s.arrayBuffer);
        pBindBuffer(0x8893 /*GL_ELEMENT_ARRAY_BUFFER*/, (GLuint)s.elementArrayBuffer);
    }
    for (int i = 0; i < 8; ++i) {
        if (pActiveTexture) pActiveTexture(0x84C0 + i);
        if (pBindTexture) pBindTexture(0x0DE1 /*GL_TEXTURE_2D*/, (GLuint)s.tex2D[i]);
    }
    if (pActiveTexture) pActiveTexture((GLenum)s.activeTexture);
    if (pViewport) pViewport(s.viewport[0], s.viewport[1], s.viewport[2], s.viewport[3]);
    if (pScissor) pScissor(s.scissorBox[0], s.scissorBox[1], s.scissorBox[2], s.scissorBox[3]);
    if (pColorMask) pColorMask(s.colorMask[0], s.colorMask[1], s.colorMask[2], s.colorMask[3]);
    gd_restoreCap(0x0BE2 /*GL_BLEND*/, s.blend);
    gd_restoreCap(0x0B71 /*GL_DEPTH_TEST*/, s.depthTest);
    gd_restoreCap(0x0B90 /*GL_STENCIL_TEST*/, s.stencilTest);
    gd_restoreCap(0x0B44 /*GL_CULL_FACE*/, s.cullFace);
    gd_restoreCap(0x0C11 /*GL_SCISSOR_TEST*/, s.scissorTest);
    if (pBlendFuncSeparate) pBlendFuncSeparate((GLenum)s.blendSrcRGB, (GLenum)s.blendDstRGB, (GLenum)s.blendSrcAlpha, (GLenum)s.blendDstAlpha);
    if (pBlendEquationSeparate) pBlendEquationSeparate((GLenum)s.blendEqRGB, (GLenum)s.blendEqAlpha);

    // Depth state restore
    {
        typedef void (WINAPI *PFN_DF)(GLenum);
        typedef void (WINAPI *PFN_DM)(GLboolean);
        typedef void (WINAPI *PFN_DR)(GLfloat, GLfloat);
        static PFN_DF pDepthFunc  = nullptr;
        static PFN_DM pDepthMask  = nullptr;
        static PFN_DR pDepthRange = nullptr;
        if (!pDepthFunc)  pDepthFunc  = (PFN_DF)glproxy::resolve("glDepthFunc");
        if (!pDepthMask)  pDepthMask  = (PFN_DM)glproxy::resolve("glDepthMask");
        if (!pDepthRange) pDepthRange = (PFN_DR)glproxy::resolve("glDepthRangef");
        if (pDepthFunc)  pDepthFunc((GLenum)s.depthFunc);
        if (pDepthMask)  pDepthMask(s.depthMask);
        if (pDepthRange) pDepthRange(s.depthRange[0], s.depthRange[1]);
    }

    // Stencil state restore (apply to both faces — safe for cocos2d's use)
    {
        typedef void (WINAPI *PFN_SF)(GLenum, GLint, GLuint);
        typedef void (WINAPI *PFN_SM)(GLuint);
        typedef void (WINAPI *PFN_SO)(GLenum, GLenum, GLenum);
        static PFN_SF pStencilFunc = nullptr;
        static PFN_SM pStencilMask = nullptr;
        static PFN_SO pStencilOp   = nullptr;
        if (!pStencilFunc) pStencilFunc = (PFN_SF)glproxy::resolve("glStencilFunc");
        if (!pStencilMask) pStencilMask = (PFN_SM)glproxy::resolve("glStencilMask");
        if (!pStencilOp)   pStencilOp   = (PFN_SO)glproxy::resolve("glStencilOp");
        if (pStencilFunc) pStencilFunc((GLenum)s.stencilFunc, s.stencilRef, (GLuint)s.stencilValueMask);
        if (pStencilMask) pStencilMask((GLuint)s.stencilMask);
        if (pStencilOp)   pStencilOp((GLenum)s.stencilOpFail, (GLenum)s.stencilOpZFail, (GLenum)s.stencilOpZPass);
    }

    // Pixel-store restore
    {
        typedef void (WINAPI *PFN_PS)(GLenum, GLint);
        static PFN_PS pPixelStorei = nullptr;
        if (!pPixelStorei) pPixelStorei = (PFN_PS)glproxy::resolve("glPixelStorei");
        if (pPixelStorei) {
            pPixelStorei(0x0CF5 /*GL_UNPACK_ALIGNMENT*/,   s.unpackAlignment);
            pPixelStorei(0x0CF2 /*GL_UNPACK_ROW_LENGTH*/,  s.unpackRowLength);
            pPixelStorei(0x0CF3 /*GL_UNPACK_SKIP_ROWS*/,   s.unpackSkipRows);
            pPixelStorei(0x0CF4 /*GL_UNPACK_SKIP_PIXELS*/, s.unpackSkipPixels);
        }
    }

    // Clear color restore
    {
        typedef void (WINAPI *PFN_CC)(GLfloat, GLfloat, GLfloat, GLfloat);
        static PFN_CC pClearColor = nullptr;
        if (!pClearColor) pClearColor = (PFN_CC)glproxy::resolve("glClearColor");
        if (pClearColor) pClearColor(s.clearColor[0], s.clearColor[1], s.clearColor[2], s.clearColor[3]);
    }


    // Restore vertex attribute state. See struct comment in GdAttribSnapshot.
    typedef void (WINAPI *PFN_VAP_)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
    typedef void (WINAPI *PFN_E_)(GLuint);
    typedef void (WINAPI *PFN_VAD_)(GLuint, GLuint);
    static PFN_VAP_ pVertexAttribPointer = nullptr;
    static PFN_E_ pEnableVAA = nullptr;
    static PFN_E_ pDisableVAA = nullptr;
    static PFN_VAD_ pVertexAttribDivisor = nullptr;
    static bool divisorResolved = false;
    if (!pVertexAttribPointer) pVertexAttribPointer = (PFN_VAP_)glproxy::resolve("glVertexAttribPointer");
    if (!pEnableVAA) pEnableVAA = (PFN_E_)glproxy::resolve("glEnableVertexAttribArray");
    if (!pDisableVAA) pDisableVAA = (PFN_E_)glproxy::resolve("glDisableVertexAttribArray");
    if (!divisorResolved) {
        pVertexAttribDivisor = (PFN_VAD_)glproxy::resolve("glVertexAttribDivisor");
        divisorResolved = true;
    }
    for (int i = 0; i < 16; ++i) {
        const auto& a = s.va[i];
        if (pBindBuffer)            pBindBuffer(0x8892 /*GL_ARRAY_BUFFER*/, a.buffer);
        if (pVertexAttribPointer)   pVertexAttribPointer((GLuint)i, a.size, a.type, a.normalized, a.stride, a.pointer);
        if (pVertexAttribDivisor)   pVertexAttribDivisor((GLuint)i, a.divisor);
        if (a.enabled) { if (pEnableVAA)  pEnableVAA((GLuint)i); g_vaaEnabledMask |=  (1u << i); }
        else            { if (pDisableVAA) pDisableVAA((GLuint)i); g_vaaEnabledMask &= ~(1u << i); }
    }
    // The per-slot rebinds clobbered ARRAY_BUFFER; restore the snapshot value.
    if (pBindBuffer) pBindBuffer(0x8892 /*GL_ARRAY_BUFFER*/, (GLuint)s.arrayBuffer);
    static int n = 0;
    if (Config::get().megahack_detected && n < 32) {
        angle::forceLog("glPopAttrib #%d: prog=%d active=0x%04X tex0=%d fbo=%d", n, s.program, s.activeTexture, s.tex2D[0], s.drawFbo);
        n++;
    }
    gdangle_invalidateVAPCache();
    gdangle_invalidateProxyStateCaches();
    gdangle_invalidateProgramCache();
}
extern "C" __declspec(dllexport) void WINAPI gl_glPushClientAttrib(GLbitfield mask) { gl_glPushAttrib(mask); }
extern "C" __declspec(dllexport) void WINAPI gl_glPopClientAttrib() { gl_glPopAttrib(); }

// --- Lighting / material / fog (GL 1.x) ---
extern "C" __declspec(dllexport) void WINAPI gl_glShadeModel(GLenum /*mode*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glLightf(GLenum /*light*/, GLenum /*pname*/, GLfloat /*param*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glLightfv(GLenum /*light*/, GLenum /*pname*/, const GLfloat* /*params*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glLightiv(GLenum /*light*/, GLenum /*pname*/, const GLint* /*params*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glLightModelf(GLenum /*pname*/, GLfloat /*param*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glLightModelfv(GLenum /*pname*/, const GLfloat* /*params*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glLightModeli(GLenum /*pname*/, GLint /*param*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glMaterialf(GLenum /*face*/, GLenum /*pname*/, GLfloat /*param*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glMaterialfv(GLenum /*face*/, GLenum /*pname*/, const GLfloat* /*params*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glMateriali(GLenum /*face*/, GLenum /*pname*/, GLint /*param*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glColorMaterial(GLenum /*face*/, GLenum /*mode*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glFogf(GLenum /*pname*/, GLfloat /*param*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glFogfv(GLenum /*pname*/, const GLfloat* /*params*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glFogi(GLenum /*pname*/, GLint /*param*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glFogiv(GLenum /*pname*/, const GLint* /*params*/) {}

// --- Legacy pixel/texture ops (GL 1.x) ---
extern "C" __declspec(dllexport) void WINAPI gl_glLogicOp(GLenum /*opcode*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glAlphaFunc(GLenum /*func*/, GLfloat /*ref*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glAccum(GLenum /*op*/, GLfloat /*value*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glClearAccum(GLfloat /*r*/, GLfloat /*g*/, GLfloat /*b*/, GLfloat /*a*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glClearIndex(GLfloat /*c*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glIndexMask(GLuint /*mask*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glRectf(GLfloat /*x1*/, GLfloat /*y1*/, GLfloat /*x2*/, GLfloat /*y2*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glRecti(GLint /*x1*/, GLint /*y1*/, GLint /*x2*/, GLint /*y2*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glBitmap(GLsizei /*w*/, GLsizei /*h*/, GLfloat /*xorig*/, GLfloat /*yorig*/, GLfloat /*xmove*/, GLfloat /*ymove*/, const GLubyte* /*bitmap*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glCopyPixels(GLint /*x*/, GLint /*y*/, GLsizei /*w*/, GLsizei /*h*/, GLenum /*type*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glDrawPixels(GLsizei /*w*/, GLsizei /*h*/, GLenum /*fmt*/, GLenum /*type*/, const GLvoid* /*pixels*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glTexEnvf(GLenum /*target*/, GLenum /*pname*/, GLfloat /*param*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glTexEnvfv(GLenum /*target*/, GLenum /*pname*/, const GLfloat* /*params*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glTexEnvi(GLenum /*target*/, GLenum /*pname*/, GLint /*param*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glTexEnviv(GLenum /*target*/, GLenum /*pname*/, const GLint* /*params*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glTexImage1D(GLenum /*target*/, GLint /*level*/, GLint /*ifmt*/, GLsizei /*w*/, GLint /*border*/, GLenum /*fmt*/, GLenum /*type*/, const GLvoid* /*px*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glTexImage3D(GLenum /*target*/, GLint /*level*/, GLint /*ifmt*/, GLsizei /*w*/, GLsizei /*h*/, GLsizei /*d*/, GLint /*border*/, GLenum /*fmt*/, GLenum /*type*/, const GLvoid* /*px*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glTexSubImage1D(GLenum /*t*/, GLint /*l*/, GLint /*xoff*/, GLsizei /*w*/, GLenum /*fmt*/, GLenum /*type*/, const GLvoid* /*px*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glCopyTexImage1D(GLenum /*t*/, GLint /*l*/, GLenum /*ifmt*/, GLint /*x*/, GLint /*y*/, GLsizei /*w*/, GLint /*border*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glGetTexImage(GLenum /*target*/, GLint /*level*/, GLenum /*fmt*/, GLenum /*type*/, GLvoid* /*px*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glGetTexLevelParameteriv(GLenum /*target*/, GLint /*level*/, GLenum /*pname*/, GLint* /*params*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glGetTexLevelParameterfv(GLenum /*target*/, GLint /*level*/, GLenum /*pname*/, GLfloat* /*params*/) {}

// --- Legacy array pointers (GL 1.1) ---
extern "C" __declspec(dllexport) void WINAPI gl_glVertexPointer(GLint /*size*/, GLenum /*type*/, GLsizei /*stride*/, const GLvoid* /*ptr*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glNormalPointer(GLenum /*type*/, GLsizei /*stride*/, const GLvoid* /*ptr*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glColorPointer(GLint /*size*/, GLenum /*type*/, GLsizei /*stride*/, const GLvoid* /*ptr*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glTexCoordPointer(GLint /*size*/, GLenum /*type*/, GLsizei /*stride*/, const GLvoid* /*ptr*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glIndexPointer(GLenum /*type*/, GLsizei /*stride*/, const GLvoid* /*ptr*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glEdgeFlagPointer(GLsizei /*stride*/, const GLvoid* /*ptr*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glEnableClientState(GLenum /*array*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glDisableClientState(GLenum /*array*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glClientActiveTexture(GLenum /*texture*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glMultiTexCoord2f(GLenum /*target*/, GLfloat /*s*/, GLfloat /*t*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glMultiTexCoord2fv(GLenum /*target*/, const GLfloat* /*v*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glInterleavedArrays(GLenum /*format*/, GLsizei /*stride*/, const GLvoid* /*ptr*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glArrayElement(GLint /*i*/) {}

// --- Display lists (GL 1.x) ---
extern "C" __declspec(dllexport) GLuint WINAPI gl_glGenLists(GLsizei /*range*/) { return 0; }
extern "C" __declspec(dllexport) void WINAPI gl_glNewList(GLuint /*list*/, GLenum /*mode*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glEndList() {}
extern "C" __declspec(dllexport) void WINAPI gl_glCallList(GLuint /*list*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glCallLists(GLsizei /*n*/, GLenum /*type*/, const GLvoid* /*lists*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glDeleteLists(GLuint /*list*/, GLsizei /*range*/) {}
extern "C" __declspec(dllexport) GLboolean WINAPI gl_glIsList(GLuint /*list*/) { return GL_FALSE; }
extern "C" __declspec(dllexport) void WINAPI gl_glListBase(GLuint /*base*/) {}

// --- Desktop GL 2.0+ without GLES equivalent ---
extern "C" __declspec(dllexport) void WINAPI gl_glDrawRangeElements(GLenum mode, GLuint /*start*/, GLuint /*end*/, GLsizei count, GLenum type, const GLvoid* indices) {
    // Forward to glDrawElements — start/end are only hints, safe to ignore
    typedef void (WINAPI *PFN)(GLenum, GLsizei, GLenum, const GLvoid*);
    static PFN p = nullptr;
    if (!p) p = (PFN)glproxy::resolve("glDrawElements");
    if (p) p(mode, count, type, indices);
}
extern "C" __declspec(dllexport) void WINAPI gl_glMultiDrawArrays(GLenum mode, const GLint* first, const GLsizei* count, GLsizei drawcount) {
    typedef void (WINAPI *PFN)(GLenum, GLint, GLsizei);
    static PFN p = nullptr;
    if (!p) p = (PFN)glproxy::resolve("glDrawArrays");
    if (p) for (GLsizei i = 0; i < drawcount; i++) p(mode, first[i], count[i]);
}
extern "C" __declspec(dllexport) void WINAPI gl_glMultiDrawElements(GLenum mode, const GLsizei* count, GLenum type, const GLvoid* const* indices, GLsizei drawcount) {
    typedef void (WINAPI *PFN)(GLenum, GLsizei, GLenum, const GLvoid*);
    static PFN p = nullptr;
    if (!p) p = (PFN)glproxy::resolve("glDrawElements");
    if (p) for (GLsizei i = 0; i < drawcount; i++) p(mode, count[i], type, indices[i]);
}

// --- GL 3.x+ desktop stubs (geometry shaders, transform feedback, queries) ---
extern "C" __declspec(dllexport) void WINAPI gl_glBeginTransformFeedback(GLenum /*primitiveMode*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glEndTransformFeedback() {}
extern "C" __declspec(dllexport) void WINAPI gl_glTransformFeedbackVaryings(GLuint /*prog*/, GLsizei /*count*/, const GLchar* const* /*varyings*/, GLenum /*mode*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glGetTransformFeedbackVarying(GLuint /*prog*/, GLuint /*idx*/, GLsizei /*bufSize*/, GLsizei* /*len*/, GLsizei* /*size*/, GLenum* /*type*/, GLchar* /*name*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glPatchParameteri(GLenum /*pname*/, GLint /*value*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glPatchParameterfv(GLenum /*pname*/, const GLfloat* /*values*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glDrawArraysInstanced(GLenum mode, GLint first, GLsizei count, GLsizei instancecount) {
    typedef void (WINAPI *PFN_I)(GLenum, GLint, GLsizei, GLsizei);
    typedef void (WINAPI *PFN_P)(GLenum, GLint, GLsizei);
    static PFN_I pInst = nullptr;
    static PFN_P pPlain = nullptr;
    if (!pInst) pInst = (PFN_I)glproxy::resolve("glDrawArraysInstanced");
    if (!pPlain) pPlain = (PFN_P)glproxy::resolve("glDrawArrays");
    if (pInst) pInst(mode, first, count, instancecount);
    else if (pPlain) pPlain(mode, first, count);
}
extern "C" __declspec(dllexport) void WINAPI gl_glDrawElementsInstanced(GLenum mode, GLsizei count, GLenum type, const GLvoid* indices, GLsizei instancecount) {
    typedef void (WINAPI *PFN_I)(GLenum, GLsizei, GLenum, const GLvoid*, GLsizei);
    typedef void (WINAPI *PFN_P)(GLenum, GLsizei, GLenum, const GLvoid*);
    static PFN_I pInst = nullptr;
    static PFN_P pPlain = nullptr;
    if (!pInst) pInst = (PFN_I)glproxy::resolve("glDrawElementsInstanced");
    if (!pPlain) pPlain = (PFN_P)glproxy::resolve("glDrawElements");
    if (pInst) pInst(mode, count, type, indices, instancecount);
    else if (pPlain) pPlain(mode, count, type, indices);
}

// --- Occlusion queries (GL 1.5) — GLES3 has them, forward if available ---
GLP_EXT_FORWARD_VOID(GenQueries,    (GLsizei n, GLuint* ids),               (n, ids))
GLP_EXT_FORWARD_VOID(DeleteQueries, (GLsizei n, const GLuint* ids),         (n, ids))
GLP_EXT_FORWARD(GLboolean, IsQuery, (GLuint id),                             (id))
GLP_EXT_FORWARD_VOID(BeginQuery,    (GLenum target, GLuint id),              (target, id))
GLP_EXT_FORWARD_VOID(EndQuery,      (GLenum target),                         (target))
GLP_EXT_FORWARD_VOID(GetQueryiv,    (GLenum target, GLenum pname, GLint* params), (target, pname, params))
GLP_EXT_FORWARD_VOID(GetQueryObjectiv,  (GLuint id, GLenum pname, GLint* params),    (id, pname, params))
GLP_EXT_FORWARD_VOID(GetQueryObjectuiv, (GLuint id, GLenum pname, GLuint* params),   (id, pname, params))

// --- Sync objects (GL 3.2 / GLES 3.0) ---
typedef void* GLsync_t;
typedef void* (WINAPI *PFN_FS)(GLenum, GLbitfield);
extern "C" __declspec(dllexport) GLsync_t WINAPI gl_glFenceSync(GLenum condition, GLbitfield flags) {
    static PFN_FS p = nullptr;
    if (!p) p = (PFN_FS)glproxy::resolve("glFenceSync");
    return p ? p(condition, flags) : nullptr;
}
typedef GLenum (WINAPI *PFN_CWS)(GLsync_t, GLbitfield, GLuint64);
extern "C" __declspec(dllexport) GLenum WINAPI gl_glClientWaitSync(GLsync_t sync, GLbitfield flags, GLuint64 timeout) {
    static PFN_CWS p = nullptr;
    if (!p) p = (PFN_CWS)glproxy::resolve("glClientWaitSync");
    return p ? p(sync, flags, timeout) : 0x911A /*GL_WAIT_FAILED*/;
}
typedef void (WINAPI *PFN_WS)(GLsync_t, GLbitfield, GLuint64);
extern "C" __declspec(dllexport) void WINAPI gl_glWaitSync(GLsync_t sync, GLbitfield flags, GLuint64 timeout) {
    static PFN_WS p = nullptr;
    if (!p) p = (PFN_WS)glproxy::resolve("glWaitSync");
    if (p) p(sync, flags, timeout);
}
typedef void (WINAPI *PFN_DS)(GLsync_t);
extern "C" __declspec(dllexport) void WINAPI gl_glDeleteSync(GLsync_t sync) {
    static PFN_DS p = nullptr;
    if (!p) p = (PFN_DS)glproxy::resolve("glDeleteSync");
    if (p) p(sync);
}
typedef GLboolean (WINAPI *PFN_IS)(GLsync_t);
extern "C" __declspec(dllexport) GLboolean WINAPI gl_glIsSync(GLsync_t sync) {
    static PFN_IS p = nullptr;
    if (!p) p = (PFN_IS)glproxy::resolve("glIsSync");
    return p ? p(sync) : GL_FALSE;
}

// --- Uniform buffer objects (GL 3.1 / GLES 3.0) ---
GLP_EXT_FORWARD(GLuint, GetUniformBlockIndex, (GLuint program, const GLchar* name), (program, name))
GLP_EXT_FORWARD_VOID(UniformBlockBinding, (GLuint program, GLuint index, GLuint binding), (program, index, binding))
GLP_EXT_FORWARD_VOID(GetActiveUniformBlockiv, (GLuint program, GLuint index, GLenum pname, GLint* params), (program, index, pname, params))
GLP_EXT_FORWARD_VOID(GetActiveUniformBlockName, (GLuint program, GLuint index, GLsizei bufSize, GLsizei* length, GLchar* name), (program, index, bufSize, length, name))
GLP_EXT_FORWARD_VOID(BindBufferBase,   (GLenum target, GLuint index, GLuint buffer),                                           (target, index, buffer))
GLP_EXT_FORWARD_VOID(BindBufferRange,  (GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size),         (target, index, buffer, offset, size))

// --- Program binary (GL 4.1 / GLES 3.0) ---
GLP_EXT_FORWARD_VOID(GetProgramBinary, (GLuint program, GLsizei bufSize, GLsizei* length, GLenum* binaryFormat, void* binary), (program, bufSize, length, binaryFormat, binary))
GLP_EXT_FORWARD_VOID(ProgramBinary,    (GLuint program, GLenum binaryFormat, const void* binary, GLsizei length),              (program, binaryFormat, binary, length))
GLP_EXT_FORWARD_VOID(ProgramParameteri,(GLuint program, GLenum pname, GLint value),                                            (program, pname, value))

// --- Vertex attrib integer / double (GL 3.0+) ---
GLP_EXT_FORWARD_VOID(VertexAttribIPointer, (GLuint index, GLint size, GLenum type, GLsizei stride, const void* pointer), (index, size, type, stride, pointer))
GLP_EXT_FORWARD_VOID(VertexAttribDivisor,  (GLuint index, GLuint divisor),                                               (index, divisor))
GLP_EXT_FORWARD_VOID(GetVertexAttribIiv,   (GLuint index, GLenum pname, GLint* params),                                  (index, pname, params))
GLP_EXT_FORWARD_VOID(GetVertexAttribIuiv,  (GLuint index, GLenum pname, GLuint* params),                                 (index, pname, params))

// --- Integer uniforms (GL 3.0 / GLES 3.0) ---
GLP_EXT_FORWARD_VOID(Uniform1ui,  (GLint loc, GLuint v0),                                (loc, v0))
GLP_EXT_FORWARD_VOID(Uniform2ui,  (GLint loc, GLuint v0, GLuint v1),                     (loc, v0, v1))
GLP_EXT_FORWARD_VOID(Uniform3ui,  (GLint loc, GLuint v0, GLuint v1, GLuint v2),          (loc, v0, v1, v2))
GLP_EXT_FORWARD_VOID(Uniform4ui,  (GLint loc, GLuint v0, GLuint v1, GLuint v2, GLuint v3), (loc, v0, v1, v2, v3))
GLP_EXT_FORWARD_VOID(Uniform1uiv, (GLint loc, GLsizei count, const GLuint* v),           (loc, count, v))
GLP_EXT_FORWARD_VOID(Uniform2uiv, (GLint loc, GLsizei count, const GLuint* v),           (loc, count, v))
GLP_EXT_FORWARD_VOID(Uniform3uiv, (GLint loc, GLsizei count, const GLuint* v),           (loc, count, v))
GLP_EXT_FORWARD_VOID(Uniform4uiv, (GLint loc, GLsizei count, const GLuint* v),           (loc, count, v))
GLP_EXT_FORWARD_VOID(UniformMatrix2x3fv, (GLint loc, GLsizei count, GLboolean t, const GLfloat* v), (loc, count, t, v))
GLP_EXT_FORWARD_VOID(UniformMatrix3x2fv, (GLint loc, GLsizei count, GLboolean t, const GLfloat* v), (loc, count, t, v))
GLP_EXT_FORWARD_VOID(UniformMatrix2x4fv, (GLint loc, GLsizei count, GLboolean t, const GLfloat* v), (loc, count, t, v))
GLP_EXT_FORWARD_VOID(UniformMatrix4x2fv, (GLint loc, GLsizei count, GLboolean t, const GLfloat* v), (loc, count, t, v))
GLP_EXT_FORWARD_VOID(UniformMatrix3x4fv, (GLint loc, GLsizei count, GLboolean t, const GLfloat* v), (loc, count, t, v))
GLP_EXT_FORWARD_VOID(UniformMatrix4x3fv, (GLint loc, GLsizei count, GLboolean t, const GLfloat* v), (loc, count, t, v))

// --- Sampler objects (GL 3.3 / GLES 3.0) ---
GLP_EXT_FORWARD_VOID(GenSamplers,         (GLsizei count, GLuint* samplers),                          (count, samplers))
GLP_EXT_FORWARD_VOID(DeleteSamplers,      (GLsizei count, const GLuint* samplers),                    (count, samplers))
GLP_EXT_FORWARD(GLboolean, IsSampler,     (GLuint sampler),                                           (sampler))
GLP_EXT_FORWARD_VOID(SamplerParameteri,   (GLuint sampler, GLenum pname, GLint param),                (sampler, pname, param))
GLP_EXT_FORWARD_VOID(SamplerParameterf,   (GLuint sampler, GLenum pname, GLfloat param),              (sampler, pname, param))
GLP_EXT_FORWARD_VOID(SamplerParameteriv,  (GLuint sampler, GLenum pname, const GLint* params),        (sampler, pname, params))
GLP_EXT_FORWARD_VOID(SamplerParameterfv,  (GLuint sampler, GLenum pname, const GLfloat* params),      (sampler, pname, params))
GLP_EXT_FORWARD_VOID(GetSamplerParameteriv,  (GLuint sampler, GLenum pname, GLint* params),           (sampler, pname, params))
GLP_EXT_FORWARD_VOID(GetSamplerParameterfv,  (GLuint sampler, GLenum pname, GLfloat* params),         (sampler, pname, params))

// --- Texture storage (GL 4.2 / GLES 3.0) ---
GLP_EXT_FORWARD_VOID(TexStorage2D,   (GLenum target, GLsizei levels, GLenum ifmt, GLsizei w, GLsizei h),                  (target, levels, ifmt, w, h))
GLP_EXT_FORWARD_VOID(TexStorage3D,   (GLenum target, GLsizei levels, GLenum ifmt, GLsizei w, GLsizei h, GLsizei d),       (target, levels, ifmt, w, h, d))
GLP_EXT_FORWARD_VOID(TexSubImage3D,  (GLenum t, GLint l, GLint xo, GLint yo, GLint zo, GLsizei w, GLsizei h, GLsizei d, GLenum fmt, GLenum type, const GLvoid* px), (t, l, xo, yo, zo, w, h, d, fmt, type, px))
GLP_EXT_FORWARD_VOID(TexImage2DMultisample, (GLenum target, GLsizei samples, GLenum ifmt, GLsizei w, GLsizei h, GLboolean fixedLoc), (target, samples, ifmt, w, h, fixedLoc))

// --- Get double/int64 (GL 3.2+) ---
extern "C" __declspec(dllexport) void WINAPI gl_glGetDoublev(GLenum pname, GLdouble* data) {
    typedef void (WINAPI *PFN)(GLenum, GLfloat*);
    static PFN p = nullptr;
    if (!p) p = (PFN)glproxy::resolve("glGetFloatv");
    if (!p || !data) return;
    GLfloat tmp = 0.0f;
    p(pname, &tmp);
    *data = (GLdouble)tmp;
}
extern "C" __declspec(dllexport) void WINAPI gl_glGetInteger64v(GLenum pname, GLint64* data) {
    typedef void (WINAPI *PFN)(GLenum, GLint*);
    static PFN p = nullptr;
    if (!p) p = (PFN)glproxy::resolve("glGetIntegerv");
    if (!p || !data) return;
    GLint tmp = 0;
    p(pname, &tmp);
    *data = (GLint64)tmp;
}

// --- Misc desktop-only ---
extern "C" __declspec(dllexport) void WINAPI gl_glPassThrough(GLfloat /*token*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glSelectBuffer(GLsizei /*size*/, GLuint* /*buffer*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glFeedbackBuffer(GLsizei /*size*/, GLenum /*type*/, GLfloat* /*buffer*/) {}
extern "C" __declspec(dllexport) GLint WINAPI gl_glRenderMode(GLenum /*mode*/) { return 0; }
extern "C" __declspec(dllexport) void WINAPI gl_glInitNames() {}
extern "C" __declspec(dllexport) void WINAPI gl_glPushName(GLuint /*name*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glPopName() {}
extern "C" __declspec(dllexport) void WINAPI gl_glLoadName(GLuint /*name*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glPointSize(GLfloat /*size*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glLineStipple(GLint /*factor*/, GLushort /*pattern*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glEdgeFlag(GLboolean /*flag*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glClipPlane(GLenum /*plane*/, const GLdouble* /*eq*/) {}
extern "C" __declspec(dllexport) void WINAPI gl_glGetClipPlane(GLenum /*plane*/, GLdouble* /*eq*/) {}