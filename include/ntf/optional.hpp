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
                        !meta::is_void_v<T> && !meta::is_rvalue_reference_v<T>;

} // namespace meta

template<typename T>
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
  constexpr OptionalData(const OptionalData<U>& other) noexcept(meta::nothrow_constructible<T, const U&>)
  requires(!meta::same_as<U, meta::remove_cv_t<T>> && meta::convertible_to<T, const U&>)
      : _dummy(), _active{other.has_value()} {
    if (_active) {
      construct(other.get_value());
    }
  }

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
  requires(!meta::same_as<U, meta::remove_cv_t<T>> && meta::convertible_to<T, U&&>)
      : _dummy(), _active{other.has_value()} {
    if (_active) {
      construct(::ntf::move(other.get_value()));
    }
  }

public:
  constexpr OptionalData& operator=(const OptionalData&) noexcept
  requires(meta::trivially_copy_assignable<T> && meta::trivially_copy_constructible<T> &&
           meta::trivially_destructible<T>)
  = default;

  constexpr OptionalData&
  operator=(const OptionalData& other) noexcept(meta::nothrow_copy_assignable<T> &&
                                               meta::nothrow_copy_constructible<T>) {
    if (this == &other) {
      return *this;
    }
    if (has_value()) {
      if (other.has_value()) {
        _value = other.get_value();
      } else {
        destroy();
      }
    } else {
      if (other.has_value()) {
        construct(other.get_value());
      }
    }
    return *this;
  }

  constexpr OptionalData& operator=(OptionalData&&) noexcept
  requires(meta::trivially_move_assignable<T> && meta::trivially_move_constructible<T> &&
           meta::trivially_destructible<T>)
  = default;

  constexpr OptionalData&
  operator=(OptionalData&& other) noexcept(meta::nothrow_move_assignable<T> &&
                                          meta::nothrow_move_constructible<T>) {
    if (this == &other) {
      return *this;
    }
    if (has_value()) {
      if (other.has_value()) {
        _value = ::ntf::move(other.get_value());
      } else {
        destroy();
      }
    } else {
      if (other.has_value()) {
        construct(::ntf::move(other.get_value()));
      }
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

template<typename T>
class Optional : public impl::OptionalData<T> {
private:
  static_assert(meta::nullable_type<T>, "Non nullable type used");
  static_assert(!meta::is_rvalue_reference_v<T>,
                "Optional cannot be instantiated with an rvalue reference type");
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
  requires(meta::copy_constructible<T> && meta::convertible_to<T, U&&>)
  {
    return has_value() ? base_t::get_value() : static_cast<T>(::ntf::forward<U>(def_value));
  }

  template<typename U = meta::remove_cv_t<T>>
  constexpr T value_or(U&& def_value) &&
  requires(meta::move_constructible<T> && meta::convertible_to<T, U&&>) {
    return has_value() ? ::ntf::move(base_t::get_value())
                       : static_cast<T>(::ntf::forward<U>(def_value));
  }

  template<typename... Args>
  constexpr T& emplace(Args&&... args) noexcept(meta::nothrow_constructible<T, Args...>) {
    base_t::destroy();
    base_t::construct(::ntf::forward<Args>(args)...);
    return base_t::get_value();
  }

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

  constexpr Optional& operator=(nullopt_t) noexcept {
    reset();
    return *this;
  }

  template<typename U = T>
  requires(!meta::same_as<meta::remove_cvref_t<U>, Optional> &&
           !meta::same_as<meta::remove_cvref_t<U>, nullopt_t> &&
           meta::constructible_from<T, U>)
  constexpr Optional& operator=(U&& val) {
    if (has_value()) {
      base_t::get_value() = ::ntf::forward<U>(val);
    } else {
      base_t::construct(::ntf::forward<U>(val));
    }
    return *this;
  }

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
    static_assert(meta::optional_type<U>, "Fn needs to return an optional");
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
    static_assert(meta::optional_type<U>, "Fn needs to return an optional");
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
    static_assert(meta::optional_type<U>, "Fn needs to return an optional");
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
    static_assert(meta::optional_type<U>, "Fn needs to return an optional");
    static_assert(!meta::is_void_v<U>, "Fn can't return void");
    if (has_value()) {
      return ::ntf::forward<Fn>(func)(::ntf::move(base_t::get_value()));
    } else {
      return U{nullopt};
    }
  }

  template<typename Fn>
  constexpr auto transform(Fn&& func) & {
    using U = meta::invoke_result_t<Fn, decltype(base_t::get_value())>;
    static_assert(!meta::is_rvalue_reference_v<U>, "Fn can't return an rvalue reference");
    static_assert(!meta::is_void_v<U>, "Fn can't return void");
    if (has_value()) {
      if constexpr (meta::is_lvalue_reference_v<U>) {
        return Optional<U>{in_place, ::ntf::forward<Fn>(func)(base_t::get_value())};
      } else {
        using NonRef = meta::remove_cv_t<U>;
        return Optional<NonRef>{in_place, ::ntf::forward<Fn>(func)(base_t::get_value())};
      }
    } else {
      if constexpr (meta::is_lvalue_reference_v<U>) {
        return Optional<U>{nullopt};
      } else {
        using NonRef = meta::remove_cv_t<U>;
        return Optional<NonRef>{nullopt};
      }
    }
  }

  template<typename Fn>
  constexpr auto transform(Fn&& func) && {
    using U = meta::invoke_result_t<Fn, decltype(::ntf::move(base_t::get_value()))>;
    static_assert(!meta::is_rvalue_reference_v<U>, "Fn can't return an rvalue reference");
    static_assert(!meta::is_void_v<U>, "Fn can't return void");
    if (has_value()) {
      if constexpr (meta::is_lvalue_reference_v<U>) {
        return Optional<U>{in_place, ::ntf::forward<Fn>(func)(::ntf::move(base_t::get_value()))};
      } else {
        using NonRef = meta::remove_cv_t<U>;
        return Optional<NonRef>{in_place, ::ntf::forward<Fn>(func)(::ntf::move(base_t::get_value()))};
      }
    } else {
      if constexpr (meta::is_lvalue_reference_v<U>) {
        return Optional<U>{nullopt};
      } else {
        using NonRef = meta::remove_cv_t<U>;
        return Optional<NonRef>{nullopt};
      }
    }
  }

  template<typename Fn>
  constexpr auto transform(Fn&& func) const& {
    using U = meta::invoke_result_t<Fn, decltype(base_t::get_value())>;
    static_assert(!meta::is_rvalue_reference_v<U>, "Fn can't return an rvalue reference");
    static_assert(!meta::is_void_v<U>, "Fn can't return void");
    if (has_value()) {
      if constexpr (meta::is_lvalue_reference_v<U>) {
        return Optional<U>{in_place, ::ntf::forward<Fn>(func)(base_t::get_value())};
      } else {
        using NonRef = meta::remove_cv_t<U>;
        return Optional<NonRef>{in_place, ::ntf::forward<Fn>(func)(base_t::get_value())};
      }
    } else {
      if constexpr (meta::is_lvalue_reference_v<U>) {
        return Optional<U>{nullopt};
      } else {
        using NonRef = meta::remove_cv_t<U>;
        return Optional<NonRef>{nullopt};
      }
    }
  }

  template<typename Fn>
  constexpr auto transform(Fn&& func) const&& {
    using U = meta::invoke_result_t<Fn, decltype(::ntf::move(base_t::get_value()))>;
    static_assert(!meta::is_rvalue_reference_v<U>, "Fn can't return an rvalue reference");
    static_assert(!meta::is_void_v<U>, "Fn can't return void");
    if (has_value()) {
      if constexpr (meta::is_lvalue_reference_v<U>) {
        return Optional<U>{in_place, ::ntf::forward<Fn>(func)(::ntf::move(base_t::get_value()))};
      } else {
        using NonRef = meta::remove_cv_t<U>;
        return Optional<NonRef>{in_place, ::ntf::forward<Fn>(func)(::ntf::move(base_t::get_value()))};
      }
    } else {
      if constexpr (meta::is_lvalue_reference_v<U>) {
        return Optional<U>{nullopt};
      } else {
        using NonRef = meta::remove_cv_t<U>;
        return Optional<NonRef>{nullopt};
      }
    }
  }

  template<typename Fn>
  constexpr Optional or_else(Fn&& func) const& {
    using U = meta::remove_cvref_t<meta::invoke_result_t<Fn>>;
    static_assert(meta::same_as<U, Optional>, "Fn needs to return the same optional type");
    return has_value() ? *this : ::ntf::forward<Fn>(func)();
  }

  template<typename Fn>
  constexpr Optional or_else(Fn&& func) && {
    using U = meta::remove_cvref_t<meta::invoke_result_t<Fn>>;
    static_assert(meta::same_as<U, Optional>, "Fn needs to return the same optional type");
    return has_value() ? ::ntf::move(*this) : ::ntf::forward<Fn>(func)();
  }
};

template<typename T>
class Optional<T&> {
public:
  using value_type = T&;

public:
  constexpr Optional() noexcept : _ptr(nullptr) {}

  constexpr Optional(nullopt_t) noexcept : _ptr(nullptr) {}

  constexpr Optional(T& ref) noexcept : _ptr(::ntf::addressof(ref)) {}

  template<typename U>
  requires(!meta::same_as<U, T> && meta::convertible_to<T&, U&>)
  constexpr Optional(U& ref) noexcept : _ptr(::ntf::addressof(ref)) {}

  Optional(T&&) = delete;

  template<typename U>
  requires(!meta::is_lvalue_reference_v<U>)
  Optional(U&&) = delete;

  constexpr Optional(in_place_t, T& ref) noexcept : _ptr(::ntf::addressof(ref)) {}

  template<typename U>
  requires(!meta::same_as<U, T> && meta::convertible_to<T&, U&>)
  constexpr Optional(in_place_t, U& ref) noexcept : _ptr(::ntf::addressof(ref)) {}

  constexpr Optional(const Optional& other) noexcept = default;
  constexpr Optional(Optional&& other) noexcept = default;

  template<typename U>
  requires(!meta::same_as<U, T> && meta::convertible_to<T&, U&>)
  constexpr Optional(const Optional<U&>& other) noexcept :
      _ptr(other.has_value() ? ::ntf::addressof(other.value()) : nullptr) {}

  ~Optional() noexcept = default;

public:
  constexpr Optional& operator=(nullopt_t) noexcept {
    _ptr = nullptr;
    return *this;
  }

  constexpr Optional& operator=(const Optional& other) noexcept = default;
  constexpr Optional& operator=(Optional&& other) noexcept = default;

  template<typename U>
  requires(!meta::same_as<U, T> && meta::convertible_to<T&, U&>)
  constexpr Optional& operator=(const Optional<U&>& other) noexcept {
    _ptr = other.has_value() ? ::ntf::addressof(other.value()) : nullptr;
    return *this;
  }

  constexpr Optional& operator=(T& ref) noexcept {
    _ptr = ::ntf::addressof(ref);
    return *this;
  }

  template<typename U>
  requires(!meta::same_as<U, T> && meta::convertible_to<T&, U&>)
  constexpr Optional& operator=(U& ref) noexcept {
    _ptr = ::ntf::addressof(ref);
    return *this;
  }

  Optional& operator=(T&&) = delete;

  template<typename U>
  requires(!meta::is_lvalue_reference_v<U>)
  Optional& operator=(U&&) = delete;

public:
  constexpr bool has_value() const noexcept { return _ptr != nullptr; }

  constexpr bool empty() const noexcept { return _ptr == nullptr; }

  constexpr explicit operator bool() const noexcept { return _ptr != nullptr; }

  constexpr T& value() const {
    NTF_THROW_IF(!has_value(), BadOptionalAccess());
    return *_ptr;
  }

  template<typename U>
  constexpr T& value_or(U& def_value) const
  requires(meta::convertible_to<T&, U&>) {
    return has_value() ? *_ptr : static_cast<T&>(def_value);
  }

  template<typename U>
  requires(!meta::is_lvalue_reference_v<U> && meta::convertible_to<meta::remove_cv_t<T>, U>)
  constexpr meta::remove_cv_t<T> value_or(U&& def_value) const {
    return has_value() ? *_ptr : static_cast<meta::remove_cv_t<T>>(::ntf::forward<U>(def_value));
  }

  constexpr T& emplace(T& ref) noexcept {
    _ptr = ::ntf::addressof(ref);
    return *_ptr;
  }

  template<typename U>
  requires(!meta::same_as<U, T> && meta::convertible_to<T&, U&>)
  constexpr T& emplace(U& ref) noexcept {
    _ptr = ::ntf::addressof(ref);
    return *_ptr;
  }

  constexpr void reset() noexcept { _ptr = nullptr; }

public:
  constexpr T& operator*() const {
    NTF_ASSERT(has_value());
    return *_ptr;
  }

  constexpr T* operator->() const {
    NTF_ASSERT(has_value());
    return _ptr;
  }

public:
  template<typename Fn>
  constexpr auto and_then(Fn&& func) const {
    using U = meta::remove_cvref_t<meta::invoke_result_t<Fn, T&>>;
    static_assert(meta::optional_type<U>, "Fn needs to return an optional");
    if (has_value()) {
      return ::ntf::forward<Fn>(func)(*_ptr);
    } else {
      return U{nullopt};
    }
  }

  template<typename Fn>
  constexpr auto transform(Fn&& func) const {
    using U = meta::invoke_result_t<Fn, T&>;
    static_assert(!meta::is_void_v<U>, "Fn can't return void");
    static_assert(!meta::is_rvalue_reference_v<U>, "Fn can't return an rvalue reference");
    if (has_value()) {
      if constexpr (meta::is_lvalue_reference_v<U>) {
        return Optional<U>{in_place, ::ntf::forward<Fn>(func)(*_ptr)};
      } else {
        using NonRef = meta::remove_cv_t<U>;
        return Optional<NonRef>{in_place, ::ntf::forward<Fn>(func)(*_ptr)};
      }
    } else {
      if constexpr (meta::is_lvalue_reference_v<U>) {
        return Optional<U>{nullopt};
      } else {
        using NonRef = meta::remove_cv_t<U>;
        return Optional<NonRef>{nullopt};
      }
    }
  }

  template<typename Fn>
  constexpr Optional or_else(Fn&& func) const {
    using U = meta::remove_cvref_t<meta::invoke_result_t<Fn>>;
    static_assert(meta::same_as<U, Optional>, "Fn needs to return the same optional type");
    return has_value() ? *this : ::ntf::forward<Fn>(func)();
  }

private:
  T* _ptr = nullptr;
};

template<typename T>
class Optional<T&&> {
  static_assert(!meta::is_rvalue_reference_v<T&&>,
                "Optional cannot be instantiated with an rvalue reference type");
};

template<typename T, typename U>
constexpr bool operator==(const Optional<T>& lhs, const Optional<U>& rhs) {
  if (lhs.has_value() != rhs.has_value()) {
    return false;
  }
  if (!lhs.has_value()) {
    return true;
  }
  return *lhs == *rhs;
}

template<typename T>
constexpr bool operator==(const Optional<T>& opt, nullopt_t) noexcept {
  return !opt.has_value();
}

template<typename T>
constexpr bool operator==(nullopt_t, const Optional<T>& opt) noexcept {
  return !opt.has_value();
}

template<typename T, typename U>
constexpr bool operator==(const Optional<T>& opt, const U& val) {
  return opt.has_value() ? *opt == val : false;
}

template<typename T, typename U>
constexpr bool operator==(const U& val, const Optional<T>& opt) {
  return opt.has_value() ? val == *opt : false;
}

} // namespace ntf

#endif // NTF_OPTIONAL_HPP_
