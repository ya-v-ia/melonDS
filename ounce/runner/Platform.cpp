// SPDX-License-Identifier: GPL-3.0-or-later
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <utility>
#include <switch.h>
#include "Platform.h"
#include "Config.h"

namespace Config {
ConfigEntry PlatformConfigFile[] = {{"", -1, nullptr, 0, nullptr, 0}};
}

namespace Platform {
static void Check(Result result) { if (R_FAILED(result)) std::abort(); }
void Init(int, char**) {}
void DeInit() {}
void StopEmu() {}

FILE* OpenFile(const char* path, const char* mode, bool mustexist) {
    if (!path || !mode) return nullptr;
    if (mustexist) {
        FILE* file = std::fopen(path, "rb");
        return file ? std::freopen(path, mode, file) : nullptr;
    }
    return std::fopen(path, mode);
}
FILE* OpenLocalFile(const char* path, const char* mode) {
    if (!path) return nullptr;
    if (path[0] == '/' || std::strchr(path, ':')) return OpenFile(path, mode);
    char full[2048];
    int n = std::snprintf(full, sizeof(full), "sdmc:/switch/melonds-ounce/%s", path);
    if (n < 0 || static_cast<size_t>(n) >= sizeof(full)) return nullptr;
    return OpenFile(full, mode);
}
FILE* OpenDataFile(const char* path) { return OpenLocalFile(path, "rb"); }
void Sleep(u64 usecs) {
    while (usecs > 1000000) { svcSleepThread(1000000000); usecs -= 1000000; }
    svcSleepThread(static_cast<s64>(usecs * 1000));
}

struct Thread { ::Thread native{}; std::function<void()> entry; };
static void ThreadEntry(void* arg) { static_cast<Thread*>(arg)->entry(); }
Thread* Thread_Create(std::function<void()> func) {
    Thread* thread = new Thread;
    thread->entry = std::move(func);
    Check(threadCreate(&thread->native, ThreadEntry, thread, nullptr, 1024 * 1024, 0x30, -2));
    Check(threadStart(&thread->native));
    return thread;
}
void Thread_Wait(Thread* thread) { if (thread) Check(threadWaitForExit(&thread->native)); }
void Thread_Free(Thread* thread) {
    if (!thread) return;
    Thread_Wait(thread);
    Check(threadClose(&thread->native));
    delete thread;
}

struct Semaphore { ::CondVar condition; ::Mutex mutex; u64 count; };
Semaphore* Semaphore_Create() {
    auto* sem = new Semaphore{};
    mutexInit(&sem->mutex);
    condvarInit(&sem->condition);
    return sem;
}
void Semaphore_Free(Semaphore* sem) { delete sem; }
void Semaphore_Reset(Semaphore* sem) {
    mutexLock(&sem->mutex); sem->count = 0; mutexUnlock(&sem->mutex);
}
void Semaphore_Wait(Semaphore* sem) {
    mutexLock(&sem->mutex);
    while (!sem->count) Check(condvarWait(&sem->condition, &sem->mutex));
    --sem->count;
    mutexUnlock(&sem->mutex);
}
void Semaphore_Post(Semaphore* sem, int count) {
    if (count <= 0) return;
    mutexLock(&sem->mutex);
    sem->count += static_cast<u64>(count);
    Check(condvarWake(&sem->condition, count));
    mutexUnlock(&sem->mutex);
}

struct Mutex { ::Mutex native; };
Mutex* Mutex_Create() { auto* mutex = new Mutex{}; mutexInit(&mutex->native); return mutex; }
void Mutex_Free(Mutex* mutex) { delete mutex; }
void Mutex_Lock(Mutex* mutex) { mutexLock(&mutex->native); }
void Mutex_Unlock(Mutex* mutex) { mutexUnlock(&mutex->native); }
bool Mutex_TryLock(Mutex* mutex) { return mutexTryLock(&mutex->native); }

bool MP_Init() { return false; }
void MP_DeInit() {}
int MP_SendPacket(u8*, int) { return 0; }
int MP_RecvPacket(u8*, bool) { return 0; }
bool LAN_Init() { return false; }
void LAN_DeInit() {}
int LAN_SendPacket(u8*, int) { return 0; }
int LAN_RecvPacket(u8*) { return 0; }
}
