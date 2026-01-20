//
// Created by Klemens Aimetti on 18.01.26.
//
#pragma once
#include <format>


#define default_formatter(type, string, ...)\
template<>\
struct std::formatter<type, char>\
{\
constexpr auto parse(std::format_parse_context& ctx){ return ctx.begin(); }\
auto format(const type& self, std::format_context& ctx)const{\
return std::format_to(ctx.out(), string, __VA_ARGS__);\
}\
};


struct SwitchFormatterBase {
  char mode = 0;

  constexpr auto parse(std::format_parse_context& ctx) {
    auto it = ctx.begin();
    if (it != ctx.end() && *it != '}') {
      mode = *it++;
    }
    return it;
  }
};

#define switch_formatter(type, cases)\
template<>\
struct std::formatter<type, char>: SwitchFormatterBase\
{\
  auto format(const type& self, std::format_context& ctx)const{\
    switch (mode)\
    {\
       cases \
       default: return std::format_to(ctx.out(), "error: invalid flags for {}", #type);\
    }\
  }\
};
#define formatter_case(mode, string, ...)\
case mode: return std::format_to(ctx.out(), string, __VA_ARGS__);

