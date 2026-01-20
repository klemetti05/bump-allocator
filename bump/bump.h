//
// Created by Klemens Aimetti on 16.01.26.
//
#pragma once
#include <__algorithm/clamp.h>
#include <__bit/bit_width.h>
#include <array>
#include <cassert>
#include <cstddef>
#include <memory>
#include <memory_resource>
#include <optional>
#include <span>
#include <type_traits>

#ifndef NDEBUG

#define DEBUG_ONLY(X) X
#else
#define DEBUG_ONLY(X)
#endif


namespace bump
{

struct Node
{
  std::byte *index;
  std::byte *end;
  Node *next;
  std::byte payload[];

  Node(size_t cap, Node *nxt) noexcept : index(payload), end(index + cap), next(nxt) {}

  size_t remaining() const noexcept { return end - index; }
  size_t used()const
  {
    return index - payload;
  }
  size_t capacity() const noexcept
  {
    return reinterpret_cast<size_t>(end) - reinterpret_cast<size_t>(payload);
  }
};

struct BumpAllocator
{
  Node *root = nullptr;
  Node *current = nullptr;

public:
  struct Frame
  {
    Node *current;
    std::byte *iterator;
    class Iterator
    {
      friend class formatter;
      Node* current;
      std::byte* min;
      Node* last_block;
      std::byte* max;

    public:
      Iterator(BumpAllocator& allocator, const Frame& first_frame);
      operator bool()const;
      std::span<std::byte> block();
      std::span<char> chars();
      std::string_view string_view()const;
      bool advance();
      size_t count_bytes();
      Iterator copy(){return *this;}
    };
  };

  Frame getFrame() noexcept;
  void restoreFrame(const Frame &frame) noexcept;

  void* allocateUnaligned(size_t bytes)noexcept;

  size_t remaining(size_t align) noexcept;
  size_t remainingBytes() noexcept;
  void truncateCurrentBuffer(std::byte* end) noexcept;
  std::byte* end()noexcept;
  template <typename T> T *push() noexcept
  {
    return static_cast<T *>(allocate(sizeof(T), alignof(T)));
  }

  void *allocate(size_t bytes, size_t align = sizeof(size_t)) noexcept;

  void free() noexcept;

  ~BumpAllocator() noexcept;

  BumpAllocator(std::byte* stack_buffer, size_t capacity) noexcept;

private:
  BumpAllocator(const BumpAllocator &other) = delete;

  BumpAllocator(BumpAllocator &&other) noexcept = delete;

  BumpAllocator &operator=(const BumpAllocator &other) = delete;

  BumpAllocator &operator=(BumpAllocator &&other) noexcept = delete;
  friend class AllocatorPool;
  friend class BumpGuard;
  friend class OwningBumpGuardBase;
};


template<size_t stack_bytes = 4096>
class allocator
{
  std::byte buffer[stack_bytes];
  BumpAllocator bumpAllocator;
public:
  allocator()noexcept
  :bumpAllocator(buffer, stack_bytes)
  {
  }
  operator BumpAllocator& ()noexcept
  {
    return bumpAllocator;
  }
  BumpAllocator& operator->()
  {
    return bumpAllocator;
  }
};

class BumpGuard: public std::pmr::memory_resource
{
  friend class format;
  static constexpr size_t MAGIC_NUMBER = 0x68dec895;
public:

  BumpAllocator& allocator;
  BumpAllocator::Frame frame;

  DEBUG_ONLY(
    size_t checksum = MAGIC_NUMBER;
    uint32_t allocations = 0;
  )

  BumpGuard(BumpAllocator &allocator) noexcept : allocator(allocator), frame(allocator.getFrame()){}

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

  DEBUG_ONLY(~BumpGuard() noexcept{
    assert(allocations == 0);
    checksum = 0;
  })
};


template <typename T> struct BumpDeleter
{
  void operator()(T *p) { p->~T(); }
};

template <typename T> using unique = std::unique_ptr<T, BumpDeleter<T>>;

using frame_ptr = BumpGuard;
using fp_iterator = BumpAllocator::Frame::Iterator;

} // namespace bump

