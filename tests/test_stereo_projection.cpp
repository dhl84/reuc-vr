#include "check.h"
#include "gl_clip.h"
#include "space_warp.h"
#include <cmath>

using namespace uc;
static void world_projection_keeps_eye_translation(float near_plane) {
    xr::StereoSettings settings; settings.scale=10;
    xr::StereoScene scene(settings);
    std::array<xr::StereoPose,2> eyes;
    eyes[0].position={-.032f,.17f,.19f}; eyes[1].position={.032f,.17f,.19f};
    eyes[0].tangents={1.3f,.8f,1.4f,.9f}; eyes[1].tangents={.8f,1.3f,1.4f,.9f};
    scene.publish(eyes);
    // Infinite-far perspective: for a tiny near plane, projecting z=-10 to
    // float clip space rounds z to w. Recovering view w from z-w then loses
    // the eye translation. Direct projection must retain it.
    xr::Matrix p{}; p[0]=1.2f; p[5]=1.6f; p[10]=-1; p[11]=-1; p[14]=-2*near_plane;
    const auto& matrices=scene.transforms(p,LG_STEREO_WORLD);
    float rows[32]; stereo_view_block(matrices,rows);
    const float point[4]={.5f,.25f,-10,1};
    for (int eye=0;eye<2;++eye) {
        float clip[4]{};
        for (int r=0;r<4;++r) for (int c=0;c<4;++c) clip[r]+=rows[16*eye+4*r+c]*point[c];
        const auto& t=eyes[eye].tangents;
        const double x=.5-double(eyes[eye].position[0])*10;
        const double y=.25-double(eyes[eye].position[1])*10;
        const double z=-10-double(eyes[eye].position[2])*10;
        const double expected_x=2*x/(t[0]+t[1])+(t[1]-t[0])*z/(t[0]+t[1]);
        const double expected_y=-(2*y/(t[2]+t[3])+(t[3]-t[2])*z/(t[2]+t[3]));
        CHECK(std::abs(clip[0]-expected_x)<2e-6);
        CHECK(std::abs(clip[1]-expected_y)<2e-6);
        CHECK(std::abs(clip[2]-(-z-near_plane))<2e-6);
        CHECK(std::abs(clip[3]+z)<2e-6);
    }
    // The pose cache must rebuild the direct transform after a new sample.
    eyes[0].position[2]+=.1f; scene.publish(eyes);
    const auto& moved=scene.transforms(p,LG_STEREO_WORLD);
    CHECK(std::abs(moved.from_view[0][15]-2.9f)<1e-6f);
}
// The space warp's depth planes (space_warp.h) are the ones lg_stereo_eye_projection recovers from the world lens.
static void space_warp_depth_range() {
    const float n=.5f, f=200.f;
    xr::Matrix p{}; p[0]=1.2f; p[5]=1.6f; p[10]=-(f+n)/(f-n); p[11]=-1; p[14]=-2*f*n/(f-n);
    const auto range=warp_depth_range(p.data());
    CHECK(range && std::abs(range->near_plane-n)<1e-5f && std::abs(range->far_plane-f)<1e-2f);
    p[10]=-1; p[14]=-2*n;   // infinite far plane: 10^4 x near
    const auto infinite=warp_depth_range(p.data());
    CHECK(infinite && std::abs(infinite->near_plane-n)<1e-6f && std::abs(infinite->far_plane-n*1e4f)<1e-2f);
    xr::Matrix ortho{}; ortho[0]=ortho[5]=1; ortho[10]=-1; ortho[15]=1;   // not perspective: none
    CHECK(!warp_depth_range(ortho.data()));
    // the game's 1 mm near plane (in its decimetres: .01) raised to 10 cm; the motion pass's lens has exactly those planes
    p[10]=-(f+.01f)/(f-.01f); p[14]=-2*f*.01f/(f-.01f);
    const auto raised=warp_depth_range(p.data(),1.f);
    CHECK(raised && std::abs(raised->near_plane-1.f)<1e-6f && std::abs(raised->far_plane-f)<.01f*f);   // a 1 mm lens's far plane: float-rounded
    xr::Matrix lens; warp_projection(p.data(),*raised,lens.data());
    const auto back=warp_depth_range(lens.data());
    CHECK(back && std::abs(back->near_plane-1.f)<1e-4f && std::abs(back->far_plane-raised->far_plane)<.05f && lens[0]==p[0] && lens[5]==p[5] && lens[11]==-1);
}
// The lens of the aim (StereoScene::observe_aim): the projection of most world draws of a frame. Before a frame it is the screen lens.
// The pointer of a world aim goes through it, and the lens of the screen for the menus and the HUD stays.
static void aim_lens_follows_world_draws() {
    xr::StereoSettings settings; settings.scale=10;
    xr::StereoScene scene(settings);
    std::array<xr::StereoPose,2> eyes;
    eyes[0].position={-.032f,0,0}; eyes[1].position={.032f,0,0};
    eyes[0].tangents={1.3f,.8f,1.4f,.9f}; eyes[1].tangents={.8f,1.3f,1.4f,.9f};
    scene.publish(eyes);
    scene.set_screen_lens({.974f,1.732f});
    CHECK(scene.aim_lens()[0]==.974f && scene.aim_lens()[1]==1.732f);
    // a controller 0.5 m to the right and 0.25 m up that points straight ahead: its hit on the plane 10 m away has the tangents 0.05 and 0.025
    xr::StereoPose controller; controller.position={.5f,.25f,0}; controller.orientation={0,0,0,1};
    xr::StereoRay ray;
    CHECK(scene.aim(controller,1280,720,false,10.f,ray) && std::abs(ray.px-640*(1+.05f*.974f))<.01f && std::abs(ray.py-360*(1-.025f*1.732f))<.01f);
    xr::Matrix wide{}; wide[0]=.6f; wide[5]=.711f; wide[10]=-1; wide[11]=-1; wide[14]=-.2f;
    xr::Matrix other=wide; other[0]=1.5f; other[5]=1.5f;
    scene.observe_aim(other);
    for (int i=0;i<3;++i) scene.observe_aim(wide);
    scene.finish_frame();
    CHECK(scene.aim_lens()[0]==.6f && scene.aim_lens()[1]==.711f);
    CHECK(scene.screen_lens()[0]==.974f && scene.screen_lens()[1]==1.732f);
    CHECK(scene.aim(controller,1280,720,false,10.f,ray) && std::abs(ray.px-640*(1+.05f*.6f))<.01f && std::abs(ray.py-360*(1-.025f*.711f))<.01f);
    scene.finish_frame();   // a frame without a world draw keeps the lens
    CHECK(scene.aim_lens()[0]==.6f && scene.aim_lens()[1]==.711f);
}
// The backdrop (LG_STEREO_BACKDROP): the eyes turn with the head and stay at the place of the camera of the game.
static void backdrop_keeps_the_place_of_the_camera() {
    xr::StereoSettings settings; settings.scale=10;
    xr::StereoScene scene(settings);
    std::array<xr::StereoPose,2> eyes;
    eyes[0].position={-.032f,-.4f,.3f}; eyes[1].position={.032f,-.4f,.3f};   // a head 40 cm below and 30 cm behind its start
    eyes[0].tangents={1.3f,.8f,1.4f,.9f}; eyes[1].tangents={.8f,1.3f,1.4f,.9f};
    scene.publish(eyes);
    xr::Matrix p{}; p[0]=1.2f; p[5]=1.6f; p[10]=-1; p[11]=-1; p[14]=-.2f;
    float rows[32]; stereo_view_block(scene.transforms(p,xr::LG_STEREO_BACKDROP),rows);
    const float point[4]={.5f,1.f,-10,1};   // a point of a sky sheet 10 cm above the camera
    for (int eye=0;eye<2;++eye) {
        float clip[4]{};
        for (int r=0;r<4;++r) for (int c=0;c<4;++c) clip[r]+=rows[16*eye+4*r+c]*point[c];
        const auto& t=eyes[eye].tangents;
        const double expected_x=2*.5/(t[0]+t[1])+(t[1]-t[0])*-10/(t[0]+t[1]);
        const double expected_y=-(2*1./(t[2]+t[3])+(t[3]-t[2])*-10/(t[2]+t[3]));
        CHECK(std::abs(clip[0]-expected_x)<2e-6);
        CHECK(std::abs(clip[1]-expected_y)<2e-6);
        CHECK(std::abs(clip[3]-10)<2e-6);
    }
    float world[32]; stereo_view_block(scene.transforms(p,LG_STEREO_WORLD),world);   // the rest of the world moves with the head
    CHECK(std::abs(world[7]-rows[7])>1 && std::abs(world[15]-rows[15])>1);
    // a turned head turns the backdrop
    const float half=.2f; eyes[0].orientation=eyes[1].orientation={0,std::sin(half),0,std::cos(half)};
    scene.publish(eyes);
    float turned[32]; stereo_view_block(scene.transforms(p,xr::LG_STEREO_BACKDROP),turned);
    CHECK(std::abs(turned[0]-rows[0])>.01f);
}
int main() {
    backdrop_keeps_the_place_of_the_camera();
    aim_lens_follows_world_draws();
    world_projection_keeps_eye_translation(.1f);
    world_projection_keeps_eye_translation(1e-9f);
    space_warp_depth_range();
}
