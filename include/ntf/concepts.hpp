#ifndef NTF_CONCEPTS_HPP_
#define NTF_CONCEPTS_HPP_

#include <ntf/core.hpp>

namespace ntf::meta {

template<typename T>
concept default_constructible = requires() { T{}; };

template<typename T>
concept nothrow_default_constructible = requires() { requires noexcept(T{}); };

template<typename T>
concept nothrow_move_constructible = requires(T&& obj) { requires noexcept(T(::ntf::move(obj))); };

template<typename T>
concept nothrow_move_assignable = requires(T&& obj) { requires noexcept(obj = ::ntf::move(obj)); };

template<typename T>
concept copy_constructible = requires(const T& obj) { T(obj); };

template<typename T>
concept nothrow_copy_constructible = requires(const T& obj) { requires noexcept(T(obj)); };

template<typename T>
concept nothrow_destructible = requires(T obj) { requires noexcept(obj.~T()); };

} // namespace ntf::meta

#endif // NTF_CONCEPTS_HPP_
