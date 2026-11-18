#include "gx_audit.h"
#include "trace.h"
#include "gx_vertices.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <mutex>
#include <set>

namespace {
std::mutex mutex;
std::map<uint32_t, uc::GxAuditEntry> entries;
bool recording = false;
bool watched(uint32_t address, uint32_t link) {
    // DOL-verified object lifecycle/update dispatch; observe native callback targets without replacing them.
    switch (link) {
    case 0x8003D208: case 0x8003D2AC: case 0x8003D348:
    case 0x8003D408: case 0x8003D430: case 0x8003D4A8:
    case 0x80131740: case 0x801317D4: case 0x801317E8: return true;
    case 0x80131850: case 0x8013188C: case 0x801318A4:
    case 0x801318C4: case 0x801318DC: case 0x80131908: return true;
    case 0x80115348: case 0x8011535C: case 0x80116204: return true;
    default: break;
    }
    if (address >= 0x8028183C && address <= 0x802894B0) return true;
    switch (address) {
    case 0x80039864: case 0x80039C50: case 0x8003A46C: case 0x8003A6AC: case 0x8003A77C:
    case 0x8003CDE0: case 0x80366D3C: case 0x80366E14: case 0x80366E78: case 0x80366FDC:
    case 0x80367AC4: case 0x80367FDC: return true;
    case 0x802F21EC: return true;
    case 0x801028D4: case 0x8011637C: case 0x80116674: return true; // verified startup resource/dialog boundaries
    case 0x802ACAC4: return true; // verified NANDGetHomeDir readiness gate
    case 0x8003D1AC: case 0x8003D250: case 0x8003D2F4: case 0x8003D380:
    case 0x8003A864: case 0x8003A900: return true;
    default: return false;
    }
}
// UC_ARG_TRACE=<value>[,<tolerance>] (diagnostics, a build with UC_TRACE): each function of the game that is entered with that value
// in f1 or f2 is printed once for each caller, with its first four float arguments. It finds the code that takes a known number, for
// example the aspect of the camera (1.77778).
struct ArgTrace { bool on=false; double values[8]{}; int count=0; double tolerance=1e-3; };   // UC_ARG_TRACE=v1/v2/...[,tolerance]
const ArgTrace arg_trace=[] {
    ArgTrace trace;
    if (const char* text=std::getenv("UC_ARG_TRACE")) {
        char* end=nullptr;
        for (const char* at=text;trace.count<8;) { const double v=std::strtod(at,&end); if (end==at) break; trace.values[trace.count++]=v; if (*end!='/') break; at=end+1; }
        trace.on=trace.count>0;
        if (trace.on && *end==',') trace.tolerance=std::strtod(end+1,nullptr);
        if (trace.on) uc::gx_audit_on.store(true, std::memory_order_relaxed);   // every function entry comes to gx_audit_enter_slow
    }
    return trace;
}();
std::atomic<bool> audit_enabled{false};
}
namespace uc {
void gx_audit_enable() { audit_enabled.store(true, std::memory_order_relaxed); gx_audit_on.store(true, std::memory_order_relaxed); }
void gx_audit_enter_slow(uint32_t address, const ppc::Ctx& ctx) {
    if (arg_trace.on) {
        // UC_ARG_TRACE_FILE=<file>: the trace starts when that file appears (looked for each 65536 entries).
        static const char* const start_file=std::getenv("UC_ARG_TRACE_FILE");
        static std::atomic<bool> started{!start_file};
        static std::atomic<uint32_t> entered{0};
        if (!started.load(std::memory_order_relaxed)) {
            if ((entered.fetch_add(1,std::memory_order_relaxed)&0xFFFF)==0) if (FILE* file=std::fopen(start_file,"rb")) { std::fclose(file); started.store(true,std::memory_order_relaxed); }
            if (!started.load(std::memory_order_relaxed)) { if (!audit_enabled.load(std::memory_order_relaxed)) return; goto audit; }
        }
        bool hit=false;
        for (int i=0;i<arg_trace.count && !hit;++i) hit=std::abs(ctx.f[1]-arg_trace.values[i])<arg_trace.tolerance || std::abs(ctx.f[2]-arg_trace.values[i])<arg_trace.tolerance;
        if (hit) {
            static std::mutex trace_mutex; static std::set<uint64_t> seen;   // once for each pair of function and caller
            std::lock_guard lock(trace_mutex);
            if (seen.size()<400 && seen.insert(uint64_t(address)<<32|ctx.lr).second)
                std::fprintf(stderr,"[arg-trace] %08X entered from %08X with f1 %.6g f2 %.6g f3 %.6g f4 %.6g\n",address,ctx.lr,double(ctx.f[1]),double(ctx.f[2]),double(ctx.f[3]),double(ctx.f[4]));
        }
        if (!audit_enabled.load(std::memory_order_relaxed)) return;
    }
audit:
    if (!gx_audit_on.load(std::memory_order_relaxed) || !watched(address,ctx.lr)) return;
    std::lock_guard lock(mutex);
    auto& entry = entries[address];
    if(ctx.mem && (address==0x80366D3C || address==0x80366E14) && entry.resources.size()<4) {
        const gx::Memory memory{{reinterpret_cast<const std::byte*>(ctx.mem),24u<<20},{reinterpret_cast<const std::byte*>(ctx.mem+0x10000000),64u<<20},{}};
        auto word=[&](uint32_t pointer) { uint32_t result=0; for(auto byte:memory.read(pointer,4)) result=result<<8|std::to_integer<uint8_t>(byte); return result; };
        const auto object=word(ctx.r[3]),meta=object+(address==0x80366D3C ? 24u : 36u);
        memory.read(object,48);
        const auto offset=word(meta+8),size=word(meta+4),skip=address==0x80366D3C && ctx.r[5] ? 32u : 0u;
        if(entry.calls<4) std::fprintf(stderr,"[mesh-meta] entry %08X object %08X descriptor %08X offset %08X size %u skip %u\n",address,object,meta,offset,size,skip);
        if(offset && size>skip) {
            const int64_t start=int64_t(meta)+int32_t(offset)+skip;
            if(start<0 || start>UINT32_MAX) throw std::invalid_argument("native mesh audit offset overflows");
            const auto pointer=uint32_t(start);
            if(std::none_of(entry.resources.begin(),entry.resources.end(),[pointer](const auto& resource){return resource.address==pointer;})) {
                const auto bytes=memory.read(pointer,size-skip),prefix=bytes.first(std::min<size_t>(bytes.size(),4096));
                entry.resources.push_back({pointer,bytes.size(),{prefix.begin(),prefix.end()}});
            }
        }
    }
    if (entry.calls == 0) {
        entry.address = address;
        entry.first_caller = ctx.lr >= 4 ? ctx.lr - 4 : 0;
        for (size_t i = 0; i < entry.first_gpr.size(); ++i) entry.first_gpr[i] = ctx.r[i + 3];
        for (size_t i = 0; i < entry.first_fpr.size(); ++i) entry.first_fpr[i] = ctx.f[i + 1];
        if (address==0x8011637C || address==0x80116674) {
            std::fprintf(stderr,"[startup resource] entry %08X caller %08X r3 %08X r4 %08X\n",address,entry.first_caller,ctx.r[3],ctx.r[4]);
            trace_dump(stderr,64);
        }
        if (address==0x802ACAC4 && ctx.mem) {
            const gx::Memory memory{{reinterpret_cast<const std::byte*>(ctx.mem),24u<<20},{reinterpret_cast<const std::byte*>(ctx.mem+0x10000000),64u<<20},{}};
            uint32_t initialized=0;
            for (auto byte : memory.read(ctx.r[13]-25056,4)) initialized=(initialized<<8)|std::to_integer<uint8_t>(byte);
            std::fprintf(stderr,"[save] NANDGetHomeDir caller %08X initialization %u (requires 2)\n",entry.first_caller,initialized);
        }
    }
    ++entry.calls;
    if (address == 0x80288684 && ctx.r[4]) { ++entry.nonempty_lists; entry.list_bytes += ctx.r[4]; }
    if (address == 0x80288528) recording = true;
    if (address == 0x802885D8) recording = false;
}
void gx_audit_fifo_write_slow(int size, uint32_t value) {
    if (!gx_audit_on.load(std::memory_order_relaxed) || !audit_enabled.load(std::memory_order_relaxed)) return;
    if (size != 1 && size != 2 && size != 4) throw std::invalid_argument("invalid GX audit write width");
    std::lock_guard lock(mutex);
    if (!recording) return;
    auto& entry = entries.at(0x80288528);
    entry.recording_bytes += unsigned(size);
    if (entry.calls == 1)
        for (int i = 0; i < size && entry.first_recording_size < entry.first_recording_prefix.size(); ++i)
            entry.first_recording_prefix[entry.first_recording_size++] = uint8_t(value >> ((size - 1 - i) * 8));
}
void gx_audit_recorded_resource(uint32_t address, std::span<const std::byte> bytes) {
    if (!uc::gx_audit_on.load(std::memory_order_relaxed) || bytes.empty()) return;
    std::lock_guard lock(mutex);
    auto found = entries.find(0x80288528);
    if (found == entries.end() || found->second.resources.size() == 4) return;
    auto& entry = found->second;
    if (std::any_of(entry.resources.begin(),entry.resources.end(),[address](const auto& resource){return resource.address==address;})) return;
    entry.resources.push_back({address,bytes.size(),{bytes.begin(),bytes.begin()+std::min<size_t>(bytes.size(),4096)}});
}
std::vector<GxAuditEntry> gx_audit_snapshot() {
    std::lock_guard lock(mutex);
    std::vector<GxAuditEntry> snapshot;
    snapshot.reserve(entries.size());
    for (const auto& [address, entry] : entries) snapshot.push_back(entry);
    return snapshot;
}
}
