// SPDX-License-Identifier: GPL-3.0-or-later
#include <cstdio>
#include <switch.h>
#include "Platform.h"
#include "NDS.h"

static bool Record(FILE* file, const char* message) {
    return std::fprintf(file, "%s\n", message) >= 0 && std::fflush(file) == 0;
}

int main(int argc, char** argv) {
    // The launcher/runtime must provide a mounted SD filesystem.
    FILE* log = std::fopen("sdmc:/melonds-ounce-smoke.log", "w");
    if (!log) return 1;
    if (!Record(log, "START: Ounce core initialization test; no ROM execution")) {
        std::fclose(log); return 2;
    }
    Platform::Init(argc, argv);
    Record(log, "TEST: mutex");
    auto* mutex = Platform::Mutex_Create();
    if (!Platform::Mutex_TryLock(mutex)) { std::fclose(log); return 3; }
    Platform::Mutex_Unlock(mutex);
    Platform::Mutex_Free(mutex);
    Record(log, "TEST: thread and semaphore (hang here means synchronization failure)");
    auto* semaphore = Platform::Semaphore_Create();
    auto* thread = Platform::Thread_Create([semaphore] { Platform::Semaphore_Post(semaphore); });
    Platform::Semaphore_Wait(semaphore);
    Platform::Thread_Free(thread);
    Platform::Semaphore_Free(semaphore);
    Record(log, "PASS: mutex, thread, semaphore");
    NDS::SetConsoleType(0);
    Record(log, "CALL: NDS::Init");
    if (!NDS::Init()) {
        Record(log, "FAIL: NDS::Init; partial initialization cleanup is not assumed safe");
        std::fclose(log); return 4;
    }
    Record(log, "PASS: NDS::Init");
    Record(log, "CALL: NDS::DeInit");
    NDS::DeInit();
    Platform::DeInit();
    bool recorded = Record(log, "PASS: core initialization and shutdown; no frames executed");
    std::fclose(log);
    return recorded ? 0 : 5;
}
