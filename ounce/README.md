# melonDS Ounce bring-up

## Status

Experimental, uncompiled core-only build scaffold. This is NOT a working Switch 2 port, a release, or an NRO-producing target. No emulator frontend, platform-service implementation, audio output, input, screen presentation, or JIT integration is supplied by this directory. No build or hardware test has been performed. The original Switch build remains unchanged.

The purpose of this first step is to compile the existing emulation core independently of the original deko3d-based Switch frontend, exposing compiler and SDK incompatibilities before runtime integration. A successful static-library build does not resolve or validate the core's platform callbacks.

## Prerequisites

Install devkitPro and ounce-devkit following https://github.com/ouncebrew/ounce-devkit. Use the libounce, OunceVK and libkaihen package versions installed by that project. Record their versions with any test report; this scaffold does not pin an SDK release. Initialize this repository's submodules.

## Build attempt

From the repository root, in bash:

```sh
git submodule update --init --recursive
source "$DEVKITPRO/switch2vars.sh"
aarch64-none-elf-cmake -S ounce -B build-ounce-core -DCMAKE_BUILD_TYPE=Debug
cmake --build build-ounce-core --target core --parallel
```

Expected scope: static core and its Teakra dependency only. Compilation may fail on platform assumptions; attach the first error and compiler command when reporting failures. Do not attempt to launch the resulting archive on a console.

The build forcibly disables JIT, OpenGL, deko3d and the experimental NEON renderer. It bypasses the root CMakeLists and original Switch frontend, removes the legacy Linux librt dependency from the core target, and gives libounce headers priority for core compilation. It does not claim full platform isolation or SDK compatibility.

## Remaining implementation milestones

1. Build this target with the real Ounce toolchain; fix core compilation and document exact package versions.
2. Implement the Platform.h callbacks for filesystem, time, threading, synchronization and other required services using verified libounce APIs.
3. Add an executable frontend with ROM loading, software-framebuffer presentation, input, touch mapping, audio and clean shutdown. Evaluate SDL2, which ounce-devkit documents as a reusable devkitPro portlib.
4. Add the executable metadata and packaging using the actual Ounce toolchain helpers. Do not reuse legacy deko3d shaders or packaging assumptions without checking them.
5. Test ROM boot, save files, save states, audio timing, suspend/resume and exit on hardware. Use homebrew test ROMs or files you are authorized to use.
6. Inspect kaiHEN's libkaihen/JIT.md before adapting executable-memory allocation, permissions and cache synchronization for the AArch64 recompiler. Keep it disabled until validated.
7. Evaluate the existing OpenGL renderer against OunceVK; measure correctness and performance before enabling acceleration by default.

## Licensing

The original repository's LICENSE and copyright notices remain applicable. This scaffold does not change them.
