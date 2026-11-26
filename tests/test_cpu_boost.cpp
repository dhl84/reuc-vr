#include "check.h"
#include "cpu_boost.h"
namespace {
// Runs seconds of one frame rate and returns the number of changes of the request.
int run(uc::CpuBoost& boost,double fps,int seconds,double gpu_ms=8) {
    int changes=0;
    for(int i=0;i<seconds;++i) changes+=boost.window(fps,gpu_ms,72);
    return changes;
}
}
int main() {
    using B=uc::CpuBoost;
    {   // a slow second at the start of a run: the share is not there yet
        B boost;
        CHECK(run(boost,50,10)==0 && !boost.on());
    }
    {   // a slow second after a long good time starts the boost, and it ends after the short hold with calm seconds
        B boost;
        CHECK(run(boost,72,300)==0 && !boost.on());
        CHECK(run(boost,50,1)==1 && boost.on());
        CHECK(run(boost,72,B::kShortHold-1)==0 && boost.on());
        CHECK(run(boost,72,1)==1 && !boost.on());
        CHECK(boost.boosted()==B::kShortHold);
    }
    {   // a scene that is slow again right after the boost: the next boost holds twice as long
        B boost;
        run(boost,72,600); run(boost,50,1); run(boost,72,B::kShortHold); CHECK(!boost.on());
        CHECK(run(boost,50,1)==1 && boost.on());
        CHECK(run(boost,72,2*B::kShortHold-1)==0 && boost.on());
        CHECK(run(boost,72,1)==1 && !boost.on());
    }
    {   // a scene that stays slow with the boost: the boost ends after the longest run, rests, and starts again
        B boost;
        run(boost,72,2000);
        CHECK(run(boost,50,1)==1 && boost.on());
        CHECK(run(boost,50,B::kLongestRun-1)==0 && boost.on());
        CHECK(run(boost,50,1)==1 && !boost.on());
        CHECK(run(boost,50,B::kRest)==0 && !boost.on());
        CHECK(run(boost,50,1)==1 && boost.on());
    }
    {   // the share: a run that is always slow never has more than a fifth of its seconds with the boost
        B boost;
        run(boost,50,5000);
        CHECK(boost.boosted()>0 && double(boost.boosted())<=0.2*double(boost.total()));
        CHECK(double(boost.boosted())>=0.12*double(boost.total()));
    }
    {   // a second whose GPU time fills the period starts no boost
        B boost;
        run(boost,72,300);
        CHECK(run(boost,50,20,13.5)==0 && !boost.on());
        CHECK(run(boost,50,1,12.0)==1 && boost.on());
    }
    {   // no frames and no target: nothing changes
        B boost;
        for(int i=0;i<100;++i) CHECK(!boost.window(0,0,72) && !boost.window(30,13,0));
        CHECK(boost.total()==0);
    }
}
