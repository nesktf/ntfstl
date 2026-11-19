#ifndef NTF_UTILLITY_HPP_
#define NTF_UTILLITY_HPP_

#include <ntf/core.hpp>
#include <ntf/impl/concepts.hpp>

namespace ntf {

template<typename T>
constexpr const T& max(const T& a, const T& b) {
  return a < b ? b : a;
}

template<typename T>
constexpr const T& min(const T& a, const T& b) {
  return a < b ? a : b;
}

template<typename T>
NTF_NODISCARD constexpr const T* as_const(T* p) noexcept {
  return p;
}

template<typename T>
NTF_NODISCARD constexpr const T* as_const(const T* p) noexcept {
  return p;
}

template<typename T>
NTF_NODISCARD constexpr const T& as_const(T& p) noexcept {
  return p;
}

template<typename T>
NTF_NODISCARD constexpr const T& as_const(const T& p) noexcept {
  return p;
}

template<typename T, typename U = T>
constexpr T exchange(T& obj, U&& new_value) noexcept(meta::nothrow_move_constructible<T> &&
                                                     meta::nothrow_copy_assignable<T>) {
  T old = move(obj);
  obj = forward<U>(new_value);
  return old;
}

} // namespace ntf

#endif // NTF_UTILLITY_HPP_
