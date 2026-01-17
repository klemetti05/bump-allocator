

#include "tracker.h"
#include "include/bump.h"
#include <iostream>
#include <memory>


#include <memory_resource>
#include <thread>
#include <unordered_map>



namespace pmr = std::pmr;

struct Trackable
{
  static inline size_t ID = size_t{0};
  size_t id = ID++;
  Trackable() { std::cout << id << ": Created" << std::endl; }
  ~Trackable() { std::cout << id << ": Destroyed" << std::endl; }
};

template <typename Map, typename Func>
[[gnu::noinline]]
void benchmark(const std::string& name, Map& map, Func&& func)
{
  auto start = std::chrono::high_resolution_clock::now();
  func(map);
  auto end = std::chrono::high_resolution_clock::now();

  auto duration_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

  std::cout << name << " took " << duration_ms << " ms\n";
}

using pmr_map = std::pmr::unordered_map<size_t, size_t>;

int main()
{
  constexpr size_t inserts = 10'000'00;

  for (size_t i = 0; i < 100; ++i){
    bump::allocator resource;
    bump::pmr_fp frame(resource);
    pmr_map map(&frame);

    benchmark("PMR unordered_map", map, [&](pmr_map& ages) {
        for (size_t i = 0; i < inserts; ++i) {
            ages[i] = i;
        }
    });
    std::cout << std::endl << map.bucket_count() << std::endl ;

    std::unordered_map<size_t, size_t> stdmap;
    benchmark("std unordered_map", stdmap, [&](std::unordered_map<size_t, size_t>& ages)
    {
      for (size_t i = 0; i < inserts; ++i) {
         ages[i] = i;
      }
    });
    std::cout << map.bucket_count() << std::endl;
    AllocationTracker::instance().report();
  }
}
