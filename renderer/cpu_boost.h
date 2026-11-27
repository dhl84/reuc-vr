#pragma once
#include <algorithm>
#include <cstdint>
namespace uc {
// The boost level of the headset's CPU, on demand. On battery a Quest 3 holds its fast cores at 1.92 GHz under the sustained-high level.
// The boost level gives up to 2.36 GHz, but the runtime allows it only for 45 s in a row and for a fifth of the run time. So the port
// asks for it only in the seconds that need it, and it keeps its own count of the share.
// One call of window() for each second of presented frames. It returns true when the request changed (on() is the new request).
//
// With a known load (the CPU time of the busiest of the game thread and the render thread, as a part of the second): a boost starts
// after a late second with a busy thread, or before a late second when the load is heavy and the share has room for a long boost. It
// ends after some seconds below the release load. Five chapters on battery (app 0.0.57, the rule without the load) had 2 slow seconds
// of a busy game thread with the boost and 103 without it, and 60 % of the boost seconds had no busy thread. Most of the 103 came
// after seconds with a load of 0.80 or more.
// Without a known load (a negative value): a boost starts after a late second whose GPU time does not fill the period, and it holds
// 16 s or more, because the runtime raises the clock only after some seconds of load (5 s in a test, not at all in 8 s of another).
struct CpuBoost {
    static constexpr uint32_t kLongestRun=40, kShortHold=16, kLoadHold=6, kLongestHold=32, kCalm=4, kRest=10, kRelapse=6, kForget=120;
    static constexpr double kLate=0.93, kGpuBound=0.95, kShare=0.18;
    // The loads are for the clock of the sustained-high level. Under the boost the same work needs about 0.81 of the time, so the
    // release load 0.58 is the load 0.72 without the boost.
    static constexpr double kBusy=0.60, kHeavy=0.80, kRelease=0.58;
    bool on() const { return on_; }
    uint32_t boosted() const { return boosted_; }
    uint32_t total() const { return total_; }
    bool window(double fps,double gpu_ms,double target_hz,double load=-1) {
        if(!(target_hz>0) || !(fps>0)) return false;
        ++total_;
        const bool late=fps<kLate*target_hz, known=load>=0;
        const uint32_t shortest=known ? kLoadHold : kShortHold;
        if(on_) {
            ++boosted_; ++run_;
            const bool needed=known ? load>=kRelease : late;
            calm_=needed ? 0 : calm_+1;
            const bool spent=double(boosted_)>=kShare*double(total_);
            if(run_>=kLongestRun || spent || (run_>=std::max(hold_,shortest) && calm_>=kCalm)) {
                on_=false; rest_=run_>=kLongestRun || spent ? kRest : 0; since_off_=0;
                return true;
            }
            return false;
        }
        if(since_off_<kForget) ++since_off_;
        if(since_off_==kForget) hold_=0;   // a long time without a boost: the next one is short again
        if(rest_) { --rest_; return false; }
        if(double(boosted_+shortest)>kShare*double(total_)) return false;   // the share is spent
        const bool gpu_bound=gpu_ms>kGpuBound*1000.0/target_hz;
        if(known) {
            // A late second whose GPU time fills the period needs a heavy load: a faster CPU does not repair it for a busy thread.
            const bool room=double(boosted_+kLongestRun)<=kShare*double(total_);
            if(!(late && load>=(gpu_bound ? kHeavy : kBusy)) && !(load>=kHeavy && room)) return false;
        } else if(!late || gpu_bound) return false;
        // Without a known load, a start soon after the end of a boost holds longer: the scene still needs it. With a known load
        // the release load does that work, and the longer hold spent a third of the boost seconds on calm seconds (app 0.0.59).
        if(!known && since_off_<=kRelapse) hold_=std::min(std::max(hold_,shortest)*2,kLongestHold);
        on_=true; run_=calm_=0;
        return true;
    }
private:
    uint32_t total_=0, boosted_=0, run_=0, calm_=0, rest_=0, hold_=0, since_off_=kForget;
    bool on_=false;
};
}
