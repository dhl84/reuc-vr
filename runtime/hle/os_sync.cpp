#include "hle/os.h"
#include <deque>
#include <map>

using namespace uc;

void hle_OSSendMessage(ppc::Ctx& c);
void hle_OSJamMessage(ppc::Ctx& c);

namespace {
struct MutexState { Thread* owner = nullptr; int count = 0; };
struct QueueState { std::deque<uint32_t> msgs; uint32_t cap = 0; };
constexpr uint32_t recv_key(uint32_t q) { return q; }
constexpr uint32_t send_key(uint32_t q) { return q + 1; }   // unaligned, so it never equals an object address
}  // namespace

namespace uc {
struct OsSyncSnapshot {
    std::map<uint32_t, MutexState> mutexes;
    std::map<uint32_t, QueueState> queues;
};
static OsSyncSnapshot state;
std::shared_ptr<const OsSyncSnapshot> os_sync_checkpoint() { return std::make_shared<OsSyncSnapshot>(state); }
void os_sync_restore(const OsSyncSnapshot& saved) { state=saved; }
}
static auto& g_mutex=uc::state.mutexes;
static auto& g_queue=uc::state.queues;

// Mutex and queue state lives on the host side keyed by the guest object address; the guest fields are never read.
// tech-debt TD-009: no priority inheritance.
void hle_OSInitMutex(ppc::Ctx& c) { g_mutex[c.r[3]] = {}; }
void hle_OSLockMutex(ppc::Ctx& c) {
    PendingHle pending(os_sched(),c,hle_OSLockMutex);
    uint32_t key = c.r[3];
    Thread* me = os_sched().current();
    for (;;) {
        MutexState& m = g_mutex[key];
        if (!m.owner || m.owner == me) { m.owner = me; ++m.count; return; }
        os_sched().block_on(key);
    }
}
void hle_OSUnlockMutex(ppc::Ctx& c) {
    PendingHle pending(os_sched(),c);
    uint32_t key = c.r[3];
    MutexState& m = g_mutex[key];
    if (m.owner != os_sched().current()) return;
    if (--m.count == 0) { m.owner = nullptr; os_sched().wake_all(key); }   // all waiters retry: a suspended one must not hold up the rest
}
// OSTryLockMutex(mutex) -> 1 when the caller has the mutex now, 0 when another thread has it. SBDE08 only (project/dc/hle_manual.txt):
// its NW4R reads a file under a mutex that it takes with this call and gives back with OSUnlockMutex (8050901C). The SDK's own code
// works on the fields of the guest's mutex, which the natives above never write: the first caller kept the mutex for ever there.
void hle_OSTryLockMutex(ppc::Ctx& c) {
    MutexState& m = g_mutex[c.r[3]];
    Thread* me = os_sched().current();
    if (m.owner && m.owner != me) { c.r[3] = 0; return; }
    m.owner = me; ++m.count;
    c.r[3] = 1;
}

void hle_OSInitMessageQueue(ppc::Ctx& c) { g_queue[c.r[3]] = {{}, c.r[5]}; }   // (queue, array, count)
static void send(ppc::Ctx& c, bool front) {                                     // (queue, msg, flags); OS_MESSAGE_BLOCK = 1
    PendingHle pending(os_sched(),c,front ? hle_OSJamMessage : hle_OSSendMessage);
    uint32_t q = c.r[3], msg = c.r[4], flags = c.r[5];
    for (;;) {
        QueueState& s = g_queue[q];
        if (s.msgs.size() < s.cap) {
            if (front) s.msgs.push_front(msg); else s.msgs.push_back(msg);
            c.r[3] = 1;
            pending.committed();
            os_sched().wake_all(recv_key(q));
            return;
        }
        if (!(flags & 1)) { c.r[3] = 0; return; }
        os_sched().block_on(send_key(q));
    }
}
void hle_OSSendMessage(ppc::Ctx& c) { send(c, false); }
void hle_OSJamMessage(ppc::Ctx& c) { send(c, true); }
void hle_OSReceiveMessage(ppc::Ctx& c) {                                        // (queue, void** msg, flags)
    PendingHle pending(os_sched(),c,hle_OSReceiveMessage);
    uint32_t q = c.r[3], out = c.r[4], flags = c.r[5];
    for (;;) {
        QueueState& s = g_queue[q];
        if (!s.msgs.empty()) {
            uint32_t m = s.msgs.front();
            s.msgs.pop_front();
            if (out) ppc::wr32(c, out, m);
            c.r[3] = 1;
            pending.committed();
            os_sched().wake_all(send_key(q));
            return;
        }
        if (!(flags & 1)) { c.r[3] = 0; return; }
        os_sched().block_on(recv_key(q));
    }
}
