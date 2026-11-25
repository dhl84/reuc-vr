#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
namespace uc {
// The headset's automatic render scale. The Quest 3 holds its GPU at 545 MHz under the sustained-high level and raises the clock to 640 MHz
// only while the GPU is busy most of the time. A frame that misses a display refresh leaves the GPU idle until the next one, so the clock
// drops and the frames after it miss too.
// So the picture gets fewer pixels while the GPU misses the game's step, and its chosen size back when the GPU has room for it. The room is
// judged at the clock of the moment, and the clock rises with the load: kRoom is therefore near the whole period.
// One call of window() for each second of presented frames. It returns true when the level changed (factor() is the new multiplier).
struct AutoScale {
    // Of the chosen scale: 2 -> 2, 1.75, 1.5, 1.25, 1.0. The fourth level is for scenes whose GPU time follows the pixels and does not fit
    // at 1.5 (SBDE08 s20c1 on a Quest 3: 13.3 ms a frame at the median at 1.5, and 64 % of the seconds at 60 fps or more). The fifth level
    // is for the heaviest fights: a standing frame of s30c4 took 10.3 ms at 1.25 and 8.2 ms at 1.0 (docs/darkside-port.md).
    static constexpr std::array<double,5> kFactors{1.0,0.875,0.75,0.625,0.5};
    static constexpr uint32_t kSettle=2, kSlowWindows=2, kUpWait=10, kDownWait=60, kLongestWait=600, kFailedUp=30, kHeldUp=120;
    static constexpr double kLate=0.93, kBusy=0.78, kOnTime=0.97, kRoom=0.88, kPixelShare=0.75, kGain=1.03, kFell=0.92;
    double factor() const { return kFactors[level_]; }
    uint32_t level() const { return level_; }
    bool window(double fps,double gpu_ms,double target_hz) {
        if(!(target_hz>0) || !(fps>0)) return false;
        const double period=1000.0/target_hz;
        if(since_up_<kLongestWait) ++since_up_;
        if(since_up_==kHeldUp) up_wait_=kUpWait;   // the last step up held: the next one waits the short time again
        if(down_block_) --down_block_;
        if(settle_) { --settle_; return false; }   // the windows after a change hold the rebuild
        const bool late=fps<kLate*target_hz, busy=gpu_ms>kBusy*period;
        if(judge_down_) {
            judge_down_=false;
            // No more frames, and the GPU now has room or its time did not fall: the pixels did not limit the frame (the game's own code
            // did, or the number of draws and passes). The pixels go back, and the next try waits longer each time. No more frames, but
            // less GPU time that is still too long: the frame needs one more level.
            if(fps<fps_before_down_*kGain && (!busy || gpu_ms>gpu_before_down_*kFell)) {
                --level_; settle_=kSettle; slow_=good_=0; down_block_=down_wait_; down_wait_=std::min(down_wait_*2,kLongestWait);
                return true;
            }
            down_wait_=kDownWait;
        }
        slow_=late && busy ? slow_+1 : 0;
        if(slow_>=kSlowWindows && level_+1<kFactors.size() && !down_block_) {
            if(since_up_<kFailedUp) up_wait_=std::min(up_wait_*2,kLongestWait);   // the step up did not hold
            fps_before_down_=fps; gpu_before_down_=gpu_ms; ++level_; settle_=kSettle; slow_=good_=0; judge_down_=true;
            return true;
        }
        if(level_) {
            const double pixels=kFactors[level_-1]*kFactors[level_-1]/(kFactors[level_]*kFactors[level_]);
            const double predicted=gpu_ms*(1-kPixelShare+kPixelShare*pixels);   // the GPU time of this frame with the pixels of the level above
            good_=fps>=kOnTime*target_hz && predicted<kRoom*period ? good_+1 : 0;
            if(good_>=up_wait_) { --level_; settle_=kSettle; slow_=good_=0; since_up_=0; return true; }
        }
        return false;
    }
private:
    uint32_t level_=0, settle_=0, slow_=0, good_=0, up_wait_=kUpWait, down_wait_=kDownWait, down_block_=0, since_up_=kLongestWait;
    double fps_before_down_=0, gpu_before_down_=0;
    bool judge_down_=false;
};
}
