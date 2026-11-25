#pragma once
#include "gx_packets.h"
#include "host_thread.h"
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>

namespace uc {
// Frees rendered packets on a thread of its own. A packet of 500 draws holds some thousand shared blocks and vectors, and to free
// them took a twentieth of the render thread's time (Quest 3, 1,500 draws a frame). The pools of a packet take a free from any
// thread (renderer/frame_arena.h, renderer/block_pool.h), and a GPU slot holds what its submission still reads. When the thread
// is behind by kMaxHeld packets, the caller frees the packet itself, so the held memory has a limit.
class PacketTrash {
public:
    static constexpr size_t kMaxHeld = 96;   // about four frames of packets
    PacketTrash() : thread_([this] { run(); }) {}
    PacketTrash(const PacketTrash&) = delete;
    PacketTrash& operator=(const PacketTrash&) = delete;
    ~PacketTrash() {
        { std::lock_guard lock(mutex_); stop_ = true; }
        wake_.notify_one();
        thread_.join();
    }
    // Takes the caller's reference. The packet is freed here when it was the last one and the thread is behind.
    void put(std::shared_ptr<const gx::FramePacket> packet) {
        bool wake = false;
        {
            std::lock_guard lock(mutex_);
            if (held_.size() < kMaxHeld) { held_.push_back(std::move(packet)); wake = waiting_; }
        }
        if (wake) wake_.notify_one();   // a notify is a system call on Android even with nobody waiting
    }
private:
    void run() {
        host_thread_set_background();
        std::unique_lock lock(mutex_);
        for (;;) {
            waiting_ = true;
            wake_.wait(lock, [this] { return stop_ || !held_.empty(); });
            waiting_ = false;
            if (held_.empty()) return;   // stop, and nothing is left
            auto packet = std::move(held_.front());
            held_.pop_front();
            lock.unlock();
            packet.reset();
            lock.lock();
        }
    }
    std::mutex mutex_;
    std::condition_variable wake_;
    std::deque<std::shared_ptr<const gx::FramePacket>> held_;
    bool waiting_ = false, stop_ = false;
    std::thread thread_;   // last: it starts in the constructor and uses the members above
};
}  // namespace uc
