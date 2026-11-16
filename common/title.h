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
inline constexpr const char* kSharedFolder = "DarksideChronicles";   // /sdcard/<folder> on the Quest (game/android_main.cpp, quest/reuc/build.gradle)
inline constexpr const char* kProjectDir = "project/dc";   // symbol lists of a build tree (game/uc_main.cpp)
inline constexpr bool kActiveThreadQueue = true;           // the native thread calls keep __OSActiveThreadQueue (runtime/hle/os_thread.cpp)
// OSJoinThread of the SDK does not wait for a detached thread (attribute bit 0): it returns FALSE at once. The game's loader threads
// (created at 80065E60 with attribute 1) call it on themselves when their work is done (8006650C), so a native that waits never
// returns there: the thread never ends, and the next OSCreateThread on the same OSThread links it into the active thread queue a
// second time (a loop in the queue).
inline constexpr bool kJoinSkipsDetached = true;
// KPAD of the 2009 SDK: a KPADStatus is 176 bytes (mulli r6,r22,176 at 80412124; 132 in RBUE08), and the read entry that the game
// calls (80411E60) takes a pointer to an error word in r6 (the KPADReadEx form).
inline constexpr uint32_t kKpadStatusSize = 176;
// The PC action layer (game/game_input.cpp) reads RBUE08 game state to place the Wii Remote buttons. It is off for this game:
// the Wii Remote sample goes to the game as it is.
inline constexpr bool kNativeActionMap = false;
// The player reloads with a wave of the Wii Remote, swings the knife with A held and a wave, and waves to shake an enemy off. A host
// has no accelerometer, so the input sample carries a wave as one more button bit, above the 16 of the Wii Remote: while it is held,
// KPADRead (runtime/native_input.cpp) reports a remote that swings from side to side. 0: the game has no such bit.
inline constexpr uint32_t kButtonWave = 0x10000;
// The game loads the next part of a chapter on a thread of lower priority while the chapter runs. With the timer step of the frame wait
// (game/game_pacing.cpp timed_wait) a late frame sleeps for this number of milliseconds, so that such threads run. 0: no sleep.
inline constexpr uint32_t kTimedYieldMs = 2;
// The game draws its shadow map into the top left 512x480 pixels of the frame buffer and copies it from there (docs/darkside-port.md,
// "Shadows"). The Vulkan renderer draws a packet with a copy of exactly this source at one pixel for each guest pixel, and not at the
// scale of its frame buffer: the picture is a texture of that size for the game, not a part of what the player sees. On a Quest 3
// the frame buffer is 1514x2160 pixels for each eye, and the shadow packets took 3.4 ms of GPU time a frame. 0: no such copy.
inline constexpr uint32_t kNativeCopyWidth = 512, kNativeCopyHeight = 480;
// The game copies the picture to the same texture two or three times in a frame: after the solid part of the scene, after the
// see-through part and after the effects. An effect that bends the picture samples the copy before it. Most frames have no such
// effect, and the next copy replaces the texture before a draw sampled it. The builder (runtime/hle/gx.cpp seal_or_hold) drops a
// copy that the game replaces unsampled, and joins the draws of its packet to the next packet. On a Quest 3 each joined packet
// saves one load and one store of the whole frame buffer and one blit. false: every copy is made.
inline constexpr bool kDropDeadCopies = true;
// The post-processing chain of the game works in the top left corner of the frame buffer: it draws a quad there, copies the corner to
// a texture, and draws the next quad with that texture (docs/darkside-port.md, "The post-processing chain"). The Vulkan renderer draws
// such a packet in a small stand-in for that corner (the bloom proxy of RBUE08), and not in the large frame buffer of both eyes. The
// stand-in has the pixel density of the frame buffer, so the picture is the same. false: the rule of RBUE08 (2 points a guest pixel,
// from a frame buffer scale of 2.5, the bloom chain of that game). UC_NO_BLOOM_PROXY=1 turns the proxy off in both games.
inline constexpr bool kCornerChainProxy = true;
inline constexpr uint32_t kTestPadButtons = 0x1FFFF;   // the test server may send every Wii Remote and Nunchuk button, and the wave (bring-up)
inline constexpr bool kKpadReadError = true;
inline constexpr uint8_t kKpadPointerValid = 2;            // dpd_valid_fg: both sensor bar points seen (test value, see docs/darkside-port.md)
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
// End of the MEM1 arena. A console's start code puts the file table of the disc at the top of MEM1 and the arena ends there:
// (0x817FFFFF - maximum table size) rounded down to 32. boot.bin gives 0x9F35 words at 0x42C: 0x817D8320 (its word at 0x430 is this
// address minus 0x01400000). The game gives all of MEM1 to one frame heap and fills it when it loads a chapter: with 0x81700000 the
// heap was full while the first chapter loaded (an allocation of 4224 bytes at 8011DB24 failed with 428 bytes left).
inline constexpr uint32_t kMem1ArenaHi = 0x817D8320;
// End of the MEM2 arena; the IPC buffers (0x20000 bytes) follow it. The value depends on the IOS version that the game runs on
// (tmd.bin, word at 0x188): IOS55 for SBDE08 leaves MEM2 up to 0x935E0000 to the game, IOS21 for RBUE08 up to 0x933E0000 (the table
// ios_memory_values of Dolphin, Source/Core/Core/IOS/VersionInfo.cpp). The game needs the 2 MB: with 0x933E0000 its MEM2 frame heap
// was 0.68 MB too small when the first chapter started. The allocation of a 1 MB heap at 80358284 then returned 0, the heap was
// made at address 0, and its header went over low memory (the active thread queue at 800000DC): "Loading data" never ended.
inline constexpr uint32_t kMem2ArenaHi = 0x935E0000;
// The game draws its shadow casters, black on white, into a 512x512 part of the frame buffer of the console (640x528), from the light
// (8033B69C). It copies that part three times: to a GX_TF_Z16 texture (8033B9B8), at half size to a GX_CTF_R8 texture (8033BBA0), and,
// after it has added the half-size texture seven times at small offsets, to the GX_CTF_R8 texture that the materials of the scene
// lay over the ground (8033C528). The native renderers have a 640x480 colour buffer. A copy that names lines below 480 takes the
// lines that exist. They keep their place in the texture (gx::TextureCopy::lines, Vulkan renderer): stretched over the whole texture,
// the seven additions read the casters 6 % of their line number away from where they add, which made every shadow white.
// The casters of the last 32 lines are not drawn.
inline constexpr uint32_t kEfbCopyLines = 528;
// The materials with the depth flag (byte 12 of their shadow data, 8033DF24) read the GX_TF_Z16 copy as GX_TF_IA8 with a ramp
// texture. No draw of the first chapter does. The native renderers have no depth copies: the texture reads as the farthest depth
// in every texel (gx::TextureChannels::depth_far), so such a material draws no shadow. An approximation.
inline constexpr bool kDepthCopyAsFar = true;
// The game measures the brightness of the picture for its exposure: it copies the middle of the frame buffer to GX_TF_I8 twice, the
// second time to a 32x16 texture (8036AD98), and then reads 66 bytes of that texture from memory (8036AE74). From their mean it takes
// away the two conversions of the hardware (Y = 16 + 0.8588 colour, twice) and gives the result to its tone mapping. A native EFB copy
// stays on the GPU, so the texture in memory was all zero and the game chose the largest exposure: the daylight scene was white.
// An intensity copy of at most this number of texels is read back from the GPU and written to the guest's texture when the GPU has
// finished it, one or two frames later (runtime/hle/gx.cpp hle_GXCopyTex). 0: no copy is read back.
inline constexpr uint32_t kEfbCopyReadbackTexels = 1024;
// The game lights each model with a texture of 4x4 texels (8x8 for some), RGBA8, that it computes on the CPU from the lights at the
// place of the model: the light object of a model (vtable 806F22B4) keeps the texels at +0x40, its update (8034F238) asks the light
// manager (80758480, virtual +0x78 = 802D61D4) to write them, every frame, and the material of the model samples them as texture map 1.
// The first chapter has about 80 such textures, and nearly every one gets new texels in every frame. A texture of at most this number
// of texels is handled as one that the game rewrites: the texture cache keeps only its newest texels (gx::TextureCache), and the
// Vulkan renderer takes its GPU image from a pool and uploads through one buffer of the frame (VulkanRenderer::make_texture).
// Before, each rewrite was a new cache entry and a new GPU image with two memory allocations. 0: no texture is handled this way.
inline constexpr uint32_t kSmallTextureTexels = 64;
// The movie player of the game decodes each frame into the same 640x480 RGBA8 texture, so a movie is a new version of one texture
// in every frame. The texture cache keeps the versions of a texture, because a game may write the same texels again; a movie never
// does, and 700 frames of it filled the cache (1 GB) and kept their GPU images. When this number of older versions of one texture
// (same address, size, format and palette) is in the cache, a new version drops them. 0: no limit.
inline constexpr uint32_t kTextureVersionLimit = 2;
// GXSetBlendMode of the SDK writes the subtract bit (bit 11 of the blend word) at every call: it is set for GX_BM_SUBTRACT and cleared for
// every other mode (803A7F98: rlwimi r9,r0,6,20,20). These are the bits that the native (runtime/hle/gx.cpp) replaces. With 0xF7E3 the
// bit stayed set after the first subtract draw, and every later draw that set its blend through this function took the destination
// minus the source: the glow of the chapter and character pieces under the pointer was a dark square, and the effect sprites of the
// menus and of the chapters were dark.
inline constexpr uint32_t kBlendModeBits = 0xFFE3;
// The effect library of the game draws one kind of particle as GX_POINTS, one point for each GXBegin (call site 804A1D10, after
// GXSetPointSize at 804A1CC0). The native renderers have no point primitive: a point draw stopped the game with "unsupported native
// draw topology" in the sixth chapter of "Memories of a Lost City". Each point becomes a square of two triangles where the draw
// packets are made (runtime/hle/gx.cpp expand_points), so every renderer gets triangles. false: the draw goes to the renderer, which stops.
inline constexpr bool kPointQuads = true;
// The health of a player (the "LEON" bar of the HUD) is the float at +0x974 of the player object: 50 is full, 0 is dead. Its setter
// (800AC7EC) holds it between 0 and 50, a hit takes its damage away (800ACA74), a herb adds (800AC86C). The player manager is the
// object at 80736378 (its getter is 80046EFC). It keeps a handle of two words for each of the two players at +0xAC: an id and an
// index into the object table at 80732B88, whose entries are an id and a pointer (80046DC8 looks a player up this way). The test aid
// UC_DC_GOD=1 (game/uc_main.cpp) writes 50 to both players at every retrace.
inline constexpr uint32_t kDcPlayerHandles = 0x80736378 + 0xAC, kDcObjectTable = 0x80732B88, kDcPlayerHealth = 0x974;
inline constexpr uint32_t kDcFullHealthBits = 0x42480000;   // 50.0f
// The partner (the "KRAUSER" bar in the first chapter) is not a player object. The object at 80735500 (its getter is 80049CC8) keeps
// four floats of health at +0xF8 and their maxima at +0xE8 (the setter 800FC664 holds a value below its maximum, the getter 800B1D88
// gives it to the HUD at 800B17C0). The test aid writes each maximum to its value.
inline constexpr uint32_t kDcPartnerHealth = 0x80735500 + 0xF8, kDcPartnerHealthMax = 0x80735500 + 0xE8;
// The post-processing effects of the game are objects of one base class (docs/darkside-port.md, "The post-processing chain"). The byte
// at +2 of an effect is its switch: the draw function of the base class (802D8D44) returns at once when it is 0, and the virtual
// functions 802D8E60 and 802D8E78 write 1 and 0. The word at +4 is the vtable. The manager of the effects is the static object at
// 8075B010 (its getter is 80323304). It holds two sets of effects, at +2824 and at +4336. The glow effect (vtable 806EE568, draw
// function 802A5E30) is at +220 of a set. With the bloom switch of the port off (graphics.bloom, the headset's Glow),
// game/uc_main.cpp holds the switch of the two glow effects at 0. The game then makes about ten packets less in a frame of a scene
// with glow. The empty draw function 8029FFE0 in the vtable is no way to do this: the base class then still prepares the draw, and
// the effects after the glow draw at twice their size.
inline constexpr uint32_t kDcGlowEffects[2] = {0x8075B010 + 2824 + 220, 0x8075B010 + 4336 + 220}, kDcGlowVtable = 0x806EE568;
// The clock of the game is the object at 80736D10 (docs/darkside-port.md, "Frame pacing and load"). Its eight time scales are the
// floats at +0x0C to +0x28. The test aid UC_DC_FREEZE_FILE (game/uc_main.cpp) holds them at 0.
inline constexpr uint32_t kDcClock = 0x80736D10;
#else
inline constexpr const char* kDiscId = "RBUE08";
inline constexpr const char* kName = "Resident Evil: The Umbrella Chronicles";
inline constexpr const char* kSharedFolder = "UmbrellaChronicles";
inline constexpr const char* kProjectDir = "project";
inline constexpr bool kActiveThreadQueue = false;
inline constexpr bool kJoinSkipsDetached = false;
inline constexpr uint32_t kKpadStatusSize = 132;
inline constexpr bool kNativeActionMap = true;
inline constexpr uint32_t kTestPadButtons = 0x1C10;   // A, B, plus, minus
inline constexpr uint32_t kButtonWave = 0;
inline constexpr uint32_t kTimedYieldMs = 0;
inline constexpr uint32_t kNativeCopyWidth = 0, kNativeCopyHeight = 0;
inline constexpr bool kDropDeadCopies = false;
inline constexpr bool kCornerChainProxy = false;
inline constexpr bool kKpadReadError = false;
inline constexpr uint8_t kKpadPointerValid = 1;
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
inline constexpr uint32_t kMem1ArenaHi = 0x81700000;
inline constexpr uint32_t kMem2ArenaHi = 0x933E0000;
inline constexpr uint32_t kEfbCopyLines = 480;
inline constexpr bool kDepthCopyAsFar = false;
inline constexpr uint32_t kEfbCopyReadbackTexels = 0;
inline constexpr uint32_t kSmallTextureTexels = 0;
inline constexpr uint32_t kTextureVersionLimit = 0;
// The bits of the blend word that the native GXSetBlendMode replaces. The SDK code of RBUE08 (80288274) is the same as that of SBDE08
// and also writes the subtract bit (0x800) at every call, so 0xFFE3 would be right here too. The value is kept until a scene of
// RBUE08 with a subtract draw is checked: a change here changes its picture.
inline constexpr uint32_t kBlendModeBits = 0xF7E3;
inline constexpr bool kPointQuads = false;
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
