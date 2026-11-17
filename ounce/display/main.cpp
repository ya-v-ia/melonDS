// SPDX-License-Identifier: GPL-3.0-or-later
#include <algorithm>
#include <array>
#include <cstdarg>
#include <cstdio>
#include <switch.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>

static FILE* logFile;
static void Log(const char* format, ...) {
    va_list args; va_start(args, format);
    std::vfprintf(logFile, format, args); va_end(args);
    std::fputc('\n', logFile); std::fflush(logFile);
}
static bool EglCheck(bool ok, const char* step) {
    if (!ok) Log("FAIL: %s EGL=0x%x", step, eglGetError());
    return ok;
}
struct Window {
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLSurface surface = EGL_NO_SURFACE;
    EGLContext context = EGL_NO_CONTEXT;
    bool initialized = false;
    bool Open() {
        display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (!EglCheck(display != EGL_NO_DISPLAY, "eglGetDisplay")) return false;
        if (!EglCheck(eglInitialize(display, nullptr, nullptr), "eglInitialize")) return false;
        initialized = true;
        if (!EglCheck(eglBindAPI(EGL_OPENGL_ES_API), "eglBindAPI")) return false;
        const EGLint attributes[] = {EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_NONE};
        EGLConfig config; EGLint count = 0;
        if (!EglCheck(eglChooseConfig(display, attributes, &config, 1, &count), "eglChooseConfig")) return false;
        if (!count) { Log("FAIL: no matching EGL config"); return false; }
        surface = eglCreateWindowSurface(display, config, nwindowGetDefault(), nullptr);
        if (!EglCheck(surface != EGL_NO_SURFACE, "eglCreateWindowSurface")) return false;
        const EGLint contextAttributes[] = {EGL_CONTEXT_MAJOR_VERSION, 3, EGL_NONE};
        context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttributes);
        if (!EglCheck(context != EGL_NO_CONTEXT, "eglCreateContext")) return false;
        if (!EglCheck(eglMakeCurrent(display, surface, surface, context), "eglMakeCurrent")) return false;
        if (!eglSwapInterval(display, 1)) Log("WARNING: swap interval EGL=0x%x", eglGetError());
        Log("GL_RENDERER: %s", reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
        Log("GL_VERSION: %s", reinterpret_cast<const char*>(glGetString(GL_VERSION)));
        return true;
    }
    ~Window() {
        if (!initialized) return;
        EglCheck(eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT), "unbind context");
        if (context != EGL_NO_CONTEXT) EglCheck(eglDestroyContext(display, context), "destroy context");
        if (surface != EGL_NO_SURFACE) EglCheck(eglDestroySurface(display, surface), "destroy surface");
        EglCheck(eglTerminate(display), "eglTerminate");
    }
};
static GLuint Shader(GLenum type, const char* text) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &text, nullptr); glCompileShader(shader);
    GLint ok = 0; glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char info[2048] = {}; glGetShaderInfoLog(shader, sizeof(info), nullptr, info);
        Log("FAIL: shader %u: %s", type, info); glDeleteShader(shader); return 0;
    }
    return shader;
}
struct Graphics {
    GLuint program = 0, vao = 0, textures[2] = {};
    ~Graphics() {
        glDeleteTextures(2, textures);
        if (vao) glDeleteVertexArrays(1, &vao);
        if (program) glDeleteProgram(program);
    }
    bool Open() {
        const char* vs = "#version 300 es\nconst vec2 p[4]=vec2[4](vec2(-1,-1),vec2(1,-1),vec2(-1,1),vec2(1,1));out vec2 uv;void main(){vec2 v=p[gl_VertexID];gl_Position=vec4(v,0,1);uv=vec2((v.x+1.0)*0.5,1.0-(v.y+1.0)*0.5);}";
        const char* fs = "#version 300 es\nprecision mediump float;in vec2 uv;uniform sampler2D image;out vec4 color;void main(){color=texture(image,uv);}";
        GLuint vertex = Shader(GL_VERTEX_SHADER, vs);
        GLuint fragment = Shader(GL_FRAGMENT_SHADER, fs);
        if (!vertex || !fragment) { if (vertex) glDeleteShader(vertex); if (fragment) glDeleteShader(fragment); return false; }
        program = glCreateProgram(); glAttachShader(program, vertex); glAttachShader(program, fragment);
        glLinkProgram(program); glDeleteShader(vertex); glDeleteShader(fragment);
        GLint ok = 0; glGetProgramiv(program, GL_LINK_STATUS, &ok);
        if (!ok) {
            char info[2048] = {}; glGetProgramInfoLog(program, sizeof(info), nullptr, info);
            Log("FAIL: program link: %s", info); return false;
        }
        glUseProgram(program); glUniform1i(glGetUniformLocation(program, "image"), 0);
        glGenVertexArrays(1, &vao); glBindVertexArray(vao);
        glActiveTexture(GL_TEXTURE0); glGenTextures(2, textures);
        for (GLuint texture : textures) {
            glBindTexture(GL_TEXTURE_2D, texture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 256, 192, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        }
        return true;
    }
};
using Pixels = std::array<unsigned char, 256 * 192 * 4>;
static void Pattern(Pixels& pixels, bool bottom, unsigned frame) {
    static const unsigned char bars[4][3] = {{255,0,0},{0,255,0},{0,0,255},{255,255,255}};
    for (int y = 0; y < 192; ++y) for (int x = 0; x < 256; ++x) {
        unsigned char r, g, b;
        if (bottom) { bool check = ((x / 16) ^ (y / 16)) & 1; r = check ? 255 : 0; g = check ? 0 : 255; b = 255; }
        else { r = bars[x / 64][0]; g = bars[x / 64][1]; b = bars[x / 64][2]; }
        if (y < 16 && x < 16) { r = 255; g = 255; b = 0; }
        if (y >= 24 && x == static_cast<int>(frame % 256)) r = g = b = 255;
        size_t i = static_cast<size_t>(y * 256 + x) * 4;
        pixels[i] = r; pixels[i+1] = g; pixels[i+2] = b; pixels[i+3] = 255;
    }
}
static bool Run() {
    Window window; if (!window.Open()) return false;
    Graphics graphics; if (!graphics.Open()) return false;
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad; padInitializeDefault(&pad);
    Pixels pixels{}; unsigned frame = 0;
    Log("READY: top=color bars; bottom=cyan/magenta checker; yellow upper-left markers; Plus exits");
    while (appletMainLoop()) {
        padUpdate(&pad);
        if (padGetButtonsDown(&pad) & HidNpadButton_Plus) { Log("EXIT: Plus pressed"); break; }
        EGLint width = 0, height = 0;
        if (!EglCheck(eglQuerySurface(window.display, window.surface, EGL_WIDTH, &width), "query width") ||
            !EglCheck(eglQuerySurface(window.display, window.surface, EGL_HEIGHT, &height), "query height")) return false;
        if (width <= 0 || height <= 16) { Log("FAIL: invalid surface size %dx%d", width, height); return false; }
        int scale = std::min(width / 256, (height - 16) / 384);
        if (scale < 1) { Log("FAIL: surface too small for integer layout"); return false; }
        int w = 256 * scale, h = 192 * scale;
        int left = (width - w) / 2, bottom = (height - (2 * h + 16)) / 2;
        glDisable(GL_SCISSOR_TEST); glDisable(GL_DEPTH_TEST); glDisable(GL_BLEND);
        glClearColor(0.025f, 0.025f, 0.04f, 1); glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(graphics.program); glBindVertexArray(graphics.vao); glActiveTexture(GL_TEXTURE0);
        for (int screen = 0; screen < 2; ++screen) {
            Pattern(pixels, screen == 1, frame);
            glBindTexture(GL_TEXTURE_2D, graphics.textures[screen]);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 192, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            glViewport(left, screen == 0 ? bottom + h + 16 : bottom, w, h);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        }
        GLenum error = glGetError();
        if (error != GL_NO_ERROR) { Log("FAIL: draw frame=%u GL=0x%x", frame, error); return false; }
        if (!EglCheck(eglSwapBuffers(window.display, window.surface), "eglSwapBuffers")) return false;
        ++frame;
        if (frame == 1 || frame == 120) Log("PRESENTED: %u frames; surface=%dx%d scale=%d", frame, width, height, scale);
    }
    Log("END LOOP: %u frames; visual confirmation still required", frame);
    return frame > 0;
}
int main() {
    logFile = std::fopen("sdmc:/melonds-ounce-display.log", "w");
    if (!logFile) return 1;
    Log("START: standalone Ounce display test; no emulation core or ROM loaded");
    bool ok = Run();
    Log(ok ? "COMPLETE: presentation loop exited; inspect any cleanup errors and confirm image visually" : "FAIL: display test did not complete");
    std::fclose(logFile); return ok ? 0 : 2;
}
