#include "cpu_sampler.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <tlhelp32.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace uc {
namespace {
constexpr int kDepth = 10;
struct Stack { uint32_t frame[kDepth]; uint32_t count; uint32_t thread; };   // thread: the Windows thread id in the all-threads mode, else 0
// Open-addressing table of distinct stacks, sized up front: the target may hold the heap lock while it is suspended.
constexpr size_t kSlots = 1u << 17;
std::atomic<bool> replaying{false};
// Walks the stack of a suspended thread into frame[] and returns the number of frames. The unwind data does not describe every place a thread can
// stop at (15 November 2026: with every thread sampled, RtlVirtualUnwind read an address that is not mapped and the process ended): the walk ends
// there and keeps the frames it has.
uint32_t walk_stack(CONTEXT& context, uintptr_t base, uint32_t* frame) {
    volatile int depth = 0;
    __try {
        for (; depth < kDepth && context.Rip; depth = depth + 1) {
            frame[depth] = uint32_t(context.Rip - base);   // outside the image (ntdll, drivers) becomes a garbage rva, filtered by the resolver
            DWORD64 image = 0;
            if (auto* entry = RtlLookupFunctionEntry(context.Rip, &image, nullptr)) {
                void* handler = nullptr; DWORD64 established = 0;
                RtlVirtualUnwind(UNW_FLAG_NHANDLER, image, context.Rip, entry, &context, &handler, &established, nullptr);
            } else break;   // leaf without unwind data
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) { return uint32_t(depth < kDepth ? depth + 1 : kDepth); }
    return uint32_t(depth);
}
}  // namespace

void cpu_sampler_set_replaying(bool value) { replaying.store(value,std::memory_order_relaxed); }

void cpu_sampler_start_once(const char* thread) {
    const char* want = std::getenv("UC_CPU_SAMPLE");
    if (want && want[0] == '1') want = "guest";
    static bool started = false;
    if (!want || started || std::string_view(want) != thread) return;
    started = true;
    // UC_CPU_SAMPLE=all: every thread of the process, each tick; the output is cpu-samples-all.txt with the thread id before each stack.
    // A thread that is outside the image (it waits in a system call, or it runs in a system library or a driver) is not counted.
    const bool all = std::string_view(want) == "all";
    HANDLE target = nullptr;
    if (!all && !DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &target, 0, FALSE, DUPLICATE_SAME_ACCESS)) return;
    std::thread([target, all] {
        const auto base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
        const uintptr_t image_end = base + reinterpret_cast<const IMAGE_NT_HEADERS*>(base + reinterpret_cast<const IMAGE_DOS_HEADER*>(base)->e_lfanew)->OptionalHeader.SizeOfImage;
        std::vector<Stack> table(kSlots);
        std::vector<std::pair<DWORD, HANDLE>> threads;   // all-threads mode: the other threads of the process, read again once per second
        threads.reserve(512);
        unsigned rounds = 0;
        const auto refresh = [&] {
            for (const auto& entry : threads) CloseHandle(entry.second);
            threads.clear();
            const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
            if (snapshot == INVALID_HANDLE_VALUE) return;
            THREADENTRY32 entry{}; entry.dwSize = sizeof(entry);
            const DWORD process = GetCurrentProcessId(), self = GetCurrentThreadId();
            for (BOOL more = Thread32First(snapshot, &entry); more; more = Thread32Next(snapshot, &entry)) {
                if (entry.th32OwnerProcessID != process || entry.th32ThreadID == self || threads.size() == threads.capacity()) continue;
                if (const HANDLE handle = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, entry.th32ThreadID)) threads.emplace_back(entry.th32ThreadID, handle);
            }
            CloseHandle(snapshot);
        };
        auto last = std::chrono::steady_clock::now();
        const char* delay = std::getenv("UC_CPU_SAMPLE_DELAY");   // seconds to skip first (boot, menus)
        const auto begin = last + std::chrono::seconds(delay ? std::atoi(delay) : 0);
        const char* flag = std::getenv("UC_CPU_SAMPLE_FLAG");   // sample only once this file exists (a test scenario creates it at the scene of interest)
        const bool only_replay=std::getenv("UC_CPU_SAMPLE_REPLAY")!=nullptr;
        bool armed = !flag;
        for (unsigned tick = 0;; ++tick) {
            std::this_thread::sleep_for(std::chrono::microseconds(500));
            if (std::chrono::steady_clock::now() < begin) continue;
            if (!armed) { if (tick % 100 == 0) armed = GetFileAttributesA(flag) != INVALID_FILE_ATTRIBUTES; if (!armed) continue; }
            if(only_replay && !replaying.load(std::memory_order_relaxed))continue;
            const auto sample = [&](HANDLE target, uint32_t thread_id) {
            if (SuspendThread(target) == DWORD(-1)) return;
            // Recheck after suspension so the sample cannot slip into forward simulation.
            if(only_replay && !replaying.load(std::memory_order_relaxed)) { ResumeThread(target);return; }
            CONTEXT context{};
            context.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
            Stack stack{};
            if (GetThreadContext(target, &context) && (!all || (context.Rip >= base && context.Rip < image_end))) {
                stack.count = walk_stack(context, base, stack.frame);
            }
            ResumeThread(target);
            if (!stack.count) return;
            stack.thread = thread_id;
            uint64_t hash = 1469598103934665603ull;
            for (int i = 0; i < kDepth; ++i) hash = (hash ^ stack.frame[i]) * 1099511628211ull;
            hash = (hash ^ thread_id) * 1099511628211ull;
            for (size_t probe = 0, slot = hash & (kSlots - 1); probe < 64; ++probe, slot = (slot + 1) & (kSlots - 1)) {
                Stack& entry = table[slot];
                if (!entry.count) { entry = stack; entry.count = 1 | (stack.count << 24); break; }
                if (std::equal(entry.frame, entry.frame + kDepth, stack.frame) && (entry.count >> 24) == stack.count && entry.thread == thread_id) { ++entry.count; break; }
            }
            };
            if (all) { ++rounds; if (threads.empty()) refresh(); for (const auto& entry : threads) sample(entry.second, entry.first); }
            else sample(target, 0);
            if (std::chrono::steady_clock::now() - last >= std::chrono::seconds(1)) {
                last = std::chrono::steady_clock::now();
                if (FILE* file = std::fopen(all ? "cpu-samples-all.txt" : "cpu-samples.txt", "w")) {
                    if (all) std::fprintf(file, "T0 0 = %u\n", rounds);   // the number of sampling rounds: a thread's share of them is the share of the time it ran code of the image
                    for (const Stack& entry : table) {
                        if (!entry.count) continue;
                        if (all) std::fprintf(file, "T%u ", entry.thread);
                        for (uint32_t i = 0; i < (entry.count >> 24); ++i) std::fprintf(file, "%x ", entry.frame[i]);
                        std::fprintf(file, "= %u\n", entry.count & 0xFFFFFF);
                    }
                    std::fclose(file);
                }
                if (all) refresh();
            }
        }
    }).detach();
}
}  // namespace uc
