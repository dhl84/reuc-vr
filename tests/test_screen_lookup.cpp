#include "check.h"
#include "screen_lookup.h"
#include "gx_textures.h"
#include <cmath>
// renderer/screen_lookup.h: the texgen of a draw that looks a copy of the picture up at the pixel's own place.
namespace {
// A camera of the game: a view matrix with a rotation and a move, and a perspective projection.
gx::Transforms camera() {
    gx::Transforms t;
    const float c=std::cos(0.4f),s=std::sin(0.4f);
    t.position_rows[0]={c,0,s,3}; t.position_rows[1]={0,1,0,-7}; t.position_rows[2]={-s,0,c,-40};
    t.projection={{{1.3f,0,0,0},{0,-2.3f,0,0},{0,0,-1.0002f,-0.2f},{0,0,-1,0}}};
    return t;
}
// The texgen of coordinate `id`: the texture matrix is the view matrix, and the post matrix takes the view position to
// (ks * x / w + bs, kt * y / w + bt) of the clip position, times q = -z.
gx::DrawUniform screen_texgen(const gx::Transforms& t,uint32_t id,float ks,float bs,float kt,float bt) {
    gx::DrawUniform u;
    u.tex_gens[id]=0x5006;   // regular, from the position, input ABC1, three rows
    for (int i=0;i<3;++i) u.tex_rows[id*3+i]=t.position_rows[i];
    const auto& p=t.projection;
    for (int c=0;c<4;++c) {
        u.post_rows[id*3+0][c]=ks*p[0][c]+bs*p[3][c];
        u.post_rows[id*3+1][c]=kt*p[1][c]+bt*p[3][c];
        u.post_rows[id*3+2][c]=p[3][c];
    }
    return u;
}
gx::DrawPacket draw_with(const gx::Transforms& t,const gx::DrawUniform& u,uint32_t id,bool copy) {
    gx::DrawPacket draw;
    draw.transforms=t; draw.uniforms=u;
    draw.material.stages=1; draw.material.stage[0].order=64|(id<<3);   // stage 0 samples map 0 with coordinate id
    auto texture=std::make_shared<gx::TexturePixels>(); texture->gpu_only=copy;
    draw.texture=texture;
    return draw;
}
bool near(float a,float b) { return std::abs(a-b)<1e-4f; }
}
int main() {
    const auto t=camera();
    {   // the usual mapping of the picture to a texture: 0.5 * x / w + 0.5 and -0.5 * y / w + 0.5
        const auto found=gx::screen_lookup(draw_with(t,screen_texgen(t,0,.5f,.5f,-.5f,.5f),0,true));
        CHECK(found && found->id==0);
        CHECK(near(found->s[0],.5f) && near(found->s[1],.5f) && near(found->t[0],-.5f) && near(found->t[1],.5f));
    }
    {   // another coordinate and another scale
        const auto found=gx::screen_lookup(draw_with(t,screen_texgen(t,2,.25f,.4f,.3f,.1f),2,true));
        CHECK(found && found->id==2 && near(found->s[0],.25f) && near(found->s[1],.4f) && near(found->t[0],.3f) && near(found->t[1],.1f));
    }
    CHECK(!gx::screen_lookup(draw_with(t,screen_texgen(t,0,.5f,.5f,-.5f,.5f),0,false)));   // not a copy of the frame buffer
    {   // a projector that is not the camera (a shadow from a light): its rows are no multiples of the clip rows
        auto u=screen_texgen(t,0,.5f,.5f,-.5f,.5f);
        u.tex_rows[0]={0,0,1,5}; u.tex_rows[1]={0,1,0,2}; u.tex_rows[2]={1,0,0,30};
        CHECK(!gx::screen_lookup(draw_with(t,u,0,true)));
    }
    {   // a coordinate from a texture coordinate of the vertex, and one without the third row
        auto u=screen_texgen(t,0,.5f,.5f,-.5f,.5f); u.tex_gens[0]=0x5280;
        CHECK(!gx::screen_lookup(draw_with(t,u,0,true)));
        u.tex_gens[0]=0x5004;
        CHECK(!gx::screen_lookup(draw_with(t,u,0,true)));
    }
    {   // a 2D draw is not a draw of the world
        auto flat=t; flat.projection={{{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}}};
        CHECK(!gx::screen_lookup(draw_with(flat,screen_texgen(t,0,.5f,.5f,-.5f,.5f),0,true)));
    }
    return 0;
}
