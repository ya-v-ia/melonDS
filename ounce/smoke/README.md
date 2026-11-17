# Ounce executable smoke test (experimental)

This adds an executable-link and packaging attempt after the core archive milestone. It is uncompiled and untested when introduced. It is not a playable emulator. The core-only instructions in ../README.md remain valid for the separate archive target.

## Build in the existing Docker image

Configure a NEW build directory, inside the container at /work:

```sh
source "$DEVKITPRO/switch2vars.sh"
aarch64-none-elf-cmake -S ounce/smoke -B build-ounce-smoke -DCMAKE_BUILD_TYPE=Debug
cmake --build build-ounce-smoke --target ounce-smoke-nro --parallel 1
```

Expected outputs, only if linking and packaging succeed:

- build-ounce-smoke/melonds-ounce-smoke.elf
- build-ounce-smoke/melonds-ounce-smoke.nro

The build uses the SDK toolchain, selects ounce_core or nx exclusively from libounce/lib, and invokes elf2nro without optional icon or NACP metadata, following the minimal packaging approach in ounce-devkit/examples/plugin-echo-client. SDK versions can differ; link failures must be investigated rather than resolved by mixing original libnx with libounce. Existing local fixes to GPU.cpp and GPU3D_Transform.cpp are still required.

## Runtime scope

Use an already working, authorized Ounce homebrew launcher/runtime. This document does not install a loader or establish firmware compatibility. Do not treat a produced NRO as proof of runtime support.

The executable has no screen UI. A blank screen followed by return to the launcher is not by itself a pass. It attempts to overwrite sdmc:/melonds-ounce-smoke.log, requires the runtime to mount SD for stdio, tests mutex/thread/semaphore callbacks, calls NDS::Init in DS mode, calls NDS::DeInit only after successful initialization, and returns. Read the log on SD. A pass requires its final line:

```text
PASS: core initialization and shutdown; no frames executed
```

No log means startup or SD logging failed and must not be interpreted as success. The last flushed line identifies the phase reached. A synchronization failure can hang the test. Native thread/condition-variable failures abort rather than silently continue. This is diagnostic bring-up code, not production error handling. Core printf output is not redirected to the phase log. Failed partial core initialization is left to process teardown because the legacy core does not expose staged cleanup.

## Platform integration

Platform.cpp provides this fork's Platform.h callbacks through libounce-compatible native threading, mutexes, condition variables, sleep and stdio. It deliberately avoids hard-coded CPU core assignment. Platform config has an empty sentinel table; this test does not load or save settings. Relative emulator data paths resolve under sdmc:/switch/melonds-ounce/; absolute paths are preserved. No ROM or save file is opened by the smoke frontend. Networking is explicitly disabled. StopEmu has no effect because this target never runs emulated frames.

## Still not covered

No BIOS boot, ROM loading, reset, frame execution, screen presentation, audio, input, save testing, suspend/resume, JIT, or GPU acceleration. A successful archive build alone never checked these executable callbacks. Link errors may expose additional fork-specific frontend dependencies; supply the full undefined-symbol list. Record SDK package versions and the final source commit with build and hardware test results.

New source files are GPL-3.0-or-later, consistent with the repository license.
