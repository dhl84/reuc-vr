#pragma once
// A pipeline whose compile crashes the graphics driver must not be compiled again.
//
// The renderer records every new pipeline key (pipeline_keys.bin) and compiles the whole list at the next start. On a Quest 3 one key of
// RBUE08's train chapter crashed the shader compiler of the Adreno driver (libllvm-qgl.so, 15 November 2026). The key was on the list, so
// every later start of the game crashed in its first second, until the list was emptied by hand.
//
// The guard keeps a journal of the compiles that are open (pipeline_compiling.bin, one entry for each compile thread). The crash handler
// of the process calls crashed() on the thread that faulted: an open compile of that thread is marked in the journal. The next start
// moves the marked entries to the list of bad pipelines (pipeline_bad.bin). A bad pipeline is never compiled again: the draw keeps the
// ubershader, or the specialized pipeline when it was the ubershader that crashed.
//
// Only the compile of the faulting thread is marked, so a process that Android kills while its threads compile marks nothing.
#include "pipeline_keys.h"
#include <atomic>
#include <cstdio>
#include <filesystem>
#include <mutex>
#include <set>
#include <utility>
#include <vector>

namespace uc {
class CompileGuard {
public:
    struct Entry { uint32_t state=0, uber=0; PipelineKey key{}; };   // state: 0 free, 1 a compile is open, 2 its thread crashed in it
    static constexpr unsigned kSlots=32;
    CompileGuard()=default;
    CompileGuard(const CompileGuard&)=delete;
    ~CompileGuard() { close(); }
    // Reads the journal of the last run in `dir`, adds its crashed compiles to the bad list, and starts an empty journal.
    // Returns the number of bad pipelines.
    size_t open(const std::filesystem::path& dir) {
        std::lock_guard lock(mutex_);
        const auto journal=dir/"pipeline_compiling.bin", list=dir/"pipeline_bad.bin";
        if (file_) { std::fclose(file_); file_=nullptr; }
        std::vector<Entry> crashed;
        if (std::FILE* f=std::fopen(journal.string().c_str(),"rb")) {
            for (Entry e; std::fread(&e,sizeof e,1,f)==1;) if (e.state==2) crashed.push_back(e);
            std::fclose(f);
        }
        if (!crashed.empty()) if (std::FILE* f=std::fopen(list.string().c_str(),"ab")) {
            for (const auto& e:crashed) std::fwrite(&e,sizeof e,1,f);
            std::fclose(f);
        }
        bad_.clear();
        if (std::FILE* f=std::fopen(list.string().c_str(),"rb")) {
            for (Entry e; std::fread(&e,sizeof e,1,f)==1;) bad_.insert({e.key,e.uber!=0});   // a torn last entry is dropped
            std::fclose(f);
        }
        new_=crashed.size(); busy_=0;
        file_=std::fopen(journal.string().c_str(),"w+b");
        if (file_) {
            const Entry empty{};
            for (unsigned i=0;i<kSlots;++i) std::fwrite(&empty,sizeof empty,1,file_);
            std::fflush(file_);
        }
        return bad_.size();
    }
    void close() { std::lock_guard lock(mutex_); if (file_) { std::fclose(file_); file_=nullptr; } busy_=0; }
    size_t new_bad() const { return new_; }   // the crashed compiles that open() found in the journal of the last run
    bool bad(const PipelineKey& key,bool uber) const { std::lock_guard lock(mutex_); return !bad_.empty() && bad_.count({key,uber})!=0; }
    // Before a compile, on the thread that compiles. Returns the journal entry for end(), or -1 without a journal or a free entry
    // (the compile is then not guarded).
    int begin(const PipelineKey& key,bool uber) {
        std::lock_guard lock(mutex_);
        if (!file_ || slot_>=0) return -1;
        for (unsigned i=0;i<kSlots;++i) if (!(busy_&(1u<<i))) {
            const Entry entry{1,uber ? 1u : 0u,key};
            if (!write(i,&entry,sizeof entry)) return -1;
            busy_|=1u<<i; slot_=int(i);
            return int(i);
        }
        return -1;
    }
    void end(int slot) {
        if (slot<0) return;
        std::lock_guard lock(mutex_);
        const uint32_t free_state=0;
        if (file_) write(unsigned(slot),&free_state,sizeof free_state);
        busy_&=~(1u<<slot); slot_=-1;
    }
    // From the crash handler, on the thread that faulted: marks the compile that this thread has open.
    void crashed() {
        if (slot_<0) return;
        std::lock_guard lock(mutex_);   // a compile runs outside the lock, so the faulting thread does not hold it
        const uint32_t crashed_state=2;
        if (file_) write(unsigned(slot_),&crashed_state,sizeof crashed_state);
    }
private:
    bool write(unsigned slot,const void* data,size_t size) {
        return std::fseek(file_,long(slot*sizeof(Entry)),SEEK_SET)==0 && std::fwrite(data,size,1,file_)==1 && std::fflush(file_)==0;
    }
    mutable std::mutex mutex_;
    std::FILE* file_=nullptr;
    std::set<std::pair<PipelineKey,bool>> bad_;
    uint32_t busy_=0;
    size_t new_=0;
    static inline thread_local int slot_=-1;   // the journal entry of the compile that this thread has open
};
// The guard of the renderer in use, for the crash handler (game/uc_main.cpp passes compile_guard_crashed to uc::set_crash_hook).
inline std::atomic<CompileGuard*> g_compile_guard{nullptr};
inline void compile_guard_crashed() { if (auto* guard=g_compile_guard.load()) guard->crashed(); }
}
