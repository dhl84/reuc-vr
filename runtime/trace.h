#pragma once
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>

namespace uc {
void trace_dump(std::FILE* out, int count = 64);       // last guest functions entered by the calling thread (needs UC_TRACE)
void trace_dump_all(std::FILE* out, int count = 16);   // the same for every host thread (used by the no-progress watchdog)
void trace_set_retrace_clock(uint64_t (*clock)());   // the retrace counter for UC_TRACE_NEW (runtime/trace.cpp)
void symbols_load(const std::string& path);            // "<hex> <name> ..." lines; '#' comments
std::string symbol_name(uint32_t addr);                // "name", "name+0x1C" or ""
std::optional<uint32_t> symbol_address(const std::string& name);   // address of a loaded symbol
}
