# Standalone Ounce display test

Experimental and uncompiled when introduced. This target intentionally does not link the emulator core, modify the working initialization smoke test, or load games. It isolates native window creation, EGL/GLES 3, shader compilation, RGBA texture uploads, presentation, and Plus-button exit. The API path follows ounce-devkit's gles-triangle example; hardware compatibility and correctness still require testing.

## Build

Inside the existing SDK Docker container, at /work:

```sh
source "$DEVKITPRO/switch2vars.sh"
aarch64-none-elf-cmake -S ounce/display -B build-ounce-display -DCMAKE_BUILD_TYPE=Debug
cmake --build build-ounce-display --target ounce-display-nro --parallel 1
```

Expected output after a successful build: build-ounce-display/melonds-ounce-display.nro. Keep the existing smoke-test NRO separately. No Docker image rebuild should be needed if its SDK already contains OunceVK/EGL. If EGL is missing, inspect the installed SDK packages; do not substitute desktop or original Switch Mesa libraries.

## Expected visual result

Launch through your working Ounce homebrew runtime. The test stays open until Plus is pressed or the runtime ends the application loop. It draws two vertically stacked, integer-scaled 256x192 images with a 16-output-pixel gap, centered on a dark background:

- Top: red, green, blue, white bars from left to right.
- Bottom: cyan/magenta checkerboard.
- Both: a yellow square at the upper-left corner and a white line moving left to right below it.

Texture input is explicit RGBA bytes with top-down source rows. The shader flips V so row zero appears at the top. Future melonDS framebuffer upload code must separately verify its channel format rather than assume this test establishes it.

## Log and acceptance

The test overwrites sdmc:/melonds-ounce-display.log and flushes messages after each logged phase. It records renderer/version, frame counts at 1 and 120, and detected EGL/GL failures. As in the working smoke test, SD must already be mounted for stdio. No log is not success.

A successful result requires BOTH:

1. The expected patterns are visible, correctly oriented, with motion; Plus returns to the launcher.
2. The log records presented frames and no FAIL entries, including cleanup.

A COMPLETE line alone does not validate the visible output. A swap call returning successfully is not proof that the panel displayed the image. Missing/black output, incorrect colors or orientation, and a nonfunctional exit should be reported with the complete log and whether the console was handheld or docked.

Not covered: core initialization in this executable, ROM loading, emulated frame execution, audio, touch, suspend/resume, or accelerated DS rendering. After visual validation, the next integration is software-core framebuffers through this texture path, with the required BIOS/firmware and ROM-loading checks.

New code: GPL-3.0-or-later, consistent with the repository license.
