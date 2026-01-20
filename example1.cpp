//
// Created by Klemens Aimetti on 17.01.26.
//

#include "bump/default_formatter.h"
#include "bump/formatter.h"
#include <format>
#include <fstream>
#include <iostream>
#include <print>
#include <vector>

class RabinKarp
{
  using HASH = size_t;

public:

  static HASH charhash(char c)
  {
    return (c - '0') % 7;
  }
  static HASH hashStr(std::string_view str)
  {
    HASH hash = 0;
    for (size_t i = 0; i < str.size(); ++i)
    {
      hash += charhash(str[i]);
    }
    return hash;
  }
  static bool checkMatch(std::string_view str, HASH whash, size_t i, std::string_view p, size_t phash)
  {
    auto windowStr =  str.substr(i, p.size());

    bool success = whash == phash;
    std::print("substr: {} ", windowStr);
    std::print("cmp {:2} {} {:2}:  {:4}", whash, success?"==": "!=", phash, success?" -> ": " skip ");
    if (!success)
      return false;

    success = windowStr == p;

    std::print("strcmp '{}' {} '{}': {}", windowStr, success?"==": "!=", p, success?"match": "skip");
    return success;
  }
  static std::vector<size_t> find(std::string_view str, std::string_view p)
  {
    std::println("string: {}", str);

    std::vector<size_t> result;
    HASH pattern_hash = hashStr(p);
    std::println("pattern: {} hash: {}", p, pattern_hash);

    HASH whash = hashStr(str.substr(0, p.size()));

    if (checkMatch(str, whash, 0, p, pattern_hash))
    {
      result.push_back(0);
    }
    std::putc('\n', stdout);

    for (size_t i = p.size(); i < str.size(); ++i)
    {
      char pop = str[i - p.size()];
      char push = str[i];
      whash -= charhash(pop);
      whash += charhash(push);
      if (checkMatch(str, whash, i - p.size() + 1, p, pattern_hash))
      {
        result.push_back(i - p.size() + 1);
      }
      std::putc('\n', stdout);
    }
    return result;
  }
};

class KMP
{
  int i = 0;
  int table_i = 0;
public:
  int index = 0;

  static size_t tableSize(std::string_view pattern)
  {
    return pattern.size();
  }
  static void MakeTable(std::string_view str, std::span<int> table_buf)
  {
    assert(table_buf.size() >= str.size());
    table_buf[0] = 0;
    int i = 1;
    int streak = 0;

    while (i < str.size())
    {
      if (str[i] == str[streak])
      {
        ++streak;
        table_buf[i] = streak;
        ++i;
      }
      else
      {
        if (streak != 0)
        {
          streak = table_buf[streak];
        }
        else
        {
          table_buf[i] = 0;
          i++;
        }
      }
    }
  }

  bool find(std::string_view text, std::string_view pattern, std::span<int> table)
  {
    assert(table.size() >= pattern.size());
    int m = pattern.size();
    int n = text.size();

    while (i < n)
    {
      char c = text[i];
      if (pattern[table_i] == c)
      {
        ++i;
        ++table_i;
        if (table_i == m)
        {
          index = i - table_i;
          table_i = table.back();
          return true;
        }
      }

      if (i < n && pattern[table_i] != text[i])
      {
        if (table_i != 0)
          table_i = table[table_i - 1];
        else ++i;
      }
    }
    return false;
  }
};


struct vec3
{
  float x,y,z;
};
default_formatter(vec3, "{{{}, {}, {}}}", self.x, self.y, self.z);

using uint = unsigned int;

int main(int args, char** argv)
{
  std::string string = "2771727372727";
  std::string pattern = "1010";

  auto matches = RabinKarp::find(string, pattern);
  for (auto match: matches)
  {
    std::println("found at {}", match);
  }

  std::vector<int> table(KMP::tableSize(pattern));
  KMP::MakeTable(pattern, table);
  std::println("table: {}, seq: {}",table, pattern);

  KMP search;
  while (search.find(string, pattern, table))
  {
    std::println("match {}", search.index);
  }

  return 0;

  vec3 v{0,0,0};
  std::println("{}", v);
  std::println("{}", v);


  if (args < 2)
  {
    std::println(stderr, "usage ./example <path to file>");
    return 1;
  }
  bump::allocator allocator;
  bump::frame_ptr ptr(allocator);

  std::filesystem::path path(argv[1]);
  auto length = std::filesystem::file_size(path);

  std::pmr::vector<char> buffer(length + 1);

  if(std::ifstream file = std::ifstream(path, std::ios::binary | std::ios::in))
  {
    file.read(buffer.data(), length);
    file.close();
  }
  else
  {
    std::println(stderr, "failed to open file: {}", argv[1]);
    return 2;
  }
  buffer.push_back('\0');

  std::println("{}", buffer.data());

  return 0;

}