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
  void into(T& container, bool nullTerminator = false)
  {
    container.reserve(container.size() + iterator.copy().count_bytes() + nullTerminator);
    size_t offset = 0;
    if (auto it = iterator.copy())do
    {
      std::string_view block = it.string_view();
      container.insert(container.end(), block.begin(), block.end());
      offset += block.size();
    }while (it.advance());
    if (nullTerminator)
    {
      container.push_back('\0');
    }
  }
  std::string_view string_view()
  {
    if (!iterator.fragmented())
    {
      return iterator.string_view();
    }
    char* str = static_cast<char*>(allocator.allocateUnaligned(iterator.copy().count_bytes()));
    char* dst = str;

    if (auto it = iterator.copy())do
    {
      auto v = it.string_view();
      std::memcpy(dst, v.data(), v.size());
      dst += v.size();
    }while (it.advance());
    return {str, static_cast<size_t>(dst - str)};
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

  explicit Formatter(const BumpGuard& frame_pointer, std::optional<char> seperator = {}, size_t buffer_size = 256)
    : allocator(frame_pointer.allocator), frame(allocator.getFrame()), hint_terminator(seperator), buffer_size(buffer_size)
  {

  }

  template<typename...Args>
  [[nodiscard]]
  std::string_view format(const std::format_string<Args...>& fmt, Args&&... args) noexcept
  {
    assert(allocator.end() >= allocator.current->payload);

    int hasTerminator = hint_terminator.has_value();

    char* buf = reinterpret_cast<char*>(allocator.end());

    //char* buf = static_cast<char*>(allocator.allocateUnaligned(size));
    size_t true_size = allocator.remainingBytes() - hasTerminator;

    auto result = std::format_to_n(buf, true_size, fmt, std::forward<Args>(args)...);
    buf = static_cast<char*>(allocator.allocateUnaligned(result.size + hasTerminator));

    if (result.size > true_size)
    {
      std::format_to(buf, fmt, std::forward<Args>(args)...);
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
    if (last_append == nullptr)
    {
      frame = allocator.getFrame();
    }
    assert(last_append == nullptr || last_append == allocator.end());
    auto result = format(fmt, std::forward<Args>(args)...);
    last_append = allocator.end();
    return result;
  }
  void start()
  {
    assert(last_append == nullptr);
    frame = allocator.getFrame();

    last_append = allocator.end();
  }
  void appendToBuffer(std::string_view str)
  {
    assert(last_append == allocator.end());

    char* buf = static_cast<char*>(allocator.allocateUnaligned(str.size()));
    std::copy_n(str.data(), str.size(), buf);

    last_append = allocator.end();
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
  static char* append_to_buffer(char* buffer, std::string_view str)
  {
    std::copy_n(str.data(), str.size(), buffer);
    buffer += str.size();
    return buffer;
  }
};





}