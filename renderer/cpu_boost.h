#pragma once
#include <algorithm>
#include <cstdint>
namespace uc {
// The boost level of the headset's CPU, on demand. On battery a Quest 3 holds its fast cores at 1.92 GHz under the sustained-high level.
// The boost level gives up to 2.36 GHz, but the runtime allows it only for 45 s in a row and for a fifth of the run time. So the port
// asks for it only in the seconds that miss the game's step, and it keeps its own count of the share. A frame whose GPU time alone
// fills the period gains nothing from a faster CPU, and such a second does not start the boost.
// One call of window() for each second of presented frames. It returns true when the request changed (on() is the new request).
struct CpuBoost {
    // The runtime raises the clock only after some seconds of load under the boost level (5 s in a test on a Quest 3, and not at
    // all in 8 s of another span). So the shortest boost holds 16 s.
    static constexpr uint32_t kLongestRun=40, kShortHold=16, kLongestHold=32, kCalm=4, kRest=10, kRelapse=6, kForget=120;
    static constexpr double kLate=0.93, kGpuBound=0.95, kShare=0.18;
    bool on() const { return on_; }
    uint32_t boosted() const { return boosted_; }
    uint32_t total() const { return total_; }
    bool window(double fps,double gpu_ms,double target_hz) {
        if(!(target_hz>0) || !(fps>0)) return false;
        ++total_;
        const bool late=fps<kLate*target_hz;
        if(on_) {
            ++boosted_; ++run_;
            calm_=late ? 0 : calm_+1;
            const bool spent=double(boosted_)>=kShare*double(total_);
            if(run_>=kLongestRun || spent || (run_>=hold_ && calm_>=kCalm)) {
                on_=false; rest_=run_>=kLongestRun || spent ? kRest : 0; since_off_=0;
                return true;
            }
            return false;
        }
        if(since_off_<kForget) ++since_off_;
        if(since_off_==kForget) hold_=kShortHold;   // a long time without a slow second: the next boost is short again
        if(rest_) { --rest_; return false; }
        if(!late || gpu_ms>kGpuBound*1000.0/target_hz) return false;
        if(double(boosted_+kShortHold)>kShare*double(total_)) return false;   // the share is spent
        // A slow second soon after the end of a boost: the scene still needs it, so this boost holds longer.
        if(since_off_<=kRelapse) hold_=std::min(hold_*2,kLongestHold);
        on_=true; run_=calm_=0;
        return true;
    }
private:
    uint32_t total_=0, boosted_=0, run_=0, calm_=0, rest_=0, hold_=kShortHold, since_off_=kForget;
    bool on_=false;
};
}
