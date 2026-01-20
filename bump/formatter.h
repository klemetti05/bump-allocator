//
// Created by Klemens Aimetti on 17.01.26.
//

#pragma once
#include "bump.h"
#include <format>

namespace bump{

struct StringBuilder
{
  BumpAllocator::Frame::Iterator iterator;
  BumpAllocator& allocator;

  template<typename T>
  requires requires(T t, char* c)
  {
    t.reserve(256);
    t.insert(t.end(), c, c);
  }
  void into(T& container)
  {
    container.reserve(container.size() + iterator.copy().count_bytes());
    size_t offset = 0;
    if (auto it = iterator.copy())do
    {
      std::string_view block = it.string_view();
      container.insert(container.end(), block.begin(), block.end());
      offset += block.size();
    }while (it.advance());
  }

  BumpAllocator::Frame::Iterator iterate()const
  {
    return iterator;
  }
};

class Formatter
{
  BumpAllocator& allocator;
  BumpAllocator::Frame frame;

  std::optional<char> hint_terminator = '\0';
  size_t buffer_size = 256;
  std::byte* last_append = nullptr;
public:

  explicit Formatter(BumpGuard& frame_pointer, std::optional<char> seperator = {}, size_t buffer_size = 256)
    : allocator(frame_pointer.allocator), frame(allocator.getFrame()), hint_terminator(seperator), buffer_size(buffer_size)
  {

  }

  template<typename...Args>
  [[nodiscard]]
  std::string_view format(const std::format_string<Args...>& fmt, Args&&... args) noexcept
  {
    size_t size = buffer_size;
    int hasTerminator = hint_terminator.has_value();

    char* buf = static_cast<char*>(allocator.allocateUnaligned(size));
    size_t true_size = size - hasTerminator + allocator.remainingBytes();

    auto result = std::format_to_n(buf, true_size, fmt, std::forward<Args>(args)...);
    if (result.size > true_size)
    {
      allocator.truncateCurrentBuffer(reinterpret_cast<std::byte*>(buf));
      buf = static_cast<char*>(allocator.allocateUnaligned(result.size + hasTerminator));
      std::format_to(buf, fmt, std::forward<Args>(args)...);
    }
    else
    {
      allocator.truncateCurrentBuffer(reinterpret_cast<std::byte*>(buf + result.size + hasTerminator));
    }
    if (hasTerminator)
    {
      buf[result.size] = hint_terminator.value();
    }
    return {buf, static_cast<size_t>(result.size)};
  }

  template<typename...Args>
  std::string_view append(const std::format_string<Args...>& fmt, Args&&... args) noexcept
  {
    assert(last_append == nullptr || last_append == allocator.end());
    auto result = format(fmt, std::forward<Args>(args)...);
    last_append = allocator.end();
    return result;
  }

  template<typename...Args>
  std::string_view nullstr(const std::format_string<Args...>& fmt, Args&&... args) noexcept
  {
    auto current = hint_terminator;
    hint_terminator = '\0';
    auto result = format(fmt, std::forward<Args>(args)...);
    hint_terminator = current;
    return result;
  }


  StringBuilder collect()
  {
    assert(last_append == nullptr || last_append == allocator.end());
    last_append = nullptr;
    return StringBuilder{{allocator, frame}, allocator};
  }
};





}