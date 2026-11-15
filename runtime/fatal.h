#pragma once
#include <cstdint>
namespace uc {
[[noreturn]] void fatal(const char* fmt, ...);
[[noreturn]] void fatal_guest(uint32_t addr, const char* what);
void set_exit_hook(void (*fn)());   // called once before the process exits (stats, flush)
void set_crash_hook(void (*fn)());  // called first on the thread of a host crash, before the report: short, and no allocation
void install_crash_handlers();      // uncaught C++ exceptions and host exceptions (access violations ...) print a report with the guest trace
}
