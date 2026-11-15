#pragma once
namespace uc {
// Dev profiler: with UC_CPU_SAMPLE=1 (UC_CPU_SAMPLE_DELAY=<s> skips the first seconds), samples the calling thread every ~0.5 ms (10-frame stack) and rewrites cpu-samples.txt once per second.
// Resolve with scripts/resolve_cpu_samples.py against a /Z7 build (build-prof). No-op otherwise; the first call picks the sampled thread.
void cpu_sampler_start_once(const char* thread = "guest");   // UC_CPU_SAMPLE=guest|main (1 = guest) picks which caller is sampled
// UC_CPU_SAMPLE=all samples every thread of the process that runs code of the image (guest threads, render thread, workers) into cpu-samples-all.txt:
// a line is "T<thread id> rva rva ... = count". A game without the hle_GameFrameWait native (The Darkside Chronicles) has no "guest" caller.
void cpu_sampler_set_replaying(bool value); // UC_CPU_SAMPLE_REPLAY limits collection to rollback work.
}
