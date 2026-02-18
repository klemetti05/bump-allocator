//
// Created by Klemens Aimetti on 23.01.26.
//

#pragma once
#include <atomic>
#include <cstdio>
#include <unordered_set>
#include <unordered_map>
#include <print>

#include "bump/default_formatter.h"
#include <thread>
#ifdef BUMP_TRACK_HEAP
namespace bump
{
struct AllocInfo;
struct AllocInfo
{
    size_t total_malloc = 0;
    size_t total_free = 0;
    bool operator ==(const AllocInfo&)const = default;
};
struct AllocMetha
{
    AllocInfo info;
    const char* name;
    AllocInfo* address;
    bool operator ==(const AllocMetha&)const = default;
};
}

default_formatter(bump::AllocInfo, "total_malloc: {}, total_free: {}", self.total_malloc, self.total_free);

namespace bump{

class HeapTracker
{
    friend struct TrackedAllocInfo;
    std::unordered_map<AllocInfo*, const char*> allocators;
    std::mutex mutex;
    std::thread logger;

    std::vector<AllocMetha> state;
    std::atomic_bool stop = false;
public:

    static HeapTracker& GetInstance()
    {
        static HeapTracker instance;
        return instance;
    }

    void StopLoggingThread()
    {
        if (logger.joinable())
        {
            this->stop.store(true, std::memory_order_relaxed);
            logger.join();
        }
    }

    void StartLoggingThread(std::chrono::nanoseconds interval)
    {
        StopLoggingThread();
        this->stop.store(true, std::memory_order_relaxed);
        logger = std::thread([this, interval]()
        {
            while (!this->stop.load(std::memory_order_acquire))
            {
                Report();
                std::this_thread::sleep_for(interval);
            }
        });
    }
    void Report()
    {
        std::vector<AllocMetha> new_state;
        gatherState(new_state);
        if (new_state != state)
        {
            report(new_state);
            state = std::move(new_state);
        }
    }


private:
    void gatherState(std::vector<AllocMetha>& out_snapshot)
    {
        std::lock_guard lock(mutex);
        out_snapshot.reserve(out_snapshot.size() + allocators.size());
        for (auto& info: allocators)
        {
            out_snapshot.emplace_back(*info.first, info.second, info.first);
        }
    }


    void report(const std::span<AllocMetha>& state)
    {
        std::printf("---------------------REPORT---------------------\n");
        AllocInfo total;
        for (auto& allocator: state){
            if (allocator.name)
            {
                std::println("Allocator at {} '{}': {{{}}}",reinterpret_cast<void*>(allocator.address), allocator.name?allocator.name: "", allocator.info);
            }
            total.total_malloc += allocator.info.total_malloc;
            total.total_free += allocator.info.total_free;
        }

        std::println("InTotal: {{{}}}", total);
    }
};

struct TrackedAllocInfo: AllocInfo
{

    TrackedAllocInfo()
    {
        auto& instance = HeapTracker::GetInstance();
        std::lock_guard guard(instance.mutex);
        instance.allocators.emplace(this, nullptr);
    }
    void SetName(const char* name)
    {
        auto& instance = HeapTracker::GetInstance();
        std::lock_guard guard(instance.mutex);
        instance.allocators[this] = name;
    }
    ~TrackedAllocInfo()
    {
        auto& instance = HeapTracker::GetInstance();
        std::lock_guard guard(instance.mutex);
        instance.allocators.erase(this);
    }

    TrackedAllocInfo(const TrackedAllocInfo& other) = delete;
    TrackedAllocInfo(TrackedAllocInfo&& other) noexcept = delete;
    TrackedAllocInfo& operator=(const TrackedAllocInfo& other) = delete;
    TrackedAllocInfo& operator=(TrackedAllocInfo&& other) noexcept = delete;
private:
};

}
#else

namespace Bump
{
    struct HeapTracker
    {
        void StopLoggingThread(){}
        void StartLoggingThread(std::chrono::nanoseconds interval){}
        void Report(){}
    };
}
#endif
