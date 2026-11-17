#include "check.h"
#include "ppc_fixture.h"
#include "xr_shot.h"
#include "title.h"
#include "native_input.h"
#include "controller_input.hpp"
#include "../renderer/aim_geometry.h"
#include <cmath>
#include <limits>

using V=std::array<float,3>;
constexpr uint32_t object=0x80010000,scene=0x80100000,camera=0x8044B4E0;
static void put(ppc::Ctx& c,uint32_t p,const V& v) {for(unsigned i=0;i<3;++i)ppc::wrf32(c,p+4*i,v[i]);}
static V get(ppc::Ctx& c,uint32_t p) {return {ppc::rdf32(c,p),ppc::rdf32(c,p+4),ppc::rdf32(c,p+8)};}
static void near(V a,V b,float eps=2e-5f) {for(unsigned i=0;i<3;++i)CHECK(std::abs(a[i]-b[i])<eps);}
static float dot(V a,V b) {float n=0;for(unsigned i=0;i<3;++i)n+=a[i]*b[i];return n;}
static void setup(ppc::Ctx& c,unsigned player=0) {
    c.r[13]=0x80500000;ppc::wr32(c,c.r[13]-27304,scene);
    // Camera at world (3,4,5), turned 90 degrees around Y:
    // view = (world.z-5, world.y-4, -world.x+3).
    put(c,camera,{0,0,1});ppc::wrf32(c,camera+12,-5);
    put(c,camera+16,{0,1,0});ppc::wrf32(c,camera+28,-4);
    put(c,camera+32,{-1,0,0});ppc::wrf32(c,camera+44,3);
    put(c,object+80,{3,4,5});put(c,object+92,{1,0,0});
    put(c,scene+player*7364+20180,{103,4,5});
    put(c,object+104,{.1f,.2f,.3f});
    ppc::wr32(c,object+136,0x1234);ppc::wr32(c,object+140,9);
}
static void collision_and_trail_start_at_the_laser() {
    Fix f;setup(f.c);
    const uc::TrackedRay ray{{2,-1,-4},{0,.6f,-.8f}};
    CHECK(uc::align_xr_projectile(f.c,object,0,ray));
    const V origin{7,3,7},direction{.8f,.6f,0};
    near(get(f.c,object+80),origin);near(get(f.c,object+92),direction);
    near(get(f.c,object+104),{});near(get(f.c,object+116),origin);
    for(unsigned i=0;i<4;++i)near(get(f.c,object+156+i*12),origin);
    CHECK(ppc::rd32(f.c,object+136)==0x1234 && ppc::rd32(f.c,object+140)==9);
    // Close and far collision points project onto the same view-space laser.
    for(float distance:{.2f,1.f,50.f}) {
        V p=origin;for(unsigned i=0;i<3;++i)p[i]+=distance*direction[i];
        const V view{p[2]-5,p[1]-4,-p[0]+3};
        V expected=ray.origin;for(unsigned i=0;i<3;++i)expected[i]+=distance*ray.direction[i];
        near(view,expected);
    }
}
static void spread_and_invalid_input() {
    Fix f;setup(f.c,1);put(f.c,object+92,{.8f,.6f,0});
    CHECK(uc::align_xr_projectile(f.c,object,1,{{0,0,0},{0,0,1}}));
    const auto pellet=get(f.c,object+92);
    CHECK(std::abs(dot(pellet,{-1,0,0})-.8f)<2e-5f && std::abs(dot(pellet,pellet)-1)<2e-5f);
    setup(f.c);ppc::wrf32(f.c,camera,2);
    CHECK(!uc::align_xr_projectile(f.c,object,0,{{0,0,0},{0,0,-1}}));
    near(get(f.c,object+80),{3,4,5});
    setup(f.c);
    CHECK(!uc::align_xr_projectile(f.c,object,0,{{0,0,0},{0,0,0}}));
    CHECK(!uc::align_xr_projectile(f.c,0xCC000000,0,{{0,0,0},{0,0,-1}}));
    CHECK(!uc::align_xr_projectile(f.c,object,2,{{0,0,0},{0,0,-1}}));
}
static void overlay_and_projectile_use_identical_rays() {
    uc::xr::StereoSettings settings;settings.scale=10;
    uc::xr::StereoScene scene(settings);
    std::array<uc::xr::StereoPose,2> eyes;scene.publish(eyes);
    uc::xr::ControllerSettings controls;controls.laser=1;
    uc::xr::ControllerInput input(controls);
    for(float depth:{.6f,2.f,15.f}) {
        uc::xr::ControllerSample sample;sample.valid={1,1};sample.aim_depth={depth,depth};
        sample.pose[0].position={-.2f,-.1f,-.15f};sample.pose[1].position={.2f,-.1f,-.15f};
        input.update(scene,sample,1000000000,true);
        uc::xr::ControllerSnapshot snapshot;CHECK(input.consume(snapshot));CHECK(snapshot.valid[0]);
        const auto& overlay=input.overlay_frames()[0];const auto& ray=snapshot.shot[0];
        float length=0;V delta{};
        for(unsigned i=0;i<3;++i){CHECK(std::abs(ray.origin[i]-overlay.muzzle[i]*10)<1e-6f);delta[i]=overlay.end[i]-overlay.muzzle[i];length+=delta[i]*delta[i];}
        for(float& v:delta)v/=std::sqrt(length);
        near(ray.direction,delta);
    }
}
static void shots_leave_the_rendered_barrel_even_at_close_range() {
    uc::xr::StereoSettings settings;settings.scale=10;
    uc::xr::StereoScene scene(settings);
    std::array<uc::xr::StereoPose,2> eyes;scene.publish(eyes);
    // Model metadata, checked independently through the same transform used to
    // draw the gun. Include recoil, translated hands and an off-axis barrel.
    for(int model:{XO_GUN_SMG,XO_GUN_TP9,XO_GUN_GHOST_SQUAD}) {
        const float z=model==XO_GUN_TP9?-.2060f:model==XO_GUN_GHOST_SQUAD?-.3102f:-.2605f;
        uc::xr::ControllerSettings controls;controls.gun_model=model;
        uc::xr::ControllerInput input(controls);
        for(float yaw:{0.f,.15f}) for(float x:{-.02f,.02f}) for(float reach:{.02f,.05f,1.f,15.f}) {
            uc::xr::ControllerSample sample;sample.valid[1]=1;
            sample.pose[1].position={x,0,-.15f};sample.pose[1].orientation={0,std::sin(yaw/2),0,std::cos(yaw/2)};
            sample.buttons.trigger[1]=1;
            sample.aim_depth[1]=.15f+(reach-z)*std::cos(yaw);
            input.update(scene,sample,1000000000,true);
            uc::xr::ControllerSnapshot snapshot;CHECK(input.consume(snapshot));CHECK(snapshot.valid[0]);
            const auto& overlay=input.overlay_frames()[0];const auto& ray=snapshot.shot[0];
            float matrix[16];xo_pose_matrix(overlay.gun_q,overlay.gun_p,matrix);
            V expected{},forward{};
            for(unsigned i=0;i<3;++i){expected[i]=(matrix[12+i]+matrix[8+i]*(z+overlay.recoil))*10;forward[i]=-matrix[8+i];}
            near(ray.origin,expected);near(ray.direction,forward);
            for(unsigned i=0;i<3;++i) CHECK(std::abs(overlay.end[i]-(sample.pose[1].position[i]+forward[i]*(reach-z)))<2e-5f);
        }
    }
    // A wall between the grip and barrel must not make the forward ray reverse.
    uc::xr::StereoPose hand;hand.position={0,0,-.15f};
    const auto muzzle=uc::xr::gun_muzzle_pose(hand,XO_GUN_TP9);
    uc::AimGeometry trace;trace.set_ray(1,muzzle,10);
    const auto wall=[](float z) {
        gx::DrawPacket d;d.primitive=0x90;d.vertices.resize(3);d.indices={0,1,2};
        d.vertices[0].position={-1,-1,z};d.vertices[1].position={1,-1,z};d.vertices[2].position={0,1,z};return d;
    };
    trace.draw(wall(-3));CHECK(trace.depth(1,10)==0);
    trace.draw(wall(-3.76f));CHECK(std::abs(trace.depth(1,10)-.376f)<1e-5f);
}
static void hook_uses_the_consumed_controller_ray_only_for_player_shots() {
    Fix f;auto& c=f.c;uc::NativeInput input;input.focus(true);uc::input_hle_init(&input);
    input.host_xr_pointer(0,0,uc::TrackedRay{{2,-1,-4},{0,0,-1}});input.consume();
    setup(c);c.lr=0x8006B97C;c.r[3]=object;c.r[7]=0;
    {uc::XrShotScope hook(c);}
    near(get(c,object+80),{7,3,7});near(get(c,object+92),{1,0,0});
    setup(c);c.lr=0x80000000;c.r[3]=object;c.r[7]=0;
    {uc::XrShotScope hook(c);}
    near(get(c,object+80),{3,4,5});
    uc::input_hle_init(nullptr);c.lr=0x8006B97C;
    {uc::XrShotScope hook(c);}
    near(get(c,object+80),{3,4,5});
}
#if defined(UC_TITLE_DC)
// SBDE08 (common/title.h, kDcBulletInit): the same camera as above, as camera 1 of a camera system.
constexpr uint32_t dc_bullet=0x80020000,dc_system=0x80200000,dc_start=0x80030000,dc_end=0x80030010;
static uint32_t dc_setup(ppc::Ctx& c,unsigned player=0) {
    using namespace uc::title;
    ppc::wr32(c,kDcCameraManager+4,dc_system);ppc::wr8(c,dc_system+kDcCameraIndex,1);
    const uint32_t view=dc_system+kDcCameraFirst+kDcCameraSize;
    put(c,view,{0,0,1});ppc::wrf32(c,view+12,-5);
    put(c,view+16,{0,1,0});ppc::wrf32(c,view+28,-4);
    put(c,view+32,{-1,0,0});ppc::wrf32(c,view+44,3);
    ppc::wr32(c,view+kDcCameraFlags,kDcCameraClean|1);
    put(c,dc_bullet+kDcBulletOrigin,{3,4,5});put(c,dc_bullet+kDcBulletDirection,{1,0,0});
    put(c,dc_bullet+kDcBulletOffset,{.1f,.2f,.3f});put(c,dc_bullet+kDcBulletStart,{3.1f,4.2f,5.3f});
    put(c,kDcAim+player*kDcAimSize+kDcAimHit,{103,4,5});put(c,kDcAim+player*kDcAimSize+kDcAimFar,{103,4,5});
    put(c,dc_start,{3,4,5});put(c,dc_end,{103,4,5});
    return view;
}
static void dc_bullet_and_trail_start_at_the_ray() {
    using namespace uc::title;
    Fix f;dc_setup(f.c);
    CHECK(uc::align_dc_bullet(f.c,dc_bullet,0,{{2,-1,-4},{0,.6f,-.8f}}));
    near(get(f.c,dc_bullet+kDcBulletOrigin),{7,3,7});near(get(f.c,dc_bullet+kDcBulletDirection),{.8f,.6f,0});
    near(get(f.c,dc_bullet+kDcBulletOffset),{});near(get(f.c,dc_bullet+kDcBulletStart),{7,3,7});
    // The spread of a pellet stays around the ray.
    dc_setup(f.c,1);put(f.c,dc_bullet+kDcBulletDirection,{.8f,.6f,0});
    CHECK(uc::align_dc_bullet(f.c,dc_bullet,1,{{0,0,0},{0,0,1}}));
    const auto pellet=get(f.c,dc_bullet+kDcBulletDirection);
    CHECK(std::abs(dot(pellet,{-1,0,0})-.8f)<2e-5f && std::abs(dot(pellet,pellet)-1)<2e-5f);
    // A stale matrix, a matrix that is no rotation, no direction, a bad address and a third player change nothing.
    uint32_t view=dc_setup(f.c);ppc::wr32(f.c,view+kDcCameraFlags,1);
    CHECK(!uc::align_dc_bullet(f.c,dc_bullet,0,{{0,0,0},{0,0,-1}}));
    view=dc_setup(f.c);ppc::wrf32(f.c,view,2);
    CHECK(!uc::align_dc_bullet(f.c,dc_bullet,0,{{0,0,0},{0,0,-1}}));
    dc_setup(f.c);
    CHECK(!uc::align_dc_bullet(f.c,dc_bullet,0,{{0,0,0},{0,0,0}}));
    CHECK(!uc::align_dc_bullet(f.c,0xCC000000,0,{{0,0,0},{0,0,-1}}));
    CHECK(!uc::align_dc_bullet(f.c,dc_bullet,2,{{0,0,0},{0,0,-1}}));
    near(get(f.c,dc_bullet+kDcBulletOrigin),{3,4,5});near(get(f.c,dc_bullet+kDcBulletStart),{3.1f,4.2f,5.3f});
}
static void dc_hooks_use_the_ray_only_at_their_call_sites() {
    using namespace uc::title;
    Fix f;auto& c=f.c;uc::NativeInput input;input.focus(true);uc::input_hle_init(&input);
    input.host_xr_pointer(0,0,uc::TrackedRay{{2,-1,-4},{0,0,-1}});input.consume();
    // The bullet of a weapon (r4 = 0, from 800B6AE4), and a bullet with its own origin.
    dc_setup(c);c.lr=kDcBulletInitReturn;c.r[3]=dc_bullet;c.r[4]=0;c.r[7]=0;
    {uc::DcShotScope hook(c);}
    near(get(c,dc_bullet+kDcBulletOrigin),{7,3,7});near(get(c,dc_bullet+kDcBulletDirection),{1,0,0});
    dc_setup(c);c.r[4]=dc_start;
    {uc::DcShotScope hook(c);}
    near(get(c,dc_bullet+kDcBulletOrigin),{3,4,5});
    dc_setup(c);c.lr=0x80000000;c.r[4]=0;
    {uc::DcShotScope hook(c);}
    near(get(c,dc_bullet+kDcBulletOrigin),{3,4,5});
    // The query of the pointer: the start and the far point are on the ray while the query runs. The start comes back after it.
    dc_setup(c);c.lr=kDcPointerQueryReturn;c.r[4]=0;c.r[5]=dc_start;c.r[6]=kDcAim+kDcAimHit;
    {
        uc::DcPickScope hook(c);
        near(get(c,dc_start),{7,3,7});near(get(c,kDcAim+kDcAimHit),{107,3,7},1e-4f);near(get(c,kDcAim+kDcAimFar),{107,3,7},1e-4f);
    }
    near(get(c,dc_start),{3,4,5});
    // The query of the item pick-up: only the start moves.
    dc_setup(c);c.lr=kDcPickupQueryReturn;c.r[4]=0;c.r[5]=dc_start;c.r[6]=dc_end;
    {
        uc::DcPickScope hook(c);
        near(get(c,dc_start),{7,3,7});near(get(c,dc_end),{103,4,5});near(get(c,kDcAim+kDcAimFar),{103,4,5});
    }
    near(get(c,dc_start),{3,4,5});
    // Another caller of the query, and a run without a tracked ray.
    c.lr=0x800B5344;
    {uc::DcPickScope hook(c);near(get(c,dc_start),{3,4,5});}
    uc::input_hle_init(nullptr);
    c.lr=kDcPointerQueryReturn;c.r[6]=kDcAim+kDcAimHit;
    {uc::DcPickScope hook(c);near(get(c,dc_start),{3,4,5});near(get(c,kDcAim+kDcAimHit),{103,4,5});}
    c.lr=kDcBulletInitReturn;c.r[3]=dc_bullet;c.r[4]=0;c.r[7]=0;
    {uc::DcShotScope hook(c);}
    near(get(c,dc_bullet+kDcBulletOrigin),{3,4,5});
}
#endif
int main() {
    hook_uses_the_consumed_controller_ray_only_for_player_shots();
    collision_and_trail_start_at_the_laser();spread_and_invalid_input();overlay_and_projectile_use_identical_rays();
    shots_leave_the_rendered_barrel_even_at_close_range();
#if defined(UC_TITLE_DC)
    dc_bullet_and_trail_start_at_the_ray();dc_hooks_use_the_ray_only_at_their_call_sites();
#endif
}
