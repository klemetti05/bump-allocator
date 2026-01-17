#pragma once
#include <iostream>
#include <cstdlib>
#include <new>
#include <mutex>
#include <unordered_map>


class AllocationTracker {
private:
    size_t mallocCount = 0;
    size_t mallocBytes = 0;
    size_t newCount = 0;
    size_t newBytes = 0;

    std::unordered_map<void*, size_t> mallocMap;

    AllocationTracker() = default;

public:
    static AllocationTracker& instance() {
        static AllocationTracker tracker;
        return tracker;
    }

    void* trackMalloc(void* ptr, size_t size) {
        if (!ptr) return nullptr;
        mallocMap[ptr] = size;
        mallocCount++;
        mallocBytes += size;
        return ptr;
    }

    void trackFree(void* ptr) {
        if (!ptr) return;

        auto it = mallocMap.find(ptr);
        if(it != mallocMap.end()) {
            mallocCount--;
            mallocBytes -= it->second;
            mallocMap.erase(it);
        }
    }

    // Track new
    void trackNew(size_t size) {
        newCount++;
        newBytes += size;
    }

    void trackDelete(size_t size) {
        newCount--;
        newBytes -= size;
    }

    void report() {
        std::cout << "Malloc allocations: " << mallocCount
                  << ", bytes: " << mallocBytes << '\n';
        std::cout << "New allocations: " << newCount
                  << ", bytes: " << newBytes << '\n';
    }
};

inline void* operator new(size_t size) {
    void* ptr = std::malloc(size);
    AllocationTracker::instance().trackNew(size);
    if(!ptr) throw std::bad_alloc();
    return ptr;
}

inline void operator delete(void* ptr, size_t size) noexcept {
    AllocationTracker::instance().trackDelete(size);
    std::free(ptr);
}

inline void* trackedMalloc(size_t size) {
    void* ptr = std::malloc(size);
    return AllocationTracker::instance().trackMalloc(ptr, size);
}

inline void trackedFree(void* ptr) {
    AllocationTracker::instance().trackFree(ptr);
    std::free(ptr);
}

#define malloc trackedMalloc
