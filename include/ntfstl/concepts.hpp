#pragma once

#include <concepts>
#include <functional>

#define NTF_DEFINE_TEMPLATE_CHECKER(_templ)                          \
  template<typename>                                                 \
  struct _templ##_check : public ::std::false_type {};               \
  template<typename... Ts>                                           \
  struct _templ##_check<_templ<Ts...>> : public ::std::true_type {}; \
  template<typename T>                                               \
  constexpr bool _templ##_check_v = _templ##_check<T>::value;        \
  template<typename T>                                               \
  concept _templ##_type = _templ##_check_v<T>

namespace ntf::meta {

template<typename TL, typename... TR>
concept same_as_any = (... or std::same_as<TL, TR>);

template<typename TL, typename... TR>
concept convertible_to_any = (... or std::convertible_to<TL, TR>);

template<typename T>
concept has_operator_equals = requires(const T a, const T b) {
  { a == b } -> std::convertible_to<bool>;
};

template<typename T>
concept has_operator_nequals = requires(const T a, const T b) {
  { a != b } -> std::convertible_to<bool>;
};

template<typename T, typename U>
concept is_forwarding = std::is_same_v<U, std::remove_cvref_t<T>>;

template<typename T>
concept not_void = !std::is_void_v<T>;

template<typename T>
concept is_nothrow_forward_constructible =
  (std::is_rvalue_reference_v<T> &&
   std::is_nothrow_move_constructible_v<std::remove_cvref_t<T>>) ||
  (std::is_lvalue_reference_v<T> && std::is_nothrow_copy_constructible_v<std::remove_cvref_t<T>>);

template<typename T>
concept is_complete = requires(T obj) {
  { sizeof(obj) };
};

template<typename Ret, typename T, typename... Args>
concept is_const_invocable = requires(const T& obj, Args... args) {
  { std::invoke(obj, std::forward<Args>(args)...) } -> std::convertible_to<Ret>;
};

template<typename Ret, typename T, typename... Args>
concept is_nothrow_const_invocable = requires(const T& obj, Args... args) {
  { std::invoke(obj, std::forward<Args>(args)...) } -> std::convertible_to<Ret>;
  requires noexcept(std::invoke(obj, std::forward<Args>(args)...));
};

template<typename T>
concept move_swappable_type = std::is_move_constructible_v<T> && std::is_move_assignable_v<T>;

} // namespace ntf::meta
