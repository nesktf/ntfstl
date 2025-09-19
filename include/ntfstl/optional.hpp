#pragma once

#include <ntfstl/core.hpp>
#include <ntfstl/concepts.hpp>

// Until i make a optional type
#include <optional>

namespace ntf {

template<typename T>
using optional = std::optional<T>;

using nullopt_t = std::nullopt_t;
constexpr nullopt_t nullopt = std::nullopt;

template<typename T>
struct optional_traits {
  static constexpr bool use_null_value = false;
};

template<typename T>
struct optional_traits<T*> {
  static constexpr bool use_null_value = true;
  static constexpr T* null_value = nullptr;
};

namespace meta {

template<typename T>
struct optional_checker : public std::false_type{};

template<typename T>
struct optional_checker<optional<T>> : public std::true_type{};

template<typename T>
constexpr bool optional_checker_v = optional_checker<T>::value;

template<typename T>
concept optional_type = optional_checker_v<T>;

template<typename T>
concept optional_nullable_type = requires(const T obj) {
  requires optional_traits<T>::use_null_value;
  requires (meta::has_operator_nequals<T> || meta::has_operator_equals<T>);
};


} // namespace meta

namespace impl {

template<typename T>
class optional_union_data {
public:
  constexpr optional_union_data() noexcept :
    _dummy{}, _active{false} {}

  constexpr optional_union_data(nullopt_t) noexcept :
    _dummy{}, _active{false} {}

  template<typename U>
  constexpr optional_union_data(U&& obj)
  noexcept(meta::is_nothrow_forward_constructible<U>) :
    _value(std::forward<U>(obj)), _active{true} {}

  template<typename... Args>
  constexpr optional_union_data(Args&&... args)
  noexcept(std::is_nothrow_constructible_v<T, Args...>) :
    _value(std::forward<Args>(args)...), _active{true} {}

  template<typename... Args, typename U>
  constexpr optional_union_data(Args&&... args, std::initializer_list<U> il)
  noexcept(std::is_nothrow_constructible_v<T, Args..., std::initializer_list<U>>) :
    _value(std::forward<Args>(args)..., il), _active{true} {}

  constexpr ~optional_union_data()
  requires(!std::is_trivially_destructible_v<T>)
  {
    if (has_value()) {
      _value.~T();
    }
  }

  constexpr ~optional_union_data() noexcept
  requires(std::is_trivially_destructible_v<T>) = default;

  constexpr optional_union_data(const optional_union_data& other)
  noexcept(std::is_nothrow_copy_constructible_v<T>) :
    _dummy(), _active{other.has_value()}
  {
    if (_active) {
      new (&_value) T(other.get_value());
    }
  }

  constexpr optional_union_data(optional_union_data&& other)
  noexcept(std::is_nothrow_move_constructible_v<T>) :
    _dummy(), _active{other.has_value()}
  {
    if (_active) {
      new (&_value) T(std::move(other.get_value()));
    }
  }

public:
  constexpr bool has_value() const noexcept { return _active; }

  constexpr T& get_value() noexcept { return _value; }
  constexpr const T& get_value() const noexcept { return _value; }

private:
  union {
    T _value;
    char _dummy;
  };
  bool _active;
};

template<typename T>
requires(meta::optional_nullable_type<T>)
class optional_nullable_data {
public:
  constexpr optional_nullable_data()
  noexcept(std::is_nothrow_copy_constructible_v<T>) :
    _value(optional_traits<T>::null_value) {}

  constexpr optional_nullable_data(nullopt_t)
  noexcept(std::is_nothrow_copy_constructible_v<T>) :
    _value(optional_traits<T>::null_value) {}

  template<typename U>
  constexpr optional_nullable_data(U&& obj)
  noexcept(meta::is_nothrow_forward_constructible<U>) :
    _value(std::forward<U>(obj)) {}

  template<typename... Args>
  constexpr optional_nullable_data(Args&&... args) 
  noexcept(std::is_nothrow_constructible_v<T, Args...>) :
    _value(std::forward<Args>(args)...) {}

  template<typename... Args, typename U>
  constexpr optional_nullable_data(Args&&... args, std::initializer_list<U> il) 
  noexcept(std::is_nothrow_constructible_v<T, Args..., std::initializer_list<U>>) :
    _value(std::forward<Args>(args)..., il) {}

public:
  constexpr bool has_value() const {
    if constexpr (meta::has_operator_nequals<T>) {
      return _value != optional_traits<T>::null_value;
    } else {
      return !(_value == optional_traits<T>::null_value);
    }
  }

  constexpr T& get_value() noexcept { return _value; }
  constexpr const T& get_value() const noexcept { return _value; }

private:
  T _value;
};

} // namespace impl

} // namespace ntf
