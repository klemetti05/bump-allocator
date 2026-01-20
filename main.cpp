

#include "bump/default_formatter.h"
#include "bump/formatter.h"
#include <iostream>

#include <memory_resource>
#include <random>
#include <thread>
#include <unordered_map>
#include <string>

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
void benchmark(const std::string& name, Map& map, Func&& func, size_t runs)
{
  auto start = std::chrono::high_resolution_clock::now();
  for (size_t i = 0; i < runs; ++i)
  {
    func(map);
  }
  auto end = std::chrono::high_resolution_clock::now();

  auto duration_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

  std::cout << name << " took " << duration_ms << " ms\n";
}

using pmr_map = std::pmr::unordered_map<size_t, size_t>;

std::vector<void*> warmup_heap(size_t max_chunk, size_t bytes_per_type, float percent_free)
{
  std::mt19937 gen(std::random_device{}());
  std::uniform_real_distribution<float> dist(0.0, 100.0);
  std::vector<void*> allocations;
  for (size_t size = 8; size < max_chunk; size *= 2)
  {
    for (size_t i = 0; i < bytes_per_type/size; ++i)
    {
      allocations.emplace_back(malloc(size));
    }
  }
  for (auto allocation : allocations)
  {
    if (dist(gen) >= percent_free)
    {
      free(allocation);
    }
  }
  return allocations;
}

int main()
{

  return 0;
  {
    using namespace bump;
    allocator resource;
    frame_ptr guard(resource);

    Formatter formater(guard);

    for (size_t i = 0; i < 100; ++i)
    {
      formater.append("mein name ist hugo{}\n", i);
    }

    pmr::string view;
    formater.collect().into(view);
    std::cout << view  << std::endl;

    constexpr size_t inserts = 10'000;

    std::vector<void*> allocations = warmup_heap(64*1024, 256*1024, 50.0f);

    for (size_t i = 0; i < 100; ++i)
    {
      std::unordered_map<std::string, size_t> stdmap;
      benchmark("std unordered_map", stdmap, [&](std::unordered_map<std::string, size_t>& ages)
      {
        for (size_t i = 0; i < inserts; ++i)
        {
           ages[std::to_string(i)] = i;
        }
      }, 1
      );

      bump::allocator resource;
      bump::frame_ptr frame(resource);
      pmr::unordered_map<std::string_view, std::string_view> map(&frame);

    }
  }
}
