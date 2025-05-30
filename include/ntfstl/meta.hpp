#pragma once

#include <ntfstl/core.hpp>
#include <ntfstl/concepts.hpp>
#include <ntfstl/types.hpp>
#include <ntfstl/span.hpp>

#include <algorithm>
#include <bit>

namespace ntf::meta {

template<typename T>
consteval cspan<char> typename_base() { return {NTF_FUNC}; }

template<typename T>
consteval cspan<char> parse_typename();

template<>
consteval cspan<char> parse_typename<void>() { return {"void", 4}; }

template<typename T>
consteval cspan<char> parse_typename() {
  constexpr auto void_str = typename_base<void>();
  constexpr auto void_name = parse_typename<void>();
  constexpr auto void_iter = std::search(void_str.begin(), void_str.end(),
                                         void_name.begin(), void_name.end());

  constexpr auto prefix_len = void_iter - std::begin(void_str);
  constexpr auto suffix_len = void_str.size() - prefix_len - void_name.size();

  constexpr auto type_str = typename_base<T>();
  constexpr auto type_name_len = type_str.size() - prefix_len - suffix_len;
  constexpr auto type_name_iter = std::begin(type_str) + prefix_len;

  return {type_name_iter, type_name_len};
}

using type_id_t = uint64;
constexpr type_id_t NULL_TYPE_ID = 0u;

template<typename T>
struct meta_traits {
private:
  static constexpr auto _name_span = parse_typename<T>();

public:
  static constexpr std::string_view type_name{_name_span.begin(), _name_span.end()};

public:
  static type_id_t get_id() noexcept { return std::bit_cast<type_id_t>(_name_span.begin()); }
};

template<typename T, typename U>
struct rebind_first_arg {};
template<template<typename, typename...> class Templ, typename U, typename T, typename... Ts>
struct rebind_first_arg<Templ<T, Ts...>, U> {
  using type = Templ<U, Ts...>;
};
template<typename T, typename U>
using rebind_first_arg_t = rebind_first_arg<T, U>::type;

} // namespace ntf::meta
