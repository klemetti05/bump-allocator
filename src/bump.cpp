//
// Created by Klemens Aimetti on 16.01.26.
//
#include "bump/bump.h"


using namespace bump;


BumpAllocator::Frame BumpAllocator::getFrame() noexcept
{
  return Frame{current, current->index};

}


void BumpAllocator::restoreFrame(const Frame &frame) noexcept
{
  assert(frame.current);
  current = frame.current;
  current->index = frame.iterator;
  IF_TRACKING(info.total_free += current->index - frame.iterator);


  for (auto it = current->next; it; it = it->next){
    IF_TRACKING(info.total_free += current->used(););
    current->index = current->payload;
  }
}


size_t BumpAllocator::remaining(size_t align) noexcept
{
  auto raw = reinterpret_cast<std::uintptr_t>(current->index);
  std::uintptr_t aligned_addr = (raw + align - 1) & ~(align - 1);
  size_t padding = aligned_addr - raw;
  return current->remaining() - padding;
}
size_t BumpAllocator::remainingBytes()noexcept
{
  return current->remaining();
}
std::byte* BumpAllocator::end()noexcept
{
  return current->index;
}

void* BumpAllocator::allocateUnaligned(size_t bytes)noexcept
{
  return allocate(bytes, 1);
}

auto get_aligned(Node* n, size_t align){
  auto raw = reinterpret_cast<std::uintptr_t>(n->index);
  return reinterpret_cast<std::byte*>((raw + align - 1) & ~(align - 1));
}

void* BumpAllocator::try_allocate(size_t bytes, size_t align) noexcept
{
  std::byte* aligned_ptr = get_aligned(current, align);
  if (aligned_ptr + bytes <= current->end) {
    IF_TRACKING(info.total_free -= (aligned_ptr + bytes) - current->index);
    current->index = aligned_ptr + bytes;
    return aligned_ptr;
  }
  return nullptr;
}
void* BumpAllocator::allocate_if_failed(size_t bytes, size_t align) noexcept
{
  std::byte* aligned_ptr = get_aligned(current, align);
  if (aligned_ptr + bytes <= current->end) {
    IF_TRACKING(info.total_free -= (aligned_ptr + bytes) - current->index);
    current->index = aligned_ptr + bytes;
    return aligned_ptr;
  }

  while (true) {
    if (current->next) {
      current = current->next;
      current->index = current->payload;
    } else {
      size_t exponential = std::min(current->capacity() * 2, 256UL * 1024UL);
      size_t minimum = std::max(size_t{1024}, bytes + align - 1);
      size_t capacity = std::max(minimum, exponential);
      auto allocation = std::allocator<std::byte>{}.allocate_at_least(sizeof(Node) + capacity);
      current->next = new (allocation.ptr) Node(allocation.count - sizeof(Node), nullptr);
      current = current->next;

      IF_TRACKING(info.total_free += (allocation.count));
      IF_TRACKING(info.total_malloc += (allocation.count));
    }

    aligned_ptr = get_aligned(current, align);
    if (aligned_ptr + bytes <= current->end) {
      IF_TRACKING(info.total_free -= (aligned_ptr + bytes) - current->index);

      current->index = aligned_ptr + bytes;
      return aligned_ptr;
    }
  }
}

void *BumpAllocator::allocate(size_t bytes, size_t align) noexcept
{
  if (void* alloc = try_allocate(bytes, align))
  {
    return alloc;
  }
  return allocate_if_failed(bytes, align);
}

void BumpAllocator::free() noexcept
{
  for (auto it = root->next; it != nullptr;)
  {
    Node *next = it->next;
    size_t node_size = it->full_size();
    std::allocator<std::byte>{}.deallocate(reinterpret_cast<std::byte*>(it), node_size);
    IF_TRACKING(info.total_malloc -= node_size);
    IF_TRACKING(info.total_free -= it->remaining(););
    it = next;
  }
  IF_TRACKING(
    info.total_malloc -= root->full_size();
    info.total_free -= root->remaining();
    );


  root->index = root->payload;
  root->next = nullptr;
  current = root;
}

BumpAllocator::~BumpAllocator() noexcept
{
  free();
}

BumpAllocator::BumpAllocator(std::byte* stack_buffer, size_t capacity) noexcept
{
  root = new (stack_buffer) Node(capacity - sizeof(Node), nullptr);
  current = root;
  IF_TRACKING(info.total_free = root->remaining());
  IF_TRACKING(info.total_malloc = root->remaining());
}

using iterator = typename BumpAllocator::Frame::Iterator;

iterator::Iterator(BumpAllocator& allocator, const Frame& first_frame)
{
  current = first_frame.current;
  min = first_frame.iterator;
  last_block = allocator.current;
  max = last_block->index;
}
bool iterator::advance()
{
  if (current == last_block)
  {
    return false;
  }
  current = current->next;
  min = current->payload;
  return true;
}

size_t iterator::count_bytes()
{
  size_t bytes = 0;
  if (*this) do
  {
    bytes += block().size_bytes();
  }while (advance());

  return bytes;
}

iterator::operator bool()const
{
  return current != nullptr;
}

std::span<std::byte> iterator::block()
{
  return {min, current == last_block? max: current->index};
}
std::span<char> iterator::chars()
{
  auto* end = (current == last_block) ? max : current->index;
  return {
    reinterpret_cast<char*>(min),
    reinterpret_cast<char*>(end)
  };
}

std::string_view iterator::string_view() const
{
  auto* end = (current == last_block) ? max : current->index;
  return {
    reinterpret_cast<const char*>(min),
    static_cast<size_t>(end - min)
  };
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