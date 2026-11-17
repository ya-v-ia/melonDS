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
    va_list args; va_start(args, format);
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
        const char* vs = "#version 300 es\nconst vec2 p[4]=vec2[4](vec2(-1,-1),vec2(1,-1),vec2(-1,1),vec2(1,1));out vec2 uv;void main(){vec2 v=p[gl_VertexID];gl_Position=vec4(v,0,1);uv=vec2((v.x+1.0)*0.5,1.0-(v.y+1.0)*0.5);}\";\n        const char* fs = \"#version 300 es\nprecision mediump float;in vec2 uv;uniform sampler2D image;out vec4 color;void main(){color=texture(image,uv);}\";\n        GLuint vertex = Shader(GL_VERTEX_SHADER, vs);\n        GLuint fragment = Shader(GL_FRAGMENT_SHADER, fs);\n        if (!vertex || !fragment) {\n            if (vertex) glDeleteShader(vertex);\n            if (fragment) glDeleteShader(fragment);\n            return false;\n        }\n        program = glCreateProgram();\n        glAttachShader(program, vertex);\n        glAttachShader(program, fragment);\n        glLinkProgram(program);\n        glDeleteShader(vertex);\n        glDeleteShader(fragment);\n        GLint ok = 0;\n        glGetProgramiv(program, GL_LINK_STATUS, &ok);\n        if (!ok) {\n            char info[2048] = {};\n            glGetProgramInfoLog(program, sizeof(info), nullptr, info);\n            Log(\"FAIL: program link: %s\", info);\n            return false;\n        }\n        glUseProgram(program);\n        glUniform1i(glGetUniformLocation(program, \"image\"), 0);\n        glGenVertexArrays(1, &vao);\n        glBindVertexArray(vao);\n        glActiveTexture(GL_TEXTURE0);\n        glGenTextures(2, textures);\n        for (GLuint texture : textures) {\n            glBindTexture(GL_TEXTURE_2D, texture);\n            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);\n            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);\n            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);\n            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);\n            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 256, 192, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);\n        }\n        return true;\n    }\n};\n\nstatic void ConvertBgr5ToRgba8(const u32* src, u32* dst) {\n    for (int i = 0; i < 256 * 192; ++i) {\n        u32 c = src[i];\n        u32 r = ((c >> 0)  & 0x1F) << 3;\n        u32 g = ((c >> 5)  & 0x1F) << 3;\n        u32 b = ((c >> 10) & 0x1F) << 3;\n        dst[i] = r | (g << 8) | (b << 16) | 0xFF000000;\n    }\n}\n\nstatic u32 MapPadToDs(u64 buttons) {\n    u32 ds = 0xFFF;\n    if (buttons & HidNpadButton_A)     ds &= ~(1 << 0);\n    if (buttons & HidNpadButton_B)     ds &= ~(1 << 1);\n    if (buttons & HidNpadButton_Minus) ds &= ~(1 << 2);\n    if (buttons & HidNpadButton_Plus)  ds &= ~(1 << 3);\n    if (buttons & HidNpadButton_Right) ds &= ~(1 << 4);\n    if (buttons & HidNpadButton_Left)  ds &= ~(1 << 5);\n    if (buttons & HidNpadButton_Up)    ds &= ~(1 << 6);\n    if (buttons & HidNpadButton_Down)  ds &= ~(1 << 7);\n    if (buttons & HidNpadButton_R)     ds &= ~(1 << 8);\n    if (buttons & HidNpadButton_L)     ds &= ~(1 << 9);\n    if (buttons & HidNpadButton_X)     ds &= ~(1 << 10);\n    if (buttons & HidNpadButton_Y)     ds &= ~(1 << 11);\n    return ds;\n}\n\nstatic bool Run(const char* romPath) {\n    Window window;\n    if (!window.Open()) return false;\n    Graphics graphics;\n    if (!graphics.Open()) return false;\n\n    padConfigureInput(1, HidNpadStyleSet_NpadStandard);\n    PadState pad;\n    padInitializeDefault(&pad);\n    hidInitializeTouchScreen();\n\n    NDS::SetConsoleType(0);\n    if (!NDS::Init()) {\n        Log(\"FAIL: NDS::Init\");\n        return false;\n    }\n    Log(\"PASS: NDS::Init\");\n\n    GPU::RenderSettings settings{};\n    settings.Soft_Threaded = false;\n    GPU::SetRenderSettings(0, settings);\n\n    char sramPath[2048];\n    std::snprintf(sramPath, sizeof(sramPath), \"%s.sav\", romPath);\n\n    Log(\"LOADING: %s (direct boot)\", romPath);\n    if (!NDS::LoadROM(romPath, sramPath, true)) {\n        Log(\"FAIL: LoadROM failed for %s\", romPath);\n        NDS::DeInit();\n        return false;\n    }\n    Log(\"PASS: LoadROM succeeded; starting emulation loop\");\n\n    std::array<u32, 256 * 192> rgba[2];\n    unsigned frame = 0;\n\n    while (appletMainLoop()) {\n        padUpdate(&pad);\n        u64 down = padGetButtonsDown(&pad);\n        u64 held = padGetButtons(&pad);\n\n        if ((down & HidNpadButton_Plus) && (down & HidNpadButton_Minus)) {\n            Log(\"EXIT: Plus+Minus combo pressed\");\n            break;\n        }\n\n        NDS::SetKeyMask(MapPadToDs(held));\n\n        HidTouchScreenState touchState = {0};\n        if (hidGetTouchScreenStates(&touchState, 1) > 0 && touchState.count > 0) {\n            EGLint width = 0, height = 0;\n            eglQuerySurface(window.display, window.surface, EGL_WIDTH, &width);\n            eglQuerySurface(window.display, window.surface, EGL_HEIGHT, &height);\n\n            int scale = std::min(width / 256, (height - 16) / 384);\n            if (scale >= 1) {\n                int w = 256 * scale, h = 192 * scale;\n                int left = (width - w) / 2;\n                int bottomGl = (height - (2 * h + 16)) / 2;\n                int topScreenY = height - (bottomGl + h);\n\n                s32 tx = touchState.touches[0].x;\n                s32 ty = touchState.touches[0].y;\n\n                if (tx >= left && tx < left + w && ty >= topScreenY && ty < topScreenY + h) {\n                    u16 dsX = (tx - left) / scale;\n                    u16 dsY = (ty - topScreenY) / scale;\n                    NDS::TouchScreen(dsX, dsY);\n                } else {\n                    NDS::ReleaseScreen();\n                }\n            } else {\n                NDS::ReleaseScreen();\n            }\n        } else {\n            NDS::ReleaseScreen();\n        }\n\n        NDS::RunFrame();\n\n        int front = GPU::FrontBuffer;\n        const u32* fbTop = GPU::Framebuffer[front][0];\n        const u32* fbBot = GPU::Framebuffer[front][1];\n\n        if (fbTop && fbBot) {\n            ConvertBgr5ToRgba8(fbTop, rgba[0].data());\n            ConvertBgr5ToRgba8(fbBot, rgba[1].data());\n        }\n\n        EGLint width = 0, height = 0;\n        eglQuerySurface(window.display, window.surface, EGL_WIDTH, &width);\n        eglQuerySurface(window.display, window.surface, EGL_HEIGHT, &height);\n\n            int scale = std::min(width / 256, (height - 16) / 384);\n            if (scale >= 1) {\n                int w = 256 * scale, h = 192 * scale;\n                int left = (width - w) / 2, bottom = (height - (2 * h + 16)) / 2;\n\n                glDisable(GL_SCISSOR_TEST);\n                glDisable(GL_DEPTH_TEST);\n                glDisable(GL_BLEND);\n                glClearColor(0.025f, 0.025f, 0.04f, 1);\n                glClear(GL_COLOR_BUFFER_BIT);\n                glUseProgram(graphics.program);\n                glBindVertexArray(graphics.vao);\n                glActiveTexture(GL_TEXTURE0);\n\n                for (int s = 0; s < 2; ++s) {\n                    glBindTexture(GL_TEXTURE_2D, graphics.textures[s]);\n                    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 192, GL_RGBA, GL_UNSIGNED_BYTE, rgba[s].data());\n                    glViewport(left, s == 0 ? bottom + h + 16 : bottom, w, h);\n                    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);\n                }\n                eglSwapBuffers(window.display, window.surface);\n            }\n\n        ++frame;\n        if (frame == 1 || frame == 60 || frame == 300) {\n            Log(\"PRESENTED: frame=%u\", frame);\n        }\n    }\n\n    Log(\"STOPPING: frames executed=%u\", frame);\n    NDS::DeInit();\n    return frame > 0;\n}\n\nint main(int argc, char** argv) {\n    logFile = std::fopen(\"sdmc:/melonds-ounce-runner.log\", \"w\");\n    if (!logFile) return 1;\n    Log(\"START: melonDS Ounce interactive ROM runner\");\n\n    Platform::Init(argc, argv);\n\n    const char* rom = \"sdmc:/switch/melonds-ounce/boot.nds\";\n    if (argc > 1 && argv[1] && argv[1][0] != '\\0') {\n        rom = argv[1];\n    }\n    Log(\"ROM path: %s\", rom);\n\n    bool ok = Run(rom);\n    Platform::DeInit();\n    Log(ok ? \"COMPLETE: runner exited cleanly\" : \"FAIL: runner terminated with error\");\n    std::fclose(logFile);\n    return ok ? 0 : 2;\n}\n