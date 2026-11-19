#include "title.h"
#include "intro.h"
#include "dispatch.h"
#include "gx_vertices.h"
#include "wide_view.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <tuple>
#include <stdexcept>

namespace {
bool skip_intro; // assigned before the scheduler starts; guest baton owns subsequent access
constexpr uint32_t kStartupLogos=0x804D1E6C;
}
namespace uc { void intro_init(bool skip) { skip_intro=skip; } }

void hle_IntroLogoUpdate(ppc::Ctx& c) {
    const auto object=c.r[3];
    if (skip_intro && object==kStartupLogos && ppc::rd8(c,object+4) && ppc::rd8(c,object+5) && !ppc::rd8(c,object+0x28)) {
        const uint32_t count=ppc::rd32(c,object+0x38), rows=ppc::rd32(c,object+0x3C);
        gx::Memory memory{{reinterpret_cast<const std::byte*>(c.mem),24u<<20},{reinterpret_cast<const std::byte*>(c.mem+0x10000000),64u<<20},{}};
        if (!count) throw std::runtime_error("startup logo table is empty");
        memory.read(rows,size_t(count)*52);
        const uint64_t last=uint64_t(rows)+uint64_t(count-1)*52;
        if (last>UINT32_MAX-48) throw std::runtime_error("startup logo table address overflows");
        const float duration=ppc::rdf32(c,uint32_t(last)+48);
        if (!std::isfinite(duration) || duration<0) throw std::runtime_error("invalid startup logo duration");
        // Enter the translated update's normal final-row completion branch.
        ppc::wr32(c,object+0x34,count-1);
        ppc::wr32(c,object+0x24,4);
        ppc::wrf32(c,object+0x2C,duration);
        std::fprintf(stderr,"[intro] skipped Nintendo/CAPCOM/CRIWARE startup screens\n");
    }
    // Static lowering of8024585C's stack store;80245860 continues the original compiled body.
    const auto stack=c.r[1];
    ppc::wr32(c,stack-48,stack);
    c.r[1]=stack-48;
    uc::call_indirect(c,0x80245860);
}

// C_MTXPerspective(Mtx44 m, f32 fovY, f32 aspect, f32 near, f32 far): m[0][0] = cot(fovY / 2) / aspect. On a wider output the aspect grows by 1 / k
// (renderer/wide_view.h), so every projection the game builds, and everything it derives from it (the pointer's range, hit tests, culling), sees
// the wide picture. A taller output shrinks the field of view's tangent on the y axis instead. At 16:9 (or with the option off) the original runs unchanged.
void hle_MTXPerspective(ppc::Ctx& c) {
    // Static lowering of 80265464's stwu r1,-64(r1); 80265468 continues the original compiled body.
    const auto stack=c.r[1];
    ppc::wr32(c,stack-64,stack);
    c.r[1]=stack-64;
    {
        const auto factors=gx::view_factors();
        const float kx=factors.x,ky=factors.y;
        float aspect=float(c.f[2]),fov=float(c.f[1]);
        const float aspect_in=aspect,fov_in=fov;
        if((kx<1.f || ky<1.f) && std::isfinite(aspect) && aspect>0 && std::isfinite(fov) && fov>0 && fov<180) {   // at 16:9 the arguments are not touched at all
            if(kx<1.f) aspect/=kx;
            if(ky<1.f) {   // the vertical tangent grows by 1 / k_y; the horizontal one stays: cot' = cot * k_y, aspect' = aspect * k_y
                const double tangent=std::tan(double(fov)*3.14159265358979323846/360.0);
                fov=float(std::atan(tangent/ky)*360.0/3.14159265358979323846);
                aspect*=ky;
            }
            c.f[1]=fov; c.f[2]=aspect;
        }
        const float vr=gx::vr_fov_scale().load(std::memory_order_relaxed);
        const float vr_y_set=gx::vr_fov_scale_y().load(std::memory_order_relaxed);
        const float vr_y=vr_y_set>0.f ? vr_y_set : vr;
        fov=float(c.f[1]);
        aspect=float(c.f[2]);
        // SBDE08 (title.h kWideViewScreenAspectOnly): the game also builds a projection that is not the view of the screen (40 degrees
        // with the aspect 1.0, near 1, far 500). Only a projection with the shape of the screen (16:9 or 4:3) gets the wide view.
        const bool screen_shape=!uc::title::kWideViewScreenAspectOnly || std::abs(aspect-16.f/9)<.02f || std::abs(aspect-4.f/3)<.02f;
        if(screen_shape && (vr>1.f || vr_y>1.f) && std::isfinite(fov) && fov>0 && fov<180 && std::isfinite(aspect) && aspect>0) {
            // VR: the vertical tangent grows by vr_y and the horizontal one by vr (tan_x = tan_y * aspect, so the aspect takes vr / vr_y);
            // culling then covers the headset's view. Each eye is reprojected from the world, so only the culled objects come back.
            const double tangent=std::tan(double(fov)*3.14159265358979323846/360.0)*std::max(1.f,vr_y);
            c.f[1]=std::min(170.0,std::atan(tangent)*360.0/3.14159265358979323846);
            c.f[2]=aspect*std::max(1.f,vr)/std::max(1.f,vr_y);
        }
        // UC_PROJ_LOG=1 (diagnostics): each caller of MTXPerspective with its field of view and aspect, once for each new value.
        static const bool projection_log=std::getenv("UC_PROJ_LOG")!=nullptr;
        if(projection_log) {
            static std::set<std::tuple<uint32_t,long,long>> seen;
            if(seen.size()<200 && seen.insert({c.lr,std::lround(fov_in*100),std::lround(aspect_in*1000)}).second)
                std::fprintf(stderr,"[proj] MTXPerspective caller %08X: fovy %.3f aspect %.4f -> fovy %.3f aspect %.4f, near %.3f far %.1f\n",c.lr,fov_in,aspect_in,float(c.f[1]),float(c.f[2]),float(c.f[3]),float(c.f[4]));
        }
    }
    uc::call_indirect(c,UC_ADDR(0x80265468));   // SBDE08: 80383B94 (project/dc/dc.cfg)
}

// SBDE08 8032DCE4 (one caller, 8032CD10): the game tests the sphere of each part of a stage model against its own view cone (80330C60)
// and hides the parts outside (804D02FC). The cone comes from the camera of the game (8032F274) and not from the projection matrix, so
// the wide view does not widen it: the cone is the picture of the console. In the wide view of the headset the parts outside that cone
// are inside the picture, and whole houses of the stage came and went with each turn of the game camera. With a wide view the test does
// not run and each part stays in the picture. In one run of the first chapter the test hid parts in 3 % of the frames, so the cost is small.
// UC_DC_PARTS_VIEW_TEST=1 (diagnostics) runs the test of the game in the wide view too.
void hle_DcStagePartsViewTest(ppc::Ctx& c) {
    static const bool keep_test=std::getenv("UC_DC_PARTS_VIEW_TEST")!=nullptr;
    const auto factors=gx::view_factors();
    const bool wide=factors.x<1.f || factors.y<1.f || gx::vr_fov_scale().load(std::memory_order_relaxed)>1.f ||
                    gx::vr_fov_scale_y().load(std::memory_order_relaxed)>1.f;   // a wider output, or the wide view of the headset
    if(wide && !keep_test) return;
    // Static lowering of 8032DCE4's stwu r1,-272(r1); 8032DCE8 continues the original compiled body (project/dc/dc.cfg).
    const auto stack=c.r[1];
    ppc::wr32(c,stack-272,stack);
    c.r[1]=stack-272;
    uc::call_indirect(c,0x8032DCE8u);
}

void hle_MovieUpdate(ppc::Ctx& c) {
    const auto object=c.r[3];
    gx::Memory memory{{reinterpret_cast<const std::byte*>(c.mem),24u<<20},{reinterpret_cast<const std::byte*>(c.mem+0x10000000),64u<<20},{}};
    memory.read(object,0x50);
    if(ppc::rd16(c,object+6)==1) {
        memory.read(c.r[2]-25960,4);
        const auto minimum=ppc::rdf32(c,c.r[2]-25960);
        if(!std::isfinite(minimum) || minimum<0) throw std::runtime_error("invalid movie skip threshold");
        ppc::wrf32(c,object+8,minimum);
    }
    const auto update=[&] {
        // Static lowering of801426D4; the remaining update and cleanup stay AOT compiled.
        const auto stack=c.r[1];
        ppc::wr32(c,stack-32,stack); c.r[1]=stack-32;
        c.r[3]=object; uc::call_indirect(c,0x801426D8);
    };
    const auto finish_fade=[&] {
        ppc::wr32(c,object,uint32_t(-950));
        ppc::wr16(c,object+4,247); // Next +8 reaches255 and runs the original decoder cleanup.
    };
    if(ppc::rd16(c,object+6)==2) finish_fade();
    update();
    if(ppc::rd16(c,object+6)==2) { finish_fade(); update(); }
}
