//
// Created by Klemens Aimetti on 16.01.26.
//
#pragma once
#include <array>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <memory>
#include <memory_resource>
#include <optional>
#include <span>
#include <type_traits>
#include <vector>
#include <bit>

#ifndef NDEBUG

#define DEBUG_ONLY(X) X
#else
#define DEBUG_ONLY(X)
#endif

#ifdef BUMP_TRACK_HEAP
#include "bump/HeapState.h"
#define IF_TRACKING(X) X

#else
#define IF_TRACKING(X)

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
  size_t full_size()const noexcept
  {
    return end - reinterpret_cast<const std::byte*>(this);
  }
  bool corrupt()const
  {
    return index < payload || index > end;
  }
};

struct BumpAllocator
{
  IF_TRACKING(TrackedAllocInfo info);
  Node *root = nullptr;
  Node *current = nullptr;
public:
  struct Frame
  {
    Node* current;
    std::byte * iterator;

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
      bool fragmented()const
      {
        return current == last_block;
      }
    };
  };

  Frame getFrame() noexcept;
  void restoreFrame(const Frame &frame) noexcept;

  void* allocateUnaligned(size_t bytes)noexcept;

  size_t remaining(size_t align) noexcept;
  size_t remainingBytes() noexcept;
  std::byte* end()noexcept;
  template <typename T> T *push() noexcept
  {
    return static_cast<T *>(allocate(sizeof(T), alignof(T)));
  }

  void SetName(const char* name)
  {
    IF_TRACKING(info.SetName(name));
  }
  void *allocate(size_t bytes, size_t align = sizeof(size_t)) noexcept;
  void *try_allocate(size_t bytes, size_t align = sizeof(size_t)) noexcept;
  void *allocate_if_failed(size_t bytes, size_t align = sizeof(size_t)) noexcept;

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
  BumpAllocator* operator->()
  {
    return &bumpAllocator;
  }
};

class bucket_allocator;
template<class T>
 struct BucketUniquePtrDeleter
{
  bucket_allocator* allocator;


  void operator()(T* ptr);
};
template<class T>
struct BucketUniquePtrDeleter<T[]>
{
  bucket_allocator* allocator;
  size_t count;
  void operator()(T* ptr);
};

template<typename T>
using bucket_unique_ptr = std::unique_ptr<T, BucketUniquePtrDeleter<T>>;

class bucket_allocator: public std::pmr::memory_resource
{
  BumpAllocator& allocator;
  IF_TRACKING(size_t user_alloc = 0;)
public:

  struct Node
  {
    Node* next;

    void* payload()
    {
      next = nullptr;
      return reinterpret_cast<void*>(this);
    }
  };
  struct AlignedAllocation
  {
    Node* begin;
    std::byte* paylaod;
  };
  bucket_allocator(BumpAllocator& alloc)
    :allocator(alloc)
  {

  }

  std::array<Node*, 24> default_alignent = {};
  size_t has_any = 0;

  static size_t bucket_func(size_t size)
  {
    if (size < 8) return 0;
    size /= 8;
    return std::bit_width(size - 1);
  }
  static size_t size_func(size_t bucket)
  {
    return (1ULL << bucket)*8;
  }
  static size_t true_size(size_t size, size_t align)
  {
    if (align <= 8)
    {
      return size_func(bucket_func(size));
    }
    throw std::bad_alloc();
  }

  void pushNode(Node* node, size_t bucket)
  {
    node->next = default_alignent[bucket];
    default_alignent[bucket] = node;
    has_any |= (1ULL << bucket);
  }
  Node* tryPopNode(size_t bucket)
  {
    Node* node = default_alignent[bucket];
    if (node)
    {
      default_alignent[bucket] = node->next;
      if (default_alignent[bucket] == nullptr)
        has_any &= ~(1ULL << bucket);
    }
    return node;
  }
  Node* popNode(size_t bucket)
  {
    Node* node = tryPopNode(bucket);
    if (node)return node;//95%
    size_t any = has_any >> bucket;
    size_t first = std::countr_zero(any);
    if (first < 5)//split nodes
    {
      auto chunk = reinterpret_cast<std::byte*>(tryPopNode(first + bucket));
      size_t chunk_size = size_func(bucket + first);
      size_t size = size_func(bucket);
      for (size_t i = size; i < chunk_size; i += size)
      {
        chunk += size;
        pushNode(reinterpret_cast<Node*>(chunk + i), bucket);
      }
      return reinterpret_cast<Node*>(chunk);
    }
    return nullptr;
  }



  void* do_allocate(size_t size, size_t align)final
  {

    if (align <= sizeof(std::max_align_t))
    {
      size_t bucket = bucket_func(size);

      if (Node* node = tryPopNode(bucket))//99%
      {
        return node->payload();
      }

      size_t bucket_size = size_func(bucket);

      void* ptr = allocator.try_allocate(bucket_size, align);//80-99%
      //old path: allocator.allocate(bucket_size, align); aka try allocate and if failed allocate if failed
      //->failed allocation is more expensive, but wastes no memory.

      if (ptr == nullptr)
      {
        size_t remaining = allocator.remaining(align);
        auto temp = static_cast<std::byte*>(allocator.try_allocate(remaining, align));
        assert(temp);
        while (true)
        {
          size_t cell = std::bit_width(remaining/8) - 1;
          size_t cell_size = size_func(cell);
          if (cell >= default_alignent.size())break;
          pushNode(new(temp)Node(), cell);

          temp += cell_size;
          remaining -= cell_size;
        }
        ptr = allocator.allocate_if_failed(bucket_size, align);
      }
      Node* allocation = new (ptr)Node();
      return allocation->payload();
    }
    throw std::bad_alloc();
  }

  void do_deallocate(void* data, size_t size, size_t align)final
  {
    if (align <= sizeof(std::max_align_t))
    {
      size_t bucket = bucket_func(size);
      IF_TRACKING(user_alloc -= size_func(bucket));
      pushNode(static_cast<Node*>(data), bucket);
    }
    else
    {
      throw std::bad_alloc();
    }
  }
  bool do_is_equal(memory_resource const& other) const noexcept final
  {
    return &other == this;
  }

  operator BumpAllocator& ()noexcept
  {
    return allocator;
  }
  template<typename T, typename... Args>
  requires (!std::is_array_v<T>)
  bucket_unique_ptr<T> make_unique(Args&&...args)
  {

    T* obj = new (allocate(sizeof(T), alignof(T)))T(std::forward<Args...>(args...));
    return bucket_unique_ptr<T>(obj, BucketUniquePtrDeleter<T>(this));
  }
  template<typename T>
  requires (std::is_array_v<T>)
  bucket_unique_ptr<T> make_unique_array(size_t count, bool initialize){
    using ElementType = std::remove_extent_t<T>;

    void* alloc = allocate(sizeof(ElementType) * count, alignof(ElementType));
    ElementType* obj = initialize?new (alloc)ElementType[count]()
    : new (alloc)ElementType[count];
    return bucket_unique_ptr<T>(obj, BucketUniquePtrDeleter<T>(this, count));
  }
  IF_TRACKING(
  ~bucket_allocator() noexcept
  {

  }
  );
};

template<typename T>
struct bucket_ceil
{
  size_t count;
  bucket_ceil(size_t count): count(bucket_allocator::true_size(count * sizeof(T), alignof(T)) / sizeof(T)){}

  operator size_t()
  {
    return count;
  }
};

template<typename T>
void BucketUniquePtrDeleter<T>::operator()(T* ptr)
{
  if (ptr)
  {
    ptr->~T();
    allocator->deallocate(ptr, sizeof(T), alignof(T));
  }
}
template<typename T>
void BucketUniquePtrDeleter<T[]>::operator()(T* ptr)
{
  if (ptr)
  {
    for (size_t i = 0; i < count; ++i)
      ptr->~T();
    allocator->deallocate(ptr, sizeof(T) * count, alignof(T));
  }
}


class BumpGuard: public std::pmr::memory_resource
{
  friend class format;
  static constexpr size_t MAGIC_NUMBER = 0x68dec895;
public:

  BumpAllocator& allocator;
  const BumpAllocator::Frame frame;

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
    return &other == this;
  }

  operator BumpAllocator&() noexcept
  {
    return allocator;
  }

  ~BumpGuard() noexcept{
    DEBUG_ONLY(assert(allocations == 0);
    checksum = 0;)
    allocator.restoreFrame(frame);
  }
};


template <typename T> struct BumpDeleter
{
  void operator()(T *p) { p->~T(); }
};

template <typename T> using unique = std::unique_ptr<T, BumpDeleter<T>>;

using frame_ptr = BumpGuard;
using fp_iterator = BumpAllocator::Frame::Iterator;

} // namespace bump

