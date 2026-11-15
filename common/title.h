#pragma once
// Constants of the game that this build is made for. The default is Resident Evil: The Umbrella Chronicles (RBUE08).
// -DUC_TITLE=dc (CMake) defines UC_TITLE_DC and selects Resident Evil: The Darkside Chronicles (SBDE08), which is being brought up
// from project/dc (see docs/darkside-port.md). Addresses here were found with tools/dolmatch.py and checked by hand where the comment says so.
#include <cstdint>

namespace uc::title {
#if defined(UC_TITLE_DC)
inline constexpr const char* kDiscId = "SBDE08";
inline constexpr const char* kName = "Resident Evil: The Darkside Chronicles";
// OSThread of the SDK's DefaultThread. __OSThreadInit is the same code in both games (8037E618 here, 80261104 in RBUE08);
// its data moved from 805F88D8 to 8075E018 together with its neighbours (tools/dolmatch.py data_addresses.txt).
inline constexpr uint32_t kDefaultThread = 0x8075E018;
#else
inline constexpr const char* kDiscId = "RBUE08";
inline constexpr const char* kName = "Resident Evil: The Umbrella Chronicles";
// The start code runs on the SDK's DefaultThread: __OSThreadInit (802611B8) stores this OSThread in the current-thread word and fills it in.
inline constexpr uint32_t kDefaultThread = 0x805F88D8;
#endif
}  // namespace uc::title
