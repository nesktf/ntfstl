#ifndef NTF_OPTIONAL_HPP_
#define NTF_OPTIONAL_HPP_

#include <ntf/impl/concepts.hpp>
#include <ntf/memory.hpp>

namespace ntf {

class BadOptionalAccess final : public Exception {
public:
  const char* what() const noexcept override { return "BadOptionalAccess"; }
};

struct nullopt_t {};

constexpr inline nullopt_t nullopt;

namespace meta {

template<typename T>
concept nullable_type = !meta::same_as<T, in_place_t> && !meta::same_as<T, nullopt_t> &&
                        !meta::is_void_v<T> && !meta::is_reference_v<T>;

} // namespace meta

template<meta::nullable_type T>
class Optional;

template<typename T>
using Nullable = Optional<T>;

namespace meta {

template<typename T>
struct optional_checker : public false_type {};

template<typename T>
struct optional_checker<Optional<T>> : public true_type {};

template<typename T>
constexpr bool optional_checker_v = optional_checker<T>::value;

template<typename T>
concept optional_type = optional_checker_v<T>;

} // namespace meta

namespace impl {

template<typename T>
class OptionalData {
public:
  constexpr OptionalData() noexcept : _dummy{}, _active{false} {}

  constexpr OptionalData(nullopt_t) noexcept : _dummy{}, _active{false} {}

  constexpr OptionalData(T&& obj) noexcept(meta::nothrow_move_constructible<T>) :
      _value(::ntf::move(obj)), _active{true} {}

  constexpr OptionalData(const T& obj) noexcept(meta::nothrow_copy_constructible<T>) :
      _value(obj), _active{true} {}

  template<typename... Args>
  constexpr OptionalData(in_place_t,
                         Args&&... args) noexcept(meta::nothrow_constructible<T, Args...>) :
      _value(::ntf::forward<Args>(args)...), _active{true} {}

#if 0
  template<typename U, typename... Args>
  constexpr OptionalData(in_place_t, std::initializer_list<U> il, Args&&... args) noexcept(
    meta::nothrow_constructible<T, std::initializer_list<U>, Args...>) :
      _value(il, ::ntf::forward<Args>(args)...), _active{true} {}
#endif

  constexpr ~OptionalData()
  requires(!meta::trivially_destructible<T>)
  {
    destroy();
  }

  constexpr ~OptionalData() noexcept
  requires(meta::trivially_destructible<T>)
  = default;

  constexpr OptionalData(const OptionalData& other) noexcept(meta::nothrow_copy_constructible<T>)
  requires(!meta::trivially_copy_constructible<T>)
      : _dummy(), _active{other.has_value()} {
    if (_active) {
      construct(other.get_value());
    }
  }

  constexpr OptionalData(const OptionalData& other) noexcept
  requires(meta::trivially_copy_constructible<T>)
  = default;

  template<typename U>
  constexpr OptionalData(const OptionalData<U>& other) noexcept(meta::nothrow_constructible<T, U>)
  requires(!meta::same_as<U, meta::remove_cv_t<T>> && meta::convertible_to<U, T>)
      : _value(other.get_value()), _active{true} {}

  constexpr OptionalData(OptionalData&& other) noexcept(meta::nothrow_move_constructible<T>)
  requires(!meta::trivially_move_constructible<T>)
      : _dummy(), _active{other.has_value()} {
    if (_active) {
      construct(::ntf::move(other.get_value()));
    }
  }

  constexpr OptionalData(OptionalData&& other) noexcept
  requires(meta::trivially_move_constructible<T>)
  = default;

  template<typename U>
  constexpr OptionalData(OptionalData<U>&& other) noexcept(meta::nothrow_constructible<T, U&&>)
  requires(!meta::same_as<U, meta::remove_cv_t<T>> && meta::convertible_to<U &&, T>)
      : _value(::ntf::move(other.get_value())), _active{true} {}

public:
  constexpr OptionalData& operator=(const OptionalData&) noexcept
  requires(meta::trivially_copy_assignable<T>)
  = default;

  constexpr OptionalData&
  operator=(const OptionalData& other) noexcept(meta::nothrow_copy_assignable<T>) {
    auto do_thing = [&, this]() {
      _active = other.has_value();
      if (_active) {
        _value = other.get_value();
      }
    };

    destroy();

    if constexpr (!meta::nothrow_copy_assignable<T>) {
      try {
        do_thing();
      } catch (...) {
        _active = false;
        throw;
      }
    } else {
      do_thing();
    }

    return *this;
  }

  constexpr OptionalData& operator=(OptionalData&&) noexcept
  requires(meta::trivially_move_assignable<T>)
  = default;

  constexpr OptionalData&
  operator=(OptionalData&& other) noexcept(meta::nothrow_move_assignable<T>) {
    auto do_thing = [&, this]() {
      _active = other.has_value();
      if (_active) {
        _value = ::ntf::move(other.get_value());
      }
    };

    destroy();

    if constexpr (meta::nothrow_move_assignable<T>) {
      try {
        do_thing();
      } catch (...) {
        _active = false;
        throw;
      }
    } else {
      do_thing();
    }

    return *this;
  }

protected:
  constexpr bool has_value() const noexcept { return _active; }

  constexpr T& get_value() noexcept { return _value; }

  constexpr const T& get_value() const noexcept { return _value; }

protected:
  constexpr void destroy() noexcept {
    if constexpr (!meta::trivially_destructible<T>) {
      if (has_value()) {
        _value.~T();
      }
    }
    _active = false;
  }

  template<typename... Args>
  constexpr void construct(Args&&... args) noexcept(meta::nothrow_constructible<T, Args...>) {
    NTF_PNEW(::ntf::addressof(_value)) T(::ntf::forward<Args>(args)...);
    _active = true;
  }

#if 0
  template<typename U, typename... Args>
  constexpr void construct(std::initializer_list<U> il,
                           Args&&... args) noexcept(meta::nothrow_constructible<T, Args...>) {
    NTF_PNEW(::ntf::addressof(_value)) T(il, ::ntf::forward<Args>(args)...);
    _active = true;
  }
#endif

private:
  union {
    T _value;
    char _dummy;
  };

  bool _active;
};
} // namespace impl

template<meta::nullable_type T>
class Optional : public impl::OptionalData<T> {
private:
  using base_t = impl::OptionalData<T>;

public:
  using impl::OptionalData<T>::OptionalData;

public:
  constexpr T& value() & {
    NTF_THROW_IF(!base_t::has_value(), BadOptionalAccess());
    return base_t::get_value();
  }

  constexpr T&& value() && {
    NTF_THROW_IF(!base_t::has_value(), BadOptionalAccess());
    return ::ntf::move(base_t::get_value());
  }

  constexpr const T& value() const& {
    NTF_THROW_IF(!base_t::has_value(), BadOptionalAccess());
    return base_t::get_value();
  }

  constexpr const T&& value() const&& {
    NTF_THROW_IF(!base_t::has_value(), BadOptionalAccess());
    return ::ntf::move(base_t::get_value());
  }

  template<typename U = meta::remove_cv_t<T>>
  constexpr T value_or(U&& def_value) const&
  requires(meta::copy_constructible<T> && meta::convertible_to<U &&, T>)
  {
    return has_value() ? base_t::get_value() : static_cast<T>(::ntf::forward<U>(def_value));
  }

  template<typename U = meta::remove_cv_t<T>>
    constexpr T value_or(U&& def_value) &&
    requires(meta::move_constructible<T>&& meta::convertible_to<U&&, T>) {
      return has_value() ? ::ntf::move(base_t::get_value())
                         : static_cast<T>(::ntf::forward<U>(def_value));
    }

    template<typename... Args>
    constexpr T& emplace(Args&&... args) noexcept(meta::nothrow_constructible<T, Args...>) {
    base_t::destroy();
    base_t::construct(::ntf::forward<Args>(args)...);
    return base_t::get_value();
  }

#if 0
  template<typename U, typename... Args>
  constexpr T& emplace(std::initializer_list<U> il, Args&&... args) noexcept(
    meta::nothrow_constructible<T, std::initializer_list<U>, Args...>) {
    base_t::destroy();
    base_t::construct(il, ::ntf::forward<Args>(args)...);
    return base_t::get_value();
  }
#endif

  constexpr void reset() noexcept {
    if (base_t::has_value()) {
      base_t::destroy();
    }
  }

  constexpr bool has_value() const noexcept { return base_t::has_value(); }

  constexpr bool empty() const noexcept { return !has_value(); }

  constexpr explicit operator bool() const noexcept { return has_value(); }

public:
  using impl::OptionalData<T>::operator=;

  constexpr T& operator*() & {
    NTF_ASSERT(base_t::has_value());
    return base_t::get_value();
  }

  constexpr T&& operator*() && {
    NTF_ASSERT(base_t::has_value());
    return ::ntf::move(base_t::get_value());
  }

  constexpr const T& operator*() const& {
    NTF_ASSERT(base_t::has_value());
    return base_t::get_value();
  }

  constexpr const T&& operator*() const&& {
    NTF_ASSERT(base_t::has_value());
    return ::ntf::move(base_t::get_value());
  }

  constexpr T* operator->() {
    NTF_ASSERT(base_t::has_value());
    return ::ntf::addressof(base_t::get_value());
  }

  constexpr const T* operator->() const {
    NTF_ASSERT(base_t::has_value());
    return ::ntf::addressof(base_t::get_value());
  }

public:
  template<typename Fn>
  constexpr auto and_then(Fn&& func) & {
    using U = meta::remove_cvref_t<meta::invoke_result_t<Fn, decltype(base_t::get_value())>>;
    static_assert(meta::optional_type<U>, "F needs to return an optional");
    static_assert(!meta::is_void_v<U>, "Fn can't return void");
    if (has_value()) {
      return ::ntf::forward<Fn>(func)(base_t::get_value());
    } else {
      return U{nullopt};
    }
  }

  template<typename Fn>
  constexpr auto and_then(Fn&& func) && {
    using U =
      meta::remove_cvref_t<meta::invoke_result_t<Fn, decltype(::ntf::move(base_t::get_value()))>>;
    static_assert(meta::optional_type<U>, "F needs to return an optional");
    static_assert(!meta::is_void_v<U>, "Fn can't return void");
    if (has_value()) {
      return ::ntf::forward<Fn>(func)(::ntf::move(base_t::get_value()));
    } else {
      return U{nullopt};
    }
  }

  template<typename F>
  constexpr auto and_then(F&& func) const& {
    using U = meta::remove_cvref_t<meta::invoke_result_t<F, decltype(base_t::get_value())>>;
    static_assert(meta::optional_type<U>, "F needs to return an optional");
    static_assert(!meta::is_void_v<U>, "Fn can't return void");
    if (has_value()) {
      return ::ntf::forward<F>(func)(base_t::get_value());
    } else {
      return U{nullopt};
    }
  }

  template<typename Fn>
  constexpr auto and_then(Fn&& func) const&& {
    using U =
      meta::remove_cvref_t<meta::invoke_result_t<Fn, decltype(::ntf::move(base_t::get_value()))>>;
    static_assert(meta::optional_type<U>, "F needs to return an optional");
    static_assert(!meta::is_void_v<U>, "Fn can't return void");
    if (has_value()) {
      return ::ntf::forward<Fn>(func)(::ntf::move(base_t::get_value()));
    } else {
      return U{nullopt};
    }
  }

  template<typename Fn>
  constexpr auto transform(Fn&& func) & {
    using U = meta::remove_cv_t<meta::invoke_result_t<Fn, decltype(base_t::get_value())>>;
    static_assert(!meta::is_reference_v<U>, "Fn can't return a reference type");
    static_assert(!meta::is_void_v<U>, "Fn can't return void");
    if (has_value()) {
      return Optional<U>{in_place, ::ntf::forward<Fn>(func)(base_t::get_value())};
    } else {
      return Optional<U>{nullopt};
    }
  }

  template<typename Fn>
  constexpr auto transform(Fn&& func) && {
    using U =
      meta::remove_cv_t<meta::invoke_result_t<Fn, decltype(::ntf::move(base_t::get_value()))>>;
    static_assert(!meta::is_reference_v<U>, "Fn can't return a reference type");
    static_assert(!meta::is_void_v<U>, "Fn can't return void");
    if (has_value()) {
      return Optional<U>{in_place, ::ntf::forward<Fn>(func)(::ntf::move(base_t::get_value()))};
    } else {
      return Optional<U>{nullopt};
    }
  }

  template<typename Fn>
  constexpr auto transform(Fn&& func) const& {
    using U = meta::remove_cv_t<meta::invoke_result_t<Fn, decltype(base_t::get_value())>>;
    static_assert(!meta::is_reference_v<U>, "Fn can't return a reference type");
    static_assert(!meta::is_void_v<U>, "Fn can't return void");
    if (has_value()) {
      return Optional<U>{in_place, ::ntf::forward<Fn>(func)(base_t::get_value())};
    } else {
      return Optional<U>{nullopt};
    }
  }

  template<typename Fn>
  constexpr auto transform(Fn&& func) const&& {
    using U =
      meta::remove_cv_t<meta::invoke_result_t<Fn, decltype(::ntf::move(base_t::get_value()))>>;
    static_assert(!meta::is_reference_v<U>, "Fn can't return a reference type");
    static_assert(!meta::is_void_v<U>, "Fn can't return void");
    if (has_value()) {
      return Optional<U>{in_place, ::ntf::forward<Fn>(func)(::ntf::move(base_t::get_value()))};
    } else {
      return Optional<U>{nullopt};
    }
  }

  template<typename Fn>
  constexpr Optional or_else(Fn&& func) const& {
    using U = meta::remove_cvref_t<meta::invoke_result_t<Fn>>;
    static_assert(meta::same_as<U, Optional>, "F needs to return the same optional type");
    return has_value() ? *this : ::ntf::forward<Fn>(func)();
  }

  template<typename Fn>
  constexpr Optional or_else(Fn&& func) && {
    using U = meta::remove_cvref_t<meta::invoke_result_t<Fn>>;
    static_assert(meta::same_as<U, Optional>, "F needs to return the same optional type");
    return has_value() ? ::ntf::move(*this) : ::ntf::forward<Fn>(func)();
  }
};

} // namespace ntf

#endif // NTF_OPTIONAL_HPP_
