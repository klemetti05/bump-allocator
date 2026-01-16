#include "include/bump.h"
#include <iostream>

struct Trackable
{
  static inline size_t ID = size_t{0};
  size_t id = ID++;
  Trackable() { std::cout << id << ": Created" << std::endl; }
  ~Trackable() { std::cout << id << ": Destroyed" << std::endl; }
};

template <typename T> struct BumpDeleter
{
  void operator()(T *p) { p->~T(); }
};

template <typename T> using bump_pointer = std::unique_ptr<T, BumpDeleter<T>>;

int main()
{
  bump::pool pool;
  bump::frame_pointer frame(pool);
  bump::vector<bump_pointer<Trackable>> list(pool);

  for (size_t i = 0; i < 20; ++i)
  {
    list.emplace_back(new (frame->push<Trackable>()) Trackable());
  }
  std::vector<bump_pointer<Trackable>> v = list.moveToVector();
  return 0;
}