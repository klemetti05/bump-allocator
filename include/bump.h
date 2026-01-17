//
// Created by Klemens Aimetti on 16.01.26.
//
#pragma once
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <memory>
#include <memory_resource>
#include <type_traits>
#ifndef NDEBUG

#define DEBUG_ONLY(X) X
#else
#define DEBUG_ONLY(X)
#endif


namespace bump
{

struct Node;

struct BumpAllocator
{
  Node *root = nullptr;
  Node *current = nullptr;

public:
  struct Frame
  {
    Node *current;
    std::byte *iterator;
  };

  Frame getFrame() noexcept;
  void restoreFrame(const Frame &frame) noexcept;

  size_t remaining(size_t align) noexcept;

  template <typename T> T *push() noexcept
  {
    return static_cast<T *>(allocate(sizeof(T), alignof(T)));
  }

  void *allocate(size_t bytes, size_t align = sizeof(size_t)) noexcept;

  void free() noexcept;

  ~BumpAllocator() noexcept;

  BumpAllocator() noexcept;

private:
  BumpAllocator(const BumpAllocator &other) = delete;

  BumpAllocator(BumpAllocator &&other) noexcept;

  BumpAllocator &operator=(const BumpAllocator &other) = delete;

  BumpAllocator &operator=(BumpAllocator &&other) noexcept;
  friend class AllocatorPool;
  friend class BumpGuard;
  friend class OwningBumpGuardBase;
};
struct Allocation
{
  uintptr_t checksum;
  std::byte payload[];
};
struct BumpGuard
{
  BumpAllocator &allocator;
  BumpAllocator::Frame frame;

  BumpGuard(BumpAllocator &allocator) noexcept : allocator(allocator), frame(allocator.getFrame())
  {
  }
  //BumpGuard(AllocatorPool &pool) noexcept : BumpGuard(pool.getStable()) {}

  ~BumpGuard() noexcept { allocator.restoreFrame(frame); }
  BumpAllocator *operator->() noexcept { return &allocator; }
};


class PmrGuard: public std::pmr::memory_resource
{
  static constexpr size_t MAGIC_NUMBER = 0x68dec895;
public:
  BumpAllocator& allocator;
  BumpAllocator::Frame frame;

  DEBUG_ONLY(
    size_t checksum = MAGIC_NUMBER;
    uint32_t allocations = 0;
  )
  PmrGuard(BumpAllocator &allocator) noexcept : allocator(allocator){}

  void* do_allocate(size_t bytes, size_t alignment)noexcept final
  {
    DEBUG_ONLY(assert(checksum == MAGIC_NUMBER);
      allocations++;
    )
    return allocator.allocate(bytes, alignment);
  }

  void do_deallocate(void* p, size_t bytes, size_t alignment)noexcept final
  {
    DEBUG_ONLY(assert(checksum == MAGIC_NUMBER);
    allocations--;
    )
  }


  bool do_is_equal(const std::pmr::memory_resource& other) const noexcept final {
    DEBUG_ONLY(assert(checksum == MAGIC_NUMBER);)
    return false;
  }

  operator BumpAllocator&() noexcept
  {
    return allocator;
  }

  DEBUG_ONLY(~PmrGuard() noexcept{
    assert(allocations == 0);
    checksum = 0;
  })
};

template <typename T> struct BumpDeleter
{
  void operator()(T *p) { p->~T(); }
};

template <typename T> using unique = std::unique_ptr<T, BumpDeleter<T>>;

/*

class AllocatorPool
{
  BumpAllocator stable = {};
  BumpAllocator *allocators = nullptr;
  size_t size = 0;
  size_t capacity = 0;

public:
  AllocatorPool() = default;

  BumpAllocator &getStable() noexcept { return stable; }

  size_t idx_of(const BumpAllocator &allocator) const noexcept
  {
    return static_cast<size_t>(&allocator - allocators);
  }
  BumpAllocator &at(size_t idx) noexcept { return allocators[idx]; }
  BumpAllocator &top() noexcept
  {
    if (size == 0)
    {
      push({});
    }
    return allocators[size - 1];
  }

  void push(BumpAllocator &&allocator) noexcept
  {
    if (size == capacity)
    {
      size_t req = sizeof(BumpAllocator) * std::max(capacity * 2, size_t{8});
      allocators = static_cast<BumpAllocator *>(realloc(allocators, req));
    }
    new (&allocators[size++]) BumpAllocator(std::move(allocator));
  }

  BumpAllocator pop() noexcept
  {
    if (size == 0)
    {
      return {};
    }
    size--;
    return std::move(allocators[size]);
  }
  void pop_back() noexcept { size--; }

  AllocatorPool(const AllocatorPool &other) = delete;
  AllocatorPool(AllocatorPool &&other) noexcept = delete;
  AllocatorPool &operator=(const AllocatorPool &other) = delete;
  AllocatorPool &operator=(AllocatorPool &&other) noexcept = delete;
  ~AllocatorPool() noexcept
  {
    for (size_t i = 0; i < size; ++i)
    {
      allocators[i].~BumpAllocator();
    }
  }
};

class OwningBumpGuardBase
{
  AllocatorPool *pool;
  BumpAllocator allocator;
  BumpAllocator::Frame frame;

public:
  BumpAllocator *operator->() noexcept { return &allocator; }

  OwningBumpGuardBase(AllocatorPool &allocatorPool) noexcept;

  void destruct() noexcept;
};

class OwningBumpGuard : public OwningBumpGuardBase
{
  using OwningBumpGuardBase::OwningBumpGuardBase;

  ~OwningBumpGuard() noexcept { destruct(); }
};

template <typename T> struct vector_builder
{
  OwningBumpGuardBase guard;

  void push_back_grow()
  {
    if (size == capacity)
      reserve(std::max(capacity * 2, size_t{8}));
  }

  T *begin = nullptr;
  size_t size = 0;
  size_t capacity = 0;

  vector_builder(AllocatorPool &pool) noexcept : guard(pool)
  {
    capacity = guard->remaining(alignof(T)) / sizeof(T);
    begin = static_cast<T *>(guard->allocate(sizeof(T) * capacity, alignof(T)));
  };

  template <typename Type = T>
    requires std::is_destructible_v<Type> && std::is_copy_constructible_v<Type>
  void resize(size_t new_size) noexcept
  {
    resize(new_size, T{});
  }
  template <typename Type = T>
    requires std::is_copy_constructible_v<Type>
  void resize(size_t new_size, const T &value) noexcept
  {
    size_t old_size = size;
    set_size_helper(new_size);
    for (size_t i = old_size; i < size; i++)
    {
      new (begin + i) T(value);
    }
  }

  void set_size_helper(size_t new_size) noexcept
  {
    if (new_size >= capacity)
    {
      reserve(std::max(new_size, size_t{8}));
    }
    size = new_size;
  }

  template <typename Type = T>
    requires std::is_trivially_destructible_v<Type>
  void setSize(size_t new_size) noexcept
  {
    set_size_helper(new_size);
  }

  void reserve(size_t capacity) noexcept
  {
    if (capacity < this->capacity)
      return;

    auto oldData = begin;
    begin = static_cast<T *>(guard->allocate(capacity * sizeof(T), alignof(T)));
    if constexpr (std::is_trivially_move_assignable_v<T>)
    {
      std::memcpy(begin, oldData, size * sizeof(T));
    }
    else
    {
      for (size_t i = 0; i < size; i++)
      {
        new (begin + i) T(std::move(oldData[i]));
      }
    }
    this->capacity = guard->remaining(alignof(T)) / sizeof(T);
  }

  void push_back(const T &item) noexcept
  {
    push_back_grow();
    new (begin + size++) T(item);
  }
  void push_back(T &&item) noexcept
  {
    push_back_grow();
    new (begin + size++) T(std::move(item));
  }
  template <typename... Args> void emplace_back(Args &&...args) noexcept
  {
    push_back_grow();
    construct_at(size++, std::forward<Args>(args)...);
  }

  template <typename... Args> void construct_at(size_t index, Args &&...args) noexcept
  {
    new (begin + index) T(std::forward<Args>(args)...);
  }
  void pop_back() noexcept
  {
    --size;
    if constexpr (!std::is_trivially_destructible_v<T>)
    {
      begin[size].~T();
    }
  }
  T &operator[](size_t index) noexcept { return begin[index]; }

  std::vector<T> moveToVector() noexcept
  {
    std::vector<T> result;
    result.reserve(this->size);
    result.insert(result.end(), std::make_move_iterator(begin),
                  std::make_move_iterator(begin + size));
    size = 0;
    guard.destruct();
    return result;
  }

  ~vector_builder() noexcept
  {
    if (size == 0)
      return;

    for (size_t i = size; i != 0; i++)
    {
      begin[i - 1].~T();
    }
    size = 0;
    guard.destruct();
  }
};
*/
/*
using pool = AllocatorPool;
using frame_handle = OwningBumpGuard;
*/
using allocator = BumpAllocator;
using pmr_fp = PmrGuard;
using frame_pointer = BumpGuard;

} // namespace bump

