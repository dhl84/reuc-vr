#pragma once
// A draw of the world that looks a copy of the picture up at the pixel's own place on the screen: water that shows the scene behind it,
// glass, heat haze. Its texgen takes the position through the camera of the game to the screen, so the coordinate is the place of the
// pixel in the picture of that one camera. In the headset each eye has its own picture, and the copy holds the picture of that eye. The
// coordinate of the game then names another place of the eye's picture. The water showed parts of the scene that lie beside it, and
// they moved with the head (SBDE08, the village of the first chapter).
// screen_lookup() finds such a coordinate from the matrices of the draw alone: the three rows of its texgen (texture matrix, then post
// matrix) are multiples of the x, y and w rows of the camera's clip transform. The vertex stage of the headset then makes the
// coordinate from the clip position of the eye (smoke.slang, StereoBlock::lookup).
#include "gx_vertices.h"
#include <array>
#include <cmath>
#include <optional>

namespace gx {
// Coordinate `id`: s / q = s[0] * x / w + s[1] and t / q = t[0] * y / w + t[1], with (x, y, w) the clip position.
struct ScreenLookup { uint32_t id=0; std::array<float,2> s{}, t{}; };
namespace screen_lookup_detail {
using Row=std::array<double,4>;
inline Row scaled(const std::array<float,4>& r,double k) { return {r[0]*k,r[1]*k,r[2]*k,r[3]*k}; }
inline Row add(const Row& a,const Row& b) { return {a[0]+b[0],a[1]+b[1],a[2]+b[2],a[3]+b[3]}; }
inline double dot(const Row& a,const Row& b) { return a[0]*b[0]+a[1]*b[1]+a[2]*b[2]+a[3]*b[3]; }
// target = k[0] * a + k[1] * b by least squares; false when a and b are parallel or the rest is more than a thousandth of the target.
inline bool fit(const Row& target,const Row& a,const Row& b,std::array<double,2>& k) {
    const double aa=dot(a,a),ab=dot(a,b),bb=dot(b,b),det=aa*bb-ab*ab;
    if (!(std::abs(det)>1e-12*aa*bb) || !(aa>0) || !(bb>0)) return false;
    const double ta=dot(target,a),tb=dot(target,b);
    k={(ta*bb-tb*ab)/det,(tb*aa-ta*ab)/det};
    Row rest; for (int i=0;i<4;++i) rest[i]=target[i]-k[0]*a[i]-k[1]*b[i];
    return dot(rest,rest)<=1e-6*dot(target,target);
}
}
inline std::optional<ScreenLookup> screen_lookup(const DrawPacket& draw) {
    using namespace screen_lookup_detail;
    const auto& t=*draw.transforms; const auto& u=*draw.uniforms;
    if (t.projection[3][3]!=0 || draw.state->descriptors[0]!=Mode::none || draw.texture_bank || t.current>61) return std::nullopt;
    // the clip rows of the camera over the object position (x, y, z, 1)
    const auto row=[&](int k) {
        Row r{0,0,0,t.projection[k][3]};
        for (int i=0;i<3;++i) r=add(r,scaled(t.position_rows[t.current+i],t.projection[k][i]));
        return r;
    };
    const Row clip_x=row(0),clip_y=row(1),clip_w=row(3);
    if (!(dot(clip_w,clip_w)>0)) return std::nullopt;
    uint32_t seen=0;
    for (uint32_t stage=0;stage<draw.material.stages && stage<16;++stage) {
        const uint32_t order=draw.material.stage[stage].order;
        if (!(order&64)) continue;
        const uint32_t map=order&7,id=(order>>3)&7;
        if (seen&(1u<<id)) continue;
        seen|=1u<<id;
        const auto& texture=map ? draw.texture_maps[map-1] : draw.texture;
        if (!texture || !texture->gpu_only) continue;   // a copy of the frame buffer only
        const uint32_t gen=u.tex_gens[id];
        // a regular texgen from the position, with a third row (STQ), without the normalization of the post transform
        if (((gen>>7)&31)!=0 || ((gen>>4)&7)!=0 || !(gen&2) || (u.tex_posts[id]&256)) continue;
        Row pre[3];
        for (int i=0;i<3;++i) {
            const auto& r=u.tex_rows[id*3+i];
            pre[i]=(gen&4) ? Row{r[0],r[1],r[2],r[3]} : Row{r[0],r[1],0,double(r[2])+r[3]};   // input form AB11: z is 1
        }
        Row out[3];
        for (int j=0;j<3;++j) {
            const auto& q=u.post_rows[id*3+j];
            out[j]={0,0,0,q[3]};
            for (int i=0;i<3;++i) for (int c=0;c<4;++c) out[j][c]+=q[i]*pre[i][c];
        }
        const double e=dot(out[2],clip_w)/dot(clip_w,clip_w);
        Row rest; for (int c=0;c<4;++c) rest[c]=out[2][c]-e*clip_w[c];
        if (!(std::abs(e)>1e-12) || dot(rest,rest)>1e-6*dot(out[2],out[2])) continue;
        std::array<double,2> s,v;
        if (!fit(out[0],clip_x,clip_w,s) || !fit(out[1],clip_y,clip_w,v)) continue;
        ScreenLookup found; found.id=id;
        found.s={float(s[0]/e),float(s[1]/e)}; found.t={float(v[0]/e),float(v[1]/e)};
        if (!std::isfinite(found.s[0]) || !std::isfinite(found.s[1]) || !std::isfinite(found.t[0]) || !std::isfinite(found.t[1])) continue;
        return found;
    }
    return std::nullopt;
}
}
