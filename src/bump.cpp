//
// Created by Klemens Aimetti on 16.01.26.
//
#include "../tracker.h"
#include "include/bump.h"


using namespace bump;

struct bump::Node
{
  std::byte *index;
  std::byte *end;
  Node *next;
  std::byte payload[];

  Node(size_t cap, Node *nxt) noexcept : index(payload), end(index + cap), next(nxt) {}

  size_t remaining() const noexcept { return end - index; }

  size_t capacity() const noexcept
  {
    return reinterpret_cast<size_t>(end) - reinterpret_cast<size_t>(payload);
  }
};


BumpAllocator::Frame BumpAllocator::getFrame() noexcept
{
  return Frame{current, current ? current->index : nullptr};

}


void BumpAllocator::restoreFrame(const Frame &frame) noexcept
{
  if (frame.current)
  {
    current = frame.current;
    current->index = frame.iterator;
  }
  else if (root)
  {
    current = root;
    current->index = frame.iterator;
  }
}

size_t BumpAllocator::remaining(size_t align) noexcept
{
  if (current == nullptr)
    return 0;
  auto raw = reinterpret_cast<std::uintptr_t>(current->index);
  std::uintptr_t aligned_addr = (raw + align - 1) & ~(align - 1);
  size_t padding = aligned_addr - raw;
  return current->remaining() - padding;
}

void *BumpAllocator::allocate(size_t bytes, size_t align) noexcept
{
  if (current)
  {
    auto raw = reinterpret_cast<std::uintptr_t>(current->index);
    std::uintptr_t aligned_addr = (raw + align - 1) & ~(align - 1);
    size_t padding = aligned_addr - raw;
    size_t total_needed = padding + bytes;

    if (total_needed <= current->remaining())
    {
      void *aligned = reinterpret_cast<void *>(aligned_addr);
      current->index = static_cast<std::byte *>(aligned) + bytes;
      return aligned;
    }
  }

  while (true)
  {
    if (current && current->next)
    {
      current = current->next;
    }
    else
    {
      Node *old = current;
      size_t capacity = std::max(size_t{1024}, bytes);
      capacity = current ? std::max(std::min(current->capacity() * 2, 1024UL*256UL), capacity) : capacity;
      current = new (malloc(sizeof(Node) + capacity)) Node(capacity, nullptr);
      if (old)
        old->next = current;
      else
        root = current;
    }

    auto raw = reinterpret_cast<std::uintptr_t>(current->index);
    std::uintptr_t aligned_addr = (raw + align - 1) & ~(align - 1);
    size_t padding = aligned_addr - raw;
    size_t total_needed = padding + bytes;

    if (total_needed <= current->remaining())
    {
      void *aligned = reinterpret_cast<void *>(aligned_addr);
      current->index = static_cast<std::byte *>(aligned) + bytes;
      return aligned;
    }
  }
}

void BumpAllocator::free() noexcept
{
  if (root == nullptr)
    return;

  for (auto it = root; it != nullptr;)
  {
    Node *next = it->next;
    ::free(it);
    it = next;
  }
  root = nullptr;
  current = nullptr;
}

BumpAllocator::~BumpAllocator() noexcept
{
  free();
}

BumpAllocator::BumpAllocator() noexcept : root(nullptr), current(nullptr) {}

BumpAllocator::BumpAllocator(BumpAllocator &&other) noexcept
    : root(other.root), current(other.current)
{
  other.root = nullptr;
  other.current = nullptr;
}

BumpAllocator &BumpAllocator::operator=(BumpAllocator &&other) noexcept
{
  if (this == &other)
    return *this;
  free();
  root = other.root;
  current = other.current;
  other.root = nullptr;
  other.current = nullptr;
  return *this;
}

/*
OwningBumpGuardBase::OwningBumpGuardBase(AllocatorPool &allocatorPool) noexcept
    : pool(&allocatorPool)
{
  auto &top = pool->top();
  new (&allocator) BumpAllocator(std::move(top));
  frame = allocator.getFrame();
  pool->pop();
}
void OwningBumpGuardBase::destruct() noexcept
{
  if (pool != nullptr)
  {
    allocator.restoreFrame(frame);
    pool->push(std::move(allocator));
    pool = nullptr;
  }
}*/