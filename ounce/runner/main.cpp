// SPDX-License-Identifier: GPL-3.0-or-later
#include <algorithm>
#include <array>
#include <cstdarg>
#include <cstdio>
#include <switch.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>

#include "Platform.h"
#include "Config.h"
#include "NDS.h"
#include "GPU.h"

static FILE* logFile;
static void Log(const char* format, ...) {
    va_list args;
    va_start(args, format);
    std::vfprintf(logFile, format, args);
    va_end(args);
    std::fputc('\n', logFile);
    std::fflush(logFile);
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
        const EGLint attributes[] = {
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
            EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
            EGL_NONE
        };
        EGLConfig config;
        EGLint count = 0;
        if (!EglCheck(eglChooseConfig(display, attributes, &config, 1, &count), "eglChooseConfig")) return false;
        if (!count) {
            Log("FAIL: no matching EGL config");
            return false;
        }
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
    glShaderSource(shader, 1, &text, nullptr);
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char info[2048] = {};
        glGetShaderInfoLog(shader, sizeof(info), nullptr, info);
        Log("FAIL: shader %u: %s", type, info);
        glDeleteShader(shader);
        return 0;
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
        static const char vs[] =
            "#version 300 es\n"
            "const vec2 p[4] = vec2[4](vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(-1.0, 1.0), vec2(1.0, 1.0));\n"
            "out vec2 uv;\n"
            "void main() {\n"
            "    vec2 v = p[gl_VertexID];\n"
            "    gl_Position = vec4(v, 0.0, 1.0);\n"
            "    uv = vec2((v.x + 1.0) * 0.5, 1.0 - (v.y + 1.0) * 0.5);\n"
            "}\n";

        static const char fs[] =
            "#version 300 es\n"
            "precision mediump float;\n"
            "in vec2 uv;\n"
            "uniform sampler2D image;\n"
            "out vec4 color;\n"
            "void main() {\n"
            "    color = texture(image, uv);\n"
            "}\n";

        GLuint vertex = Shader(GL_VERTEX_SHADER, vs);
        GLuint fragment = Shader(GL_FRAGMENT_SHADER, fs);
        if (!vertex || !fragment) {
            if (vertex) glDeleteShader(vertex);
            if (fragment) glDeleteShader(fragment);
            return false;
        }
        program = glCreateProgram();
        glAttachShader(program, vertex);
        glAttachShader(program, fragment);
        glLinkProgram(program);
        glDeleteShader(vertex);
        glDeleteShader(fragment);
        GLint ok = 0;
        glGetProgramiv(program, GL_LINK_STATUS, &ok);
        if (!ok) {
            char info[2048] = {};
            glGetProgramInfoLog(program, sizeof(info), nullptr, info);
            Log("FAIL: program link: %s", info);
            return false;
        }
        glUseProgram(program);
        glUniform1i(glGetUniformLocation(program, "image"), 0);
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        glActiveTexture(GL_TEXTURE0);
        glGenTextures(2, textures);
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

static void ConvertBgr5ToRgba8(const u32* src, u32* dst) {
    for (int i = 0; i < 256 * 192; ++i) {
        u32 c = src[i];
        u32 r = ((c >> 0)  & 0x1F) << 3;
        u32 g = ((c >> 5)  & 0x1F) << 3;
        u32 b = ((c >> 10) & 0x1F) << 3;
        dst[i] = r | (g << 8) | (b << 16) | 0xFF000000;
    }
}

static u32 MapPadToDs(u64 buttons) {
    u32 ds = 0xFFF;
    if (buttons & HidNpadButton_A)     ds &= ~(1 << 0);
    if (buttons & HidNpadButton_B)     ds &= ~(1 << 1);
    if (buttons & HidNpadButton_Minus) ds &= ~(1 << 2);
    if (buttons & HidNpadButton_Plus)  ds &= ~(1 << 3);
    if (buttons & HidNpadButton_Right) ds &= ~(1 << 4);
    if (buttons & HidNpadButton_Left)  ds &= ~(1 << 5);
    if (buttons & HidNpadButton_Up)    ds &= ~(1 << 6);
    if (buttons & HidNpadButton_Down)  ds &= ~(1 << 7);
    if (buttons & HidNpadButton_R)     ds &= ~(1 << 8);
    if (buttons & HidNpadButton_L)     ds &= ~(1 << 9);
    if (buttons & HidNpadButton_X)     ds &= ~(1 << 10);
    if (buttons & HidNpadButton_Y)     ds &= ~(1 << 11);
    return ds;
}

static bool Run(const char* romPath) {
    Window window;
    if (!window.Open()) return false;
    Graphics graphics;
    if (!graphics.Open()) return false;

    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);
    hidInitializeTouchScreen();

    NDS::SetConsoleType(0);
    if (!NDS::Init()) {
        Log("FAIL: NDS::Init");
        return false;
    }
    Log("PASS: NDS::Init");

    GPU::RenderSettings settings{};
    settings.Soft_Threaded = false;
    GPU::SetRenderSettings(0, settings);

    char sramPath[2048];
    std::snprintf(sramPath, sizeof(sramPath), "%s.sav", romPath);

    Log("LOADING: %s (direct boot)", romPath);
    if (!NDS::LoadROM(romPath, sramPath, true)) {
        Log("FAIL: LoadROM failed for %s", romPath);
        NDS::DeInit();
        return false;
    }
    Log("PASS: LoadROM succeeded; starting emulation loop");

    std::array<u32, 256 * 192> rgba[2];
    unsigned frame = 0;

    while (appletMainLoop()) {
        padUpdate(&pad);
        u64 down = padGetButtonsDown(&pad);
        u64 held = padGetButtons(&pad);

        if ((down & HidNpadButton_Plus) && (down & HidNpadButton_Minus)) {
            Log("EXIT: Plus+Minus combo pressed");
            break;
        }

        NDS::SetKeyMask(MapPadToDs(held));

        HidTouchScreenState touchState = {0};
        if (hidGetTouchScreenStates(&touchState, 1) > 0 && touchState.count > 0) {
            EGLint width = 0, height = 0;
            eglQuerySurface(window.display, window.surface, EGL_WIDTH, &width);
            eglQuerySurface(window.display, window.surface, EGL_HEIGHT, &height);

            int scale = std::min(width / 256, (height - 16) / 384);
            if (scale >= 1) {
                int w = 256 * scale, h = 192 * scale;
                int left = (width - w) / 2;
                int bottomGl = (height - (2 * h + 16)) / 2;
                int topScreenY = height - (bottomGl + h);

                s32 tx = touchState.touches[0].x;
                s32 ty = touchState.touches[0].y;

                if (tx >= left && tx < left + w && ty >= topScreenY && ty < topScreenY + h) {
                    u16 dsX = (tx - left) / scale;
                    u16 dsY = (ty - topScreenY) / scale;
                    NDS::TouchScreen(dsX, dsY);
                } else {
                    NDS::ReleaseScreen();
                }
            } else {
                NDS::ReleaseScreen();
            }
        } else {
            NDS::ReleaseScreen();
        }

        NDS::RunFrame();

        int front = GPU::FrontBuffer;
        const u32* fbTop = GPU::Framebuffer[front][0];
        const u32* fbBot = GPU::Framebuffer[front][1];

        if (fbTop && fbBot) {
            ConvertBgr5ToRgba8(fbTop, rgba[0].data());
            ConvertBgr5ToRgba8(fbBot, rgba[1].data());
        }

        EGLint width = 0, height = 0;
        eglQuerySurface(window.display, window.surface, EGL_WIDTH, &width);
        eglQuerySurface(window.display, window.surface, EGL_HEIGHT, &height);

        int scale = std::min(width / 256, (height - 16) / 384);
        if (scale >= 1) {
            int w = 256 * scale, h = 192 * scale;
            int left = (width - w) / 2, bottom = (height - (2 * h + 16)) / 2;

            glDisable(GL_SCISSOR_TEST);
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_BLEND);
            glClearColor(0.025f, 0.025f, 0.04f, 1);
            glClear(GL_COLOR_BUFFER_BIT);
            glUseProgram(graphics.program);
            glBindVertexArray(graphics.vao);
            glActiveTexture(GL_TEXTURE0);

            for (int s = 0; s < 2; ++s) {
                glBindTexture(GL_TEXTURE_2D, graphics.textures[s]);
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 192, GL_RGBA, GL_UNSIGNED_BYTE, rgba[s].data());
                glViewport(left, s == 0 ? bottom + h + 16 : bottom, w, h);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            }
            eglSwapBuffers(window.display, window.surface);
        }

        ++frame;
        if (frame == 1 || frame == 60 || frame == 300) {
            Log("PRESENTED: frame=%u", frame);
        }
    }

    Log("STOPPING: frames executed=%u", frame);
    NDS::DeInit();
    return frame > 0;
}

int main(int argc, char** argv) {
    logFile = std::fopen("sdmc:/melonds-ounce-runner.log", "w");
    if (!logFile) return 1;
    Log("START: melonDS Ounce interactive ROM runner");

    Platform::Init(argc, argv);

    const char* rom = "sdmc:/switch/melonds-ounce/boot.nds";
    if (argc > 1 && argv[1] && argv[1][0] != '\0') {
        rom = argv[1];
    }
    Log("ROM path: %s", rom);

    bool ok = Run(rom);
    Platform::DeInit();
    Log(ok ? "COMPLETE: runner exited cleanly" : "FAIL: runner terminated with error");
    std::fclose(logFile);
    return ok ? 0 : 2;
}
