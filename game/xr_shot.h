#pragma once
#include "ppc_rt.h"
#include "tracked_ray.h"
#include <optional>
namespace uc {
// RBUE08 projectile initializer. The original executes first (ammo, spread,
// weapon flags and speed), then collision and visual trail share the VR ray.
bool align_xr_projectile(ppc::Ctx&,uint32_t object,unsigned player,const TrackedRay&);
class XrShotScope {
public:
    explicit XrShotScope(ppc::Ctx&);
    ~XrShotScope() noexcept(false);
    XrShotScope(const XrShotScope&)=delete;
private:
    ppc::Ctx& c_;
    uint32_t object_=0;
    unsigned player_=0;
    int exceptions_=0;
    std::optional<TrackedRay> ray_;
};
// The segment query 801C7800 from the pointer's ray (800F2B00) and from the item pickup (801AB354): along the VR controller's ray.
class XrPickScope {
public:
    explicit XrPickScope(ppc::Ctx&);
    ~XrPickScope() noexcept(false);
    XrPickScope(const XrPickScope&)=delete;
private:
    ppc::Ctx& c_;
    int exceptions_=0;
    uint32_t start_=0;
    std::array<float,3> saved_{};
};
struct NoXrShotScope {};
template<uint32_t entry> auto xr_shot_scope(ppc::Ctx& c) {
    if constexpr(entry==0x80069210u) return XrShotScope(c);
    else if constexpr(entry==0x801C7800u) return XrPickScope(c);
    else return NoXrShotScope{};
}
#if defined(UC_TITLE_DC)
// SBDE08 (common/title.h, kDcBulletInit). The bullet initializer 800B4E30 runs first. Then a bullet that the weapon of a player fired
// starts at the origin of the VR ray and goes along it. The spread of the pellet around the aim of the game stays around the ray.
bool align_dc_bullet(ppc::Ctx&,uint32_t bullet,unsigned player,const TrackedRay&);
class DcShotScope {
public:
    explicit DcShotScope(ppc::Ctx&);
    ~DcShotScope() noexcept(false);
    DcShotScope(const DcShotScope&)=delete;
private:
    ppc::Ctx& c_;
    uint32_t object_=0;
    unsigned player_=0;
    int exceptions_=0;
    std::optional<TrackedRay> ray_;
};
// The segment query 800B1EB0 of the pointer (from 800B151C) and of the item pick-up (from 800C12E8): along the ray of the VR controller.
class DcPickScope {
public:
    explicit DcPickScope(ppc::Ctx&);
    ~DcPickScope() noexcept(false);
    DcPickScope(const DcPickScope&)=delete;
private:
    ppc::Ctx& c_;
    int exceptions_=0;
    uint32_t start_=0;
    std::array<float,3> saved_{};
};
// The two entries are kDcBulletInit and kDcSegmentQuery of common/title.h (game/xr_shot.cpp makes sure of it). This header names no
// constant of title.h: every translated function includes it, and a change of title.h must not compile them all again.
template<uint32_t entry> auto dc_xr_scope(ppc::Ctx& c) {
    if constexpr(entry==0x800B4E30u) return DcShotScope(c);
    else if constexpr(entry==0x800B1EB0u) return DcPickScope(c);
    else return NoXrShotScope{};
}
#endif
}
