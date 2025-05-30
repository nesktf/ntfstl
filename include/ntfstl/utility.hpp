#pragma once

#include <ntfstl/types.hpp>

// #include <variant>

#define NTF_DECLARE_PUBLIC_CAST(_member, _tag) \
struct _tag{}; \
namespace ntf::meta { \
consteval auto public_cast_impl(_tag) noexcept; \
template struct public_cast_t<_tag, _member>; \
template<> \
consteval auto public_cast<_tag>() noexcept { return public_cast_impl(_tag{}); } \
}

namespace ntf {

template<typename... Fs>
struct overload : Fs... { using Fs::operator()...; };

template<typename... Fs>
overload(Fs...) -> overload<Fs...>;

// template<typename... Ts, typename... Fs>
// constexpr decltype(auto) operator|(std::variant<Ts...>& v,
//                                    const overload<Fs...>& overload) {
//   return std::visit(overload, v);
// }
//
// template<typename... Ts, typename... Fs>
// constexpr decltype(auto) operator|(const std::variant<Ts...>& v,
//                                    const overload<Fs...>& overload) {
//   return std::visit(overload, v);
// }

template<typename T>
constexpr T implicit_cast(std::type_identity_t<T> val)
noexcept(std::is_nothrow_move_constructible_v<T>)
{
  return val;
}

namespace meta {

template<typename T = decltype([]{})>
using unique_type_t = T;

/* For the lulz
 *
 * class thing {
 *   int x = 2;
 *   void other_thing() {}
 * };
 * NTF_DECLARE_PUBLIC_CAST(&thing::x, thing_x);
 * NTF_DECLARE_PUBLIC_CAST(&thing::other_thing, thing_other)
 *
 * thing t;
 * t.*ntf::meta::public_cast<thing_x>() = 400;
 * (t.*ntf::meta::public_cast<thing_other>())();
*/
template<typename TagT, auto Member>
struct public_cast_t {
  consteval friend auto public_cast_impl(TagT) noexcept { return Member; }
};
template<typename TagT>
consteval auto public_cast() noexcept {
  return public_cast_impl(TagT{});
}

} // namespace meta

// Lifetime logger thing
template<size_t dummy_sz = 19*sizeof(uint32), bool call_noexcept = true>
struct chiruno_t {
  chiruno_t() : baka{0u} {
    fmt::print("chiruno_t::chiruno_t() [{}]\n", baka);
  }
  chiruno_t(uint32 a) : baka{a} {
    fmt::print("chiruno_t::chiruno_t(uint32) [{}]\n", baka);
  }
  ~chiruno_t() noexcept {
    fmt::print("chiruno_t::~chiruno_t() [{}]\n", baka);
  }
  chiruno_t(chiruno_t&& other) noexcept : baka{other.baka} {
    fmt::print("chiruno_t::chiruno_t(chiruno_t&&) [{}]\n", baka);
  }
  chiruno_t(const chiruno_t& other) noexcept : baka{other.baka} {
    fmt::print("chiruno_t::chiruno_t(const chiruno_t&) [{}]\n", baka);
  }
  chiruno_t& operator=(chiruno_t&&) noexcept {
    fmt::print("chiruno_t::operator=(chiruno_t&&)\n");
    return *this;
  }
  chiruno_t& operator=(const chiruno_t&) noexcept {
    fmt::print("chiruno_t::operator=(const chiruno_t&)\n");
    return *this;
  }

  void operator()() const noexcept(call_noexcept) {
    fmt::print("chiruno_t::operator() [{}]\n", baka);
  }

  uint32 baka;
  uint8 _dummy[dummy_sz];
};

} // namespace ntf
