// SPDX-License-Identifier: GPL-3.0-or-later
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include "Config.h"
#include "NDS.h"
#include "Platform.h"

namespace SPI_Firmware {
extern u8* Firmware;
extern u32 FirmwareLength;
}

static FILE* logFile;
static void Log(const char* format, ...) {
    va_list args;
    va_start(args, format);
    std::vfprintf(logFile, format, args);
    va_end(args);
    std::fputc('\n', logFile);
    std::fflush(logFile);
}

static bool Read(const char* path, u8* data, size_t size) {
    FILE* f = std::fopen(path, "rb");
    if (!f) { Log("FAIL: cannot open %s", path); return false; }
    bool seek = std::fseek(f, 0, SEEK_END) == 0;
    long length = seek ? std::ftell(f) : -1;
    Log("FILE: %s length=%ld", path, length);
    bool valid = data ? length == static_cast<long>(size)
                      : length == 0x20000 || length == 0x40000 || length == 0x80000;
    if (valid && data) valid = std::fseek(f, 0, SEEK_SET) == 0 && std::fread(data, 1, size, f) == size;
    std::fclose(f);
    if (!valid) Log("FAIL: invalid length or read error: %s", path);
    return valid;
}

static bool Probe() {
    std::strcpy(Config::BIOS7Path, "sdmc:/switch/melonds-ounce/bios7.bin");
    std::strcpy(Config::BIOS9Path, "sdmc:/switch/melonds-ounce/bios9.bin");
    std::strcpy(Config::FirmwarePath, "sdmc:/switch/melonds-ounce/firmware.bin");
    static u8 bios7[sizeof(NDS::ARM7BIOS)];
    static u8 bios9[sizeof(NDS::ARM9BIOS)];
    const bool a = Read(Config::BIOS7Path, bios7, sizeof(bios7));
    const bool b = Read(Config::BIOS9Path, bios9, sizeof(bios9));
    const bool c = Read(Config::FirmwarePath, nullptr, 0);
    if (!(a && b && c)) return false;

    Log("CALL: NDS::SetConsoleType(0)");
    NDS::SetConsoleType(0);
    Log("RETURN: NDS::SetConsoleType");
    Log("CALL: NDS::Init");
    if (!NDS::Init()) { Log("FAIL: NDS::Init"); return false; }
    Log("PASS: NDS::Init");
    Log("CALL: NDS::LoadBIOS");
    NDS::LoadBIOS();
    Log("RETURN: NDS::LoadBIOS");
    const bool match = std::memcmp(bios7, NDS::ARM7BIOS, sizeof(bios7)) == 0 &&
                       std::memcmp(bios9, NDS::ARM9BIOS, sizeof(bios9)) == 0;
    Log(match ? "PASS: core BIOS buffers match SD files" : "FAIL: core BIOS buffers do not match SD files");
    const bool firmware = SPI_Firmware::Firmware && SPI_Firmware::FirmwareLength;
    Log("FIRMWARE: core buffer %s; length=%u", firmware ? "present" : "absent", static_cast<unsigned>(SPI_Firmware::FirmwareLength));
    Log("CALL: NDS::DeInit");
    NDS::DeInit();
    Log("RETURN: NDS::DeInit");
    return match && firmware;
}

int main(int argc, char** argv) {
    logFile = std::fopen("sdmc:/melonds-ounce-bios-probe.log", "w");
    if (!logFile) return 1;
    Log("BUILD: runner-bios-probe-01");
    Log("CALL: Platform::Init");
    Platform::Init(argc, argv);
    Log("RETURN: Platform::Init");
    const bool ok = Probe();
    Log("CALL: Platform::DeInit");
    Platform::DeInit();
    Log("RETURN: Platform::DeInit");
    Log(ok ? "COMPLETE: probe finished; no ROM or frames" : "FAIL: probe did not verify BIOS and firmware");
    std::fclose(logFile);
    return ok ? 0 : 2;
}
