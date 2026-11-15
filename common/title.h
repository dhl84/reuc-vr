#pragma once
// Constants of the game that this build is made for. The default is Resident Evil: The Umbrella Chronicles (RBUE08).
// -DUC_TITLE=dc (CMake) defines UC_TITLE_DC and selects Resident Evil: The Darkside Chronicles (SBDE08), which is being brought up
// from project/dc (see docs/darkside-port.md). Addresses here were found with tools/dolmatch.py and checked by hand where the comment says so.
#include <cstddef>
#include <cstdint>

namespace uc::title {
#if defined(UC_TITLE_DC)
inline constexpr const char* kDiscId = "SBDE08";
inline constexpr const char* kName = "Resident Evil: The Darkside Chronicles";
inline constexpr const char* kProjectDir = "project/dc";   // symbol lists of a build tree (game/uc_main.cpp)
inline constexpr bool kActiveThreadQueue = true;           // the native thread calls keep __OSActiveThreadQueue (runtime/hle/os_thread.cpp)
// OSThread of the SDK's DefaultThread. __OSThreadInit is the same code in both games (8037E618 here, 80261104 in RBUE08);
// its data moved from 805F88D8 to 8075E018 together with its neighbours (tools/dolmatch.py data_addresses.txt).
inline constexpr uint32_t kDefaultThread = 0x8075E018;
// Small-data (r13) displacements and data addresses below come from the same place of functions that are the same code in both games
// (tools/dolmatch.py sda_offsets.txt and data_addresses.txt; the vote count is the number of such places).
inline constexpr int32_t kSdaSchedulerLock = -9000;        // 6 votes
inline constexpr int32_t kSdaViRetraceCount = -8436, kSdaViPostCb = -8480, kSdaViPreCb = -8484;   // 10, 4 and 5 votes
inline constexpr int32_t kSdaNandState = -7496;            // 3 votes
inline constexpr uint32_t kNandHomeA = 0x8070A4A0, kNandHomeB = 0x807819E0;   // 4 and 2 votes
inline constexpr const char* kTitleDir = "/title/00010000/53424445";          // "SBDE"
inline constexpr char kSaveHome[] = "/title/00010000/53424445/data";
inline constexpr int32_t kSdaAxCallback = -8116, kSdaAxVoiceCount = -8004, kSdaAxVoiceBase = -8016;   // 4, 12 and 12 votes
inline constexpr uint32_t kAxSyncPbs = 0x803963BC;         // same code as RBUE08 80275F34
inline constexpr uint32_t kAxServiceCallbacks = 0x80393AE4; // called from the same place as RBUE08 80273688; its body differs: not verified
#else
inline constexpr const char* kDiscId = "RBUE08";
inline constexpr const char* kName = "Resident Evil: The Umbrella Chronicles";
inline constexpr const char* kProjectDir = "project";
inline constexpr bool kActiveThreadQueue = false;
// The start code runs on the SDK's DefaultThread: __OSThreadInit (802611B8) stores this OSThread in the current-thread word and fills it in.
inline constexpr uint32_t kDefaultThread = 0x805F88D8;
inline constexpr int32_t kSdaSchedulerLock = -26464;       // the SDK's scheduler-lock counter
inline constexpr int32_t kSdaViRetraceCount = -25948, kSdaViPostCb = -25992, kSdaViPreCb = -25996;   // retrace handler at 8026D1C0, setters at 8026D8C4 / 8026D908
inline constexpr int32_t kSdaNandState = -25056;
inline constexpr uint32_t kNandHomeA = 0x804291C0, kNandHomeB = 0x8061F6C0;
inline constexpr const char* kTitleDir = "/title/00010000/52425545";          // "RBUE"
inline constexpr char kSaveHome[] = "/title/00010000/52425545/data";
inline constexpr int32_t kSdaAxCallback = -25636, kSdaAxVoiceCount = -25524, kSdaAxVoiceBase = -25536;
inline constexpr uint32_t kAxSyncPbs = 0x80275F34, kAxServiceCallbacks = 0x80273688;
#endif
}  // namespace uc::title

// UC_ADDR(0x8xxxxxxx), UC_SDA13(n) and UC_SDA2(n) name an RBUE08 address, r13 displacement and r2 displacement in native code.
// In an RBUE08 build they are the value itself. In an SBDE08 build they are the value of that game from common/title_map_dc.inc
// (tools/title_map.py), found at compile time; a value that the table does not have stops the compile.
#if defined(UC_TITLE_DC)
namespace uc::title::detail {
struct AddrPair { uint32_t ref, here; };
struct SdaPair { int32_t ref, here; };
inline constexpr AddrPair kAddr[] = {
#define UC_TITLE_ADDR(ref, here) {ref, here},
#define UC_TITLE_SDA13(ref, here)
#define UC_TITLE_SDA2(ref, here)
#include "title_map_dc.inc"
#undef UC_TITLE_ADDR
#undef UC_TITLE_SDA13
#undef UC_TITLE_SDA2
    {0, 0}};
inline constexpr SdaPair kSda13[] = {
#define UC_TITLE_ADDR(ref, here)
#define UC_TITLE_SDA13(ref, here) {ref, here},
#define UC_TITLE_SDA2(ref, here)
#include "title_map_dc.inc"
#undef UC_TITLE_ADDR
#undef UC_TITLE_SDA13
#undef UC_TITLE_SDA2
    {0x7FFFFFFF, 0}};
inline constexpr SdaPair kSda2[] = {
#define UC_TITLE_ADDR(ref, here)
#define UC_TITLE_SDA13(ref, here)
#define UC_TITLE_SDA2(ref, here) {ref, here},
#include "title_map_dc.inc"
#undef UC_TITLE_ADDR
#undef UC_TITLE_SDA13
#undef UC_TITLE_SDA2
    {0x7FFFFFFF, 0}};
constexpr uint32_t addr(uint32_t ref) { for (const auto& p : kAddr) if (p.ref == ref && ref) return p.here; return 0; }
template <size_t N> constexpr int64_t sda(const SdaPair (&table)[N], int32_t ref) { for (const auto& p : table) if (p.ref == ref) return p.here; return INT64_MIN; }
template <uint32_t Ref> struct Addr {
    static constexpr uint32_t value = addr(Ref);
    static_assert(value != 0, "UC_ADDR: common/title_map_dc.inc has no SBDE08 address for this RBUE08 address");
};
template <int32_t Ref> struct Sda13 {
    static constexpr int64_t found = sda(kSda13, Ref);
    static_assert(found != INT64_MIN, "UC_SDA13: common/title_map_dc.inc has no SBDE08 displacement for this RBUE08 displacement");
    static constexpr int32_t value = int32_t(found);
};
template <int32_t Ref> struct Sda2 {
    static constexpr int64_t found = sda(kSda2, Ref);
    static_assert(found != INT64_MIN, "UC_SDA2: common/title_map_dc.inc has no SBDE08 displacement for this RBUE08 displacement");
    static constexpr int32_t value = int32_t(found);
};
}  // namespace uc::title::detail
#define UC_ADDR(x) (::uc::title::detail::Addr<(x)>::value)
#define UC_SDA13(x) (::uc::title::detail::Sda13<(x)>::value)
#define UC_SDA2(x) (::uc::title::detail::Sda2<(x)>::value)
#else
#define UC_ADDR(x) (uint32_t(x))
#define UC_SDA13(x) (int32_t(x))
#define UC_SDA2(x) (int32_t(x))
#endif
