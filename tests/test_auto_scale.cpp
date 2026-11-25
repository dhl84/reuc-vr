#include "check.h"
#include "auto_scale.h"
namespace {
// A frame on the headset: GPU time for the pixels of the level, and the display's 72 Hz only while that time fits a refresh.
// fixed_ms: GPU time that does not follow the pixels (many draws, many passes).
struct Scene { double base_ms, cpu_fps, fixed_ms=0; };
double gpu_ms(const Scene& scene,const uc::AutoScale& scale) { return scene.fixed_ms+scene.base_ms*(0.25+0.75*scale.factor()*scale.factor()); }
double fps(const Scene& scene,const uc::AutoScale& scale) { return std::min(scene.cpu_fps,gpu_ms(scene,scale)>12.0 ? 48.0 : 72.0); }
// Runs the scene for a number of seconds and returns the number of level changes.
int run(uc::AutoScale& scale,const Scene& scene,int seconds) {
    int changes=0;
    for(int i=0;i<seconds;++i) changes+=scale.window(fps(scene,scale),gpu_ms(scene,scale),72);
    return changes;
}
}
int main() {
    {   // the slow state: the frame fits with the pixels of the next level, and stays there (no room for the chosen size)
        uc::AutoScale scale; const Scene heavy{12.5,72};
        CHECK(run(scale,heavy,1)==0 && scale.level()==0);
        CHECK(run(scale,heavy,1)==1 && scale.level()==1);
        CHECK(run(scale,heavy,600)==0 && scale.level()==1);
        const Scene light{6,72};   // a light scene: the chosen size comes back after the wait
        CHECK(run(scale,light,uc::AutoScale::kUpWait-1)==0 && scale.level()==1);
        CHECK(run(scale,light,1)==1 && scale.level()==0);
        CHECK(run(scale,light,600)==0 && scale.level()==0);
    }
    {   // a frame that needs two levels
        uc::AutoScale scale; const Scene heavy{15,72};
        CHECK(run(scale,heavy,10)==2 && scale.level()==2);
        CHECK(run(scale,heavy,600)==0);
    }
    {   // a frame that needs three levels, and the chosen size comes back in three steps
        uc::AutoScale scale; const Scene heavy{19,72}, light{6,72};
        CHECK(run(scale,heavy,20)==3 && scale.level()==3);
        CHECK(run(scale,heavy,600)==0);
        CHECK(run(scale,light,200)==3 && scale.level()==0);
    }
    {   // a frame that needs four levels
        uc::AutoScale scale; const Scene heavy{24,72};
        CHECK(run(scale,heavy,30)==4 && scale.level()==4);
        CHECK(run(scale,heavy,600)==0);
    }
    {   // the heaviest frame stops at the last level
        uc::AutoScale scale; const Scene heavy{40,72};
        CHECK(run(scale,heavy,60)==4 && scale.level()==4 && scale.level()+1==uc::AutoScale::kFactors.size());
    }
    {   // the game's own code limits the frame: fewer pixels bring no frames, so they come back, and each new try waits longer
        uc::AutoScale scale; const Scene slow{11.5,55};
        CHECK(run(scale,slow,2)==1 && scale.level()==1);
        CHECK(run(scale,slow,uc::AutoScale::kSettle+1)==1 && scale.level()==0);
        CHECK(run(scale,slow,uc::AutoScale::kDownWait-1)==0);
        CHECK(run(scale,slow,10)==2 && scale.level()==0);          // one more try and its end
        CHECK(run(scale,slow,2*uc::AutoScale::kDownWait-10)==0);   // the second wait is twice as long
    }
    {   // GPU time that does not follow the pixels: the same end
        uc::AutoScale scale; const Scene draws{2,72,11};
        CHECK(run(scale,draws,2)==1 && scale.level()==1);
        CHECK(run(scale,draws,uc::AutoScale::kSettle+1)==1 && scale.level()==0);
        CHECK(run(scale,draws,uc::AutoScale::kDownWait-1)==0);
    }
    {   // a step up that does not hold doubles the wait for the next one
        uc::AutoScale scale; const Scene heavy{12.5,72}, light{6,72};
        run(scale,heavy,2); run(scale,light,uc::AutoScale::kSettle+uc::AutoScale::kUpWait); CHECK(scale.level()==0);
        CHECK(run(scale,heavy,10)==1 && scale.level()==1);        // within kFailedUp windows of the step up
        CHECK(run(scale,light,2*uc::AutoScale::kUpWait-1)==0 && scale.level()==1);
        CHECK(run(scale,light,1)==1 && scale.level()==0);
    }
    {   // no frames (a pause, the headset off the head) and no target: nothing changes
        uc::AutoScale scale;
        for(int i=0;i<100;++i) CHECK(!scale.window(0,0,72) && !scale.window(30,13,0));
    }
}
