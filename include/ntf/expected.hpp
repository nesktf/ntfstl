#ifndef NTF_EXPECTED_HPP_
#define NTF_EXPECTED_HPP_

#include <ntf/memory.hpp>

namespace ntf {

template<typename>
class BadExpectedAccess;

template<>
class BadExpectedAccess<void> : public Exception {
public:
  const char* what() const noexcept override { return "BadExpectedAccess"; }
};

template<typename E>
class BadExpectedAccess final : public BadExpectedAccess<void> {
public:
  explicit BadExpectedAccess(const E& err) : _err(err) {}

  explicit BadExpectedAccess(E&& err) : _err(::ntf::move(err)) {}

public:
  const char* what() const noexcept override {
    if constexpr (meta::exception_msg_type<E>) {
      return _err.what();
    } else {
      return BadExpectedAccess<void>::what();
    }
  }

  E& error() & noexcept { return _err; }

  const E& error() const& noexcept { return _err; }

  E&& error() && noexcept { return ::ntf::move(_err); }

  const E&& error() const&& noexcept { return ::ntf::move(_err); }

private:
  E _err;
};

template<typename E>
class unexpected {
public:
  static_assert(!meta::is_void_v<E>, "E can't be void");

public:
  unexpected() = delete;

  constexpr unexpected(E&& err) noexcept(meta::nothrow_move_constructible<E>) :
      _err(::ntf::move(err)) {}

  constexpr unexpected(const E& err) noexcept(meta::nothrow_copy_constructible<E>) : _err(err) {}

  template<typename... Args>
  constexpr explicit unexpected(Args&&... args) noexcept(meta::nothrow_constructible<E, Args...>) :
      _err(::ntf::forward<Args>(args)...) {}

#if 0
  template<typename U, typename... Args>
  constexpr explicit unexpected(std::initializer_list<U> list, Args&&... args) noexcept(
    meta::nothrow_constructible<E, std::initializer_list<U>, Args...>) :
      _err(list, ::ntf::forward<Args>(args)...) {}
#endif

public:
  constexpr E& error() & noexcept { return _err; };

  constexpr const E& error() const& noexcept { return _err; }

  constexpr E&& error() && noexcept { return ::ntf::move(_err); };

  constexpr const E&& error() const&& noexcept { return ::ntf::move(_err); }

private:
  E _err;
};

template<typename E, typename G>
constexpr bool operator==(const unexpected<E>& lhs, const unexpected<G>& rhs) {
  return lhs.error() == rhs.error();
}

template<typename E, typename G>
constexpr bool operator!=(const unexpected<E>& lhs, const unexpected<G>& rhs) {
  return lhs.error() != rhs.error();
}

template<typename E, typename G>
constexpr bool operator<=(const unexpected<E>& lhs, const unexpected<G>& rhs) {
  return lhs.error() <= rhs.error();
}

template<typename E, typename G>
constexpr bool operator>=(const unexpected<E>& lhs, const unexpected<G>& rhs) {
  return lhs.error() >= rhs.error();
}

template<typename E, typename G>
constexpr bool operator<(const unexpected<E>& lhs, const unexpected<G>& rhs) {
  return lhs.error() < rhs.error();
}

template<typename E, typename G>
constexpr bool operator>(const unexpected<E>& lhs, const unexpected<G>& rhs) {
  return lhs.error() > rhs.error();
}

template<typename E>
unexpected(E) -> unexpected<E>;

template<typename E>
unexpected<typename meta::decay_t<E>> make_unexpected(E&& e) {
  return unexpected<typename meta::decay_t<E>>{::ntf::forward<E>(e)};
}

struct unexpect_t {};

constexpr inline unexpect_t unexpect;

namespace impl {

template<bool Valid, typename T, typename... Args>
constexpr void rebind_nullable(T& obj,
                               Args&&... args) noexcept(meta::nothrow_constructible<T, Args...>) {
  static_assert(meta::nothrow_destructible<T>, "T has to be nothrow destructible");
  if constexpr (Valid) {
    // If is obj a constructed object
    if constexpr (meta::nothrow_constructible<T, Args...>) {
      ::ntf::addressof(obj)->~T();
      NTF_PNEW(::ntf::addressof(obj)) T(::ntf::forward<Args>(args)...);
    } else {
      T old(::ntf::move(obj)); // Might throw
      ::ntf::addressof(obj)->~T();
#ifdef __cpp_exceptions
      try {
#endif
        NTF_PNEW(::ntf::addressof(obj)) T(::ntf::forward<Args>(args)...);
#ifdef __cpp_exceptions
      } catch (...) {
        NTF_PNEW(::ntf::addressof(obj)) T(::ntf::move(old));
        throw;
      }
#endif
    }
  } else {
    NTF_PNEW(::ntf::addressof(obj)) T(::ntf::forward<Args>(args)...); // Might throw
  }
}

template<bool valid, typename T, typename E, typename... Args>
constexpr void expect_rebind_error(T& val, E& err, Args&&... args) noexcept(
  meta::nothrow_constructible<T, Args...> || meta::nothrow_move_constructible<T>) {
  static_assert(meta::nothrow_destructible<E>, "E has to be nothrow destructible");
  static_assert(meta::nothrow_destructible<T>, "T has to be nothrow destructible");
  if constexpr (valid) {
    // If is val a constructed object
    if constexpr (meta::nothrow_constructible<E, Args...>) {
      ::ntf::addressof(val)->~T();
      NTF_PNEW(::ntf::addressof(err)) E(::ntf::forward<Args>(args)...);
    } else if constexpr (meta::nothrow_move_constructible<E>) {
      E new_err(::ntf::forward<Args>(args)...); // Might throw
      ::ntf::addressof(val)->~T();
      NTF_PNEW(::ntf::addressof(err)) E(::ntf::move(new_err));
    } else {
      T old_val(::ntf::move(val)); // Might or might not throw
      ::ntf::addressof(val)->~T();
#ifdef __cpp_exceptions
      try {
#endif
        NTF_PNEW(::ntf::addressof(err)) E(::ntf::forward<Args>(args)...);
#ifdef __cpp_exceptions
      } catch (...) {
        NTF_PNEW(::ntf::addressof(val)) T(::ntf::move(old_val));
        throw;
      }
#endif
    }
  } else {
    // If is err a constructed object
    if constexpr (meta::nothrow_constructible<E, Args...>) {
      ::ntf::addressof(err)->~E();
      NTF_PNEW(::ntf::addressof(err)) E(::ntf::forward<Args>(args)...);
    } else if constexpr (meta::nothrow_move_constructible<E>) {
      E new_err(::ntf::forward<Args>(args)...); // Might throw
      ::ntf::addressof(err)->~E();
      NTF_PNEW(::ntf::addressof(err)) E(::ntf::move(new_err));
    } else {
      E old_err(::ntf::move(err)); // Might throw
      ::ntf::addressof(err)->~E();
#ifdef __cpp_exceptions
      try {
#endif
        NTF_PNEW(::ntf::addressof(err)) E(::ntf::forward<Args>(args)...);
#ifdef __cpp_exceptions
      } catch (...) {
        NTF_PNEW(::ntf::addressof(err)) E(::ntf::move(old_err));
        throw;
      }
#endif
    }
  }
}

template<bool valid, typename T, typename E, typename... Args>
constexpr void expect_rebind_value(T& val, E& err, Args&&... args) noexcept(
  meta::nothrow_constructible<T, Args...> || meta::nothrow_move_constructible<T>) {
  static_assert(meta::nothrow_destructible<E>, "E has to be nothrow destructible");
  static_assert(meta::nothrow_destructible<T>, "T has to be nothrow destructible");

  if constexpr (valid) {
    // If is val a constructed object
    if constexpr (meta::nothrow_constructible<T, Args...>) {
      ::ntf::addressof(val)->~T();
      NTF_PNEW(::ntf::addressof(val)) T(::ntf::forward<Args>(args)...);
    } else if constexpr (meta::nothrow_move_constructible<T>) {
      T new_val(::ntf::forward<Args>(args)...); // Might throw
      ::ntf::addressof(val)->~T();
      NTF_PNEW(::ntf::addressof(val)) T(::ntf::move(new_val));
    } else {
      T old_val(::ntf::move(val)); // Might throw
      ::ntf::addressof(val)->~T();
#ifdef __cpp_exceptions
      try {
#endif
        NTF_PNEW(::ntf::addressof(val)) T(::ntf::forward<Args>(args)...);
#ifdef __cpp_exceptions
      } catch (...) {
        NTF_PNEW(::ntf::addressof(val)) T(::ntf::move(old_val));
        throw;
      }
#endif
    }
  } else {
    // If is err a constructed object
    if constexpr (meta::nothrow_constructible<T, Args...>) {
      ::ntf::addressof(err)->~E();
      NTF_PNEW(::ntf::addressof(val)) T(::ntf::forward<Args>(args)...);
    } else if constexpr (meta::nothrow_move_constructible<T>) {
      T new_val(::ntf::forward<Args>(args)...); // Might throw
      ::ntf::addressof(err)->~E();
      NTF_PNEW(::ntf::addressof(val)) T(::ntf::move(new_val));
    } else {
      E old_err(::ntf::move(err)); // Might or might not throw
      ::ntf::addressof(err)->~E();
#ifdef __cpp_exceptions
      try {
#endif
        NTF_PNEW(::ntf::addressof(val)) T(::ntf::forward<Args>(args)...);
#ifdef __cpp_exceptions
      } catch (...) {
        NTF_PNEW(::ntf::addressof(err)) E(::ntf::move(old_err));
        throw;
      }
#endif
    }
  }
}

template<typename T, typename E>
class ExpectedStorage {
private:
  static constexpr bool _triv_destr_val = meta::trivially_destructible<T>;
  static constexpr bool _triv_destr_err = meta::trivially_destructible<E>;

  static constexpr bool _triv_movec =
    meta::trivially_move_constructible<T> && meta::trivially_move_constructible<E>;

  static constexpr bool _triv_copyc =
    meta::trivially_copy_constructible<T> && meta::trivially_copy_constructible<E>;

public:
  constexpr ExpectedStorage() noexcept(meta::nothrow_default_constructible<T>)
  requires(meta::default_constructible<T>)
      : _value(), _valid(true) {}

  template<typename... Args>
  constexpr ExpectedStorage(in_place_t,
                            Args&&... arg) noexcept(meta::nothrow_constructible<T, Args...>) :
      _value(::ntf::forward<Args>(arg)...), _valid(true) {}

#if 0
  template<typename U, typename... Args>
  constexpr ExpectedStorage(in_place_t, std::initializer_list<U> il, Args&&... arg) noexcept(
    meta::nothrow_constructible<T, std::initializer_list<U>, Args...>) :
      _value(il, ::ntf::forward<Args>(arg)...), _valid(true) {}
#endif

  template<typename... Args>
  constexpr ExpectedStorage(unexpect_t,
                            Args&&... arg) noexcept(meta::nothrow_constructible<E, Args...>) :
      _error(::ntf::forward<Args>(arg)...), _valid(false) {}

#if 0
  template<typename U, typename... Args>
  constexpr ExpectedStorage(unexpect_t, std::initializer_list<U> il, Args&&... arg) noexcept(
    meta::nothrow_constructible_v<E, std::initializer_list<U>, Args...>) :
      _error(il, ::ntf::forward<Args>(arg)...), _valid(false) {}
#endif

  constexpr ExpectedStorage(T&& val) noexcept(meta::nothrow_move_constructible<T>) :
      _value(::ntf::move(val)), _valid(true) {}

  constexpr ExpectedStorage(const T& val) noexcept(meta::nothrow_copy_constructible<T>) :
      _value(val), _valid(true) {}

  template<typename U = meta::remove_cv_t<T>>
  constexpr explicit(!meta::convertible_to<U, T>)
    ExpectedStorage(U&& val) noexcept(meta::nothrow_constructible<T, U>) :
      _value(::ntf::forward<U>(val)), _valid(true) {}

  template<typename G>
  constexpr explicit(!meta::convertible_to<const G&, E>)
    ExpectedStorage(const unexpected<G>& unex) noexcept(meta::nothrow_constructible<E, const G&>) :
      _error(unex.error()), _valid(false) {}

  template<typename G>
  constexpr explicit(!meta::convertible_to<G, E>)
    ExpectedStorage(unexpected<G>&& unex) noexcept(meta::nothrow_constructible<E, G>) :
      _error(::ntf::move(unex).error()), _valid(false) {}

public:
  constexpr ExpectedStorage(ExpectedStorage&& other) noexcept
  requires(_triv_movec)
  = default;

  constexpr ExpectedStorage(ExpectedStorage&& other) noexcept(
    meta::nothrow_move_constructible<T> && meta::nothrow_move_constructible<E>)
  requires(meta::move_constructible<T> && meta::move_constructible<E> && !_triv_movec)
      : _valid(::ntf::move(other._valid)) {
    if (other.has_value()) {
      new (::ntf::addressof(_value)) T(::ntf::move(other.get()));
    } else {
      new (::ntf::addressof(_error)) E(::ntf::move(other.get_error()));
    }
  }

public:
  constexpr ExpectedStorage(const ExpectedStorage& other) noexcept
  requires(_triv_copyc)
  = default;

  constexpr ExpectedStorage(const ExpectedStorage& other) noexcept(
    meta::nothrow_copy_constructible<T> && meta::nothrow_copy_constructible<E>)
  requires(meta::copy_constructible<T> && meta::copy_constructible<E> && !_triv_copyc)
      : _valid(other._valid) {
    if (other.has_value()) {
      new (::ntf::addressof(_value)) T(other.get());
    } else {
      new (::ntf::addressof(_error)) E(other.get_error());
    }
  }

public:
  constexpr ExpectedStorage&
  operator=(ExpectedStorage& other) noexcept(meta::nothrow_move_constructible<T> &&
                                             meta::nothrow_move_constructible<E>)
  requires(meta::move_constructible<T> && meta::move_constructible<E>)
  {
    if (::ntf::addressof(other) == this) {
      return *this;
    }

    if (has_value()) {
      if (other.has_value()) {
        expect_rebind_value<true>(_value, _error, ::ntf::move(other.get()));
      } else {
        expect_rebind_error<true>(_value, _error, ::ntf::move(other.get_error()));
      }
    } else {
      if (other.has_value()) {
        expect_rebind_value<false>(_value, _error, ::ntf::move(other.get()));
      } else {
        expect_rebind_error<false>(_value, _error, ::ntf::move(other.get_error()));
      }
    }
    _valid = ::ntf::move(other._valid);
    return *this;
  }

public:
  constexpr ExpectedStorage&
  operator=(const ExpectedStorage& other) noexcept(meta::nothrow_copy_constructible<T> &&
                                                   meta::nothrow_copy_constructible<E>)
  requires(meta::copy_constructible<T> && meta::copy_constructible<E>)
  {
    if (::ntf::addressof(other) == this) {
      return *this;
    }

    if (has_value()) {
      if (other.has_value()) {
        expect_rebind_value<true>(_value, _error, other.get());
      } else {
        expect_rebind_error<true>(_value, _error, other.get_error());
      }
    } else {
      if (other.has_value()) {
        expect_rebind_value<false>(_value, _error, other.get());
      } else {
        expect_rebind_error<false>(_value, _error, other.get_error());
      }
    }
    _valid = other._valid;
    return *this;
  }

public:
  constexpr ~ExpectedStorage() noexcept
  requires(_triv_destr_val && _triv_destr_err)
  = default;

  constexpr ~ExpectedStorage() noexcept
  requires(!_triv_destr_val || !_triv_destr_err)
  {
    static_assert(meta::nothrow_destructible<E>, "E has to be nothrow destructible");
    static_assert(meta::nothrow_destructible<T>, "T has to be nothrow destructible");
    if constexpr (_triv_destr_val && !_triv_destr_err) {
      if (!_valid) {
        ::ntf::destroy_at(::ntf::addressof(_error));
      }
    } else if constexpr (!_triv_destr_val && _triv_destr_err) {
      if (_valid) {
        ::ntf::destroy_at(::ntf::addressof(_value));
      }
    } else {
      if (_valid) {
        ::ntf::destroy_at(::ntf::addressof(_value));
      } else {
        ::ntf::destroy_at(::ntf::addressof(_error));
      }
    }
  }

public:
  template<typename... Args>
  constexpr T& emplace(Args&&... args) noexcept(meta::nothrow_constructible<T, Args...> ||
                                                meta::nothrow_move_constructible<T>) {
    if (has_value()) {
      expect_rebind_value<true>(_value, _error, ::ntf::forward<Args>(args)...);
    } else {
      expect_rebind_value<false>(_value, _error, ::ntf::forward<Args>(args)...);
      _valid = true;
    }
    return _value;
  }

#if 0
  template<typename U, typename... Args>
  constexpr T& emplace(std::initializer_list<U> il, Args&&... args) noexcept(
    meta::nothrow_constructible<T, std::initializer_list<U>, Args...> ||
    meta::nothrow_move_constructible<T>) {
    if (has_value()) {
      expect_rebind_value<true>(_value, _error, il, ::ntf::forward<Args>(args)...);
    } else {
      expect_rebind_value<false>(_value, _error, il, ::ntf::forward<Args>(args)...);
      _valid = true;
    }
    return _value;
  }
#endif

  template<typename... Args>
  constexpr E& emplace_error(Args&&... args) noexcept(meta::nothrow_constructible<E, Args...> ||
                                                      meta::nothrow_move_constructible<E>) {
    if (has_value()) {
      expect_rebind_error<true>(_value, _error, ::ntf::forward<Args>(args)...);
      _valid = false;
    } else {
      expect_rebind_error<false>(_value, _error, ::ntf::forward<Args>(args)...);
    }
    return _error;
  }

#if 0
  template<typename U, typename... Args>
  constexpr E& emplace_error(std::initializer_list<U> il, Args&&... args) noexcept(
    meta::nothrow_constructible<E, std::initializer_list<U>, Args...> ||
    meta::nothrow_move_constructible_v<E>) {
    if (has_value()) {
      expect_rebind_error<true>(_value, _error, il, ::ntf::forward<Args>(args)...);
      _valid = false;
    } else {
      expect_rebind_error<false>(_value, _error, il, ::ntf::forward<Args>(args)...);
    }
    return _error;
  }
#endif

public:
  constexpr bool has_value() const noexcept { return _valid; }

  constexpr bool has_error() const noexcept { return !_valid; }

protected:
  constexpr T& get() & noexcept { return _value; }

  constexpr T&& get() && noexcept { return ::ntf::move(_value); }

  constexpr const T& get() const& noexcept { return _value; }

  constexpr const T&& get() const&& noexcept { return ::ntf::move(_value); }

  constexpr E& get_error() & noexcept { return _error; }

  constexpr E&& get_error() && noexcept { return ::ntf::move(_error); }

  constexpr const E& get_error() const& noexcept { return _error; }

  constexpr const E&& get_error() const&& noexcept { return ::ntf::move(_error); }

private:
  union {
    T _value;
    E _error;
  };

  bool _valid;
};

template<typename E>
class ExpectedStorage<void, E> {
private:
  static constexpr bool _triv_destr = meta::trivially_destructible<E>;
  static constexpr bool _triv_movec = meta::trivially_move_constructible<E>;
  static constexpr bool _triv_copyc = meta::trivially_copy_constructible<E>;

public:
  constexpr ExpectedStorage() noexcept : _valid(true) {}

  constexpr ExpectedStorage(in_place_t) noexcept : _valid(true) {}

  template<typename... Args>
  constexpr ExpectedStorage(unexpect_t,
                            Args&&... arg) noexcept(meta::nothrow_constructible<E, Args...>) :
      _error(::ntf::forward<Args>(arg)...), _valid(false) {}

#if 0
  template<typename U, typename... Args>
  constexpr ExpectedStorage(unexpect_t, std::initializer_list<U> l, Args&&... arg) noexcept(
    meta::nothrow_constructible<E, std::initializer_list<U>, Args...>) :
      _error(l, ::ntf::forward<Args>(arg)...), _valid(false) {}
#endif

  template<typename G>
  constexpr explicit(!meta::convertible_to<const G&, E>)
    ExpectedStorage(const unexpected<G>& unex) noexcept(meta::nothrow_constructible<E, const G&>) :
      _error(unex.error()), _valid(false) {}

  template<typename G>
  constexpr explicit(!meta::convertible_to<G, E>)
    ExpectedStorage(unexpected<G>&& unex) noexcept(meta::nothrow_constructible<E, G>) :
      _error(::ntf::move(unex).error()), _valid(false) {}

public:
  constexpr ExpectedStorage(ExpectedStorage&& other) noexcept
  requires(_triv_movec)
  = default;

  constexpr ExpectedStorage(ExpectedStorage&& other) noexcept(meta::nothrow_move_constructible<E>)
  requires(meta::move_constructible<E> && !_triv_movec)
      : _valid(::ntf::move(other._valid)) {
    if (other.has_error()) {
      new (::ntf::addressof(_error)) E(::ntf::move(other.get_error()));
    }
  }

public:
  constexpr ExpectedStorage(const ExpectedStorage& other) noexcept
  requires(_triv_copyc)
  = default;

  constexpr ExpectedStorage(const ExpectedStorage& other) noexcept(
    meta::nothrow_copy_constructible<E>)
  requires(meta::copy_constructible<E> && !_triv_copyc)
      : _valid(other._valid) {
    if (other.has_error()) {
      new (::ntf::addressof(_error)) E(other.get_error());
    }
  }

public:
  constexpr ExpectedStorage&
  operator=(ExpectedStorage&& other) noexcept(meta::nothrow_move_constructible<E>)
  requires(meta::move_constructible<E>)
  {
    if (has_value()) {
      if (other.has_error()) {
        rebind_nullable<false>(_error, ::ntf::move(other.get_error()));
      }
    } else {
      if (other.has_error()) {
        rebind_nullable<true>(_error, ::ntf::move(other.get_error()));
      } else {
        ::ntf::addressof(_error)->~E();
      }
    }
    _valid = ::ntf::move(other._valid);
    return *this;
  }

public:
  constexpr ExpectedStorage&
  operator=(const ExpectedStorage& other) noexcept(meta::nothrow_copy_constructible<E>)
  requires(meta::copy_constructible<E>)
  {
    if (has_value()) {
      if (other.has_error()) {
        rebind_nullable<false>(_error, other.get_error());
      }
    } else {
      if (other.has_error()) {
        rebind_nullable<true>(_error, other.get_error());
      } else {
        ::ntf::addressof(_error)->~E();
      }
    }
    _valid = other._valid;
    return *this;
  }

public:
  constexpr ~ExpectedStorage() noexcept
  requires(_triv_destr)
  = default;

  constexpr ~ExpectedStorage() noexcept(meta::nothrow_destructible<E>)
  requires(!_triv_destr)
  {
    if (has_error()) {
      ::ntf::addressof(_error)->~E();
    }
  }

public:
  constexpr void emplace() noexcept {
    if (has_value()) {
      return;
    }
    ::ntf::addressof(_error)->~E();
    _valid = true;
  }

  template<typename... Args>
  constexpr E& emplace_error(Args&&... args) noexcept(meta::nothrow_constructible<E, Args...>) {
    if (has_value()) {
      rebind_nullable<false>(_error, ::ntf::forward<Args>(args)...);
      _valid = false;
    } else {
      rebind_nullable<true>(_error, ::ntf::forward<Args>(args)...);
    }
    return _error;
  }

#if 0
  template<typename U, typename... Args>
  constexpr E& emplace_error(std::initializer_list<U> il, Args&&... args) noexcept(
    meta::nothrow_constructible<E, std::initializer_list<U>, Args...>) {
    if (has_value()) {
      rebind_nullable<false>(_error, il, ::ntf::forward<Args>(args)...);
      _valid = false;
    } else {
      rebind_nullable<true>(_error, il, ::ntf::forward<Args>(args)...);
    }
    return _error;
  }
#endif

  constexpr bool has_value() const noexcept { return _valid; }

  constexpr bool has_error() const noexcept { return !_valid; }

protected:
  void get() const noexcept {}

  constexpr E& get_error() & noexcept { return _error; }

  constexpr E&& get_error() && noexcept { return ::ntf::move(_error); }

  constexpr const E& get_error() const& noexcept { return _error; }

  constexpr const E&& get_error() const&& noexcept { return ::ntf::move(_error); }

private:
  union {
    char _dummy;
    E _error;
  };

  bool _valid;
};

} // namespace impl

template<typename T, typename E>
class Expected;

namespace meta {

template<typename>
struct expected_check : public false_type {};

template<typename T, typename E>
struct expected_check<Expected<T, E>> : public true_type {};

template<typename T>
constexpr bool expected_check_v = expected_check<T>::value;

template<typename T>
concept expected_type = expected_check_v<T>;

template<typename Exp, typename T>
struct expected_check_t : public false_type {};

template<typename T>
struct expected_check_t<Expected<T, void>, T> : public false_type {};

template<typename T, typename E>
struct expected_check_t<Expected<T, E>, T> : public true_type {};

template<typename Exp, typename T>
constexpr bool expected_check_t_v = expected_check_t<Exp, T>::value;

template<typename Exp, typename T>
concept expected_with_type = expected_check_t_v<Exp, T>;

template<typename Exp, typename E>
struct expected_check_e : public false_type {};

template<typename T>
struct expected_check_e<Expected<T, void>, void> : public false_type {};

template<typename T, typename E>
struct expected_check_e<Expected<T, E>, E> : public true_type {};

template<typename Exp, typename E>
constexpr bool expected_check_e_v = expected_check_e<Exp, E>::value;

template<typename Exp, typename E>
concept expected_with_error = expected_check_e_v<Exp, E>;

template<bool cond, typename T, typename Err>
struct expected_wrap_if {
  using type = T;
};

template<typename T, typename Err>
struct expected_wrap_if<true, T, Err> {
  using type = Expected<T, Err>;
};

} // namespace meta

namespace impl {

template<typename Fn, typename T>
struct expect_monadic_chain {
  using type = meta::remove_cvref_t<meta::invoke_result_t<Fn, T>>;
};

template<typename Fn>
struct expect_monadic_chain<Fn, void> {
  using type = meta::remove_cvref_t<meta::invoke_result_t<Fn>>;
};

template<typename Fn, typename T>
using expect_monadic_chain_t = expect_monadic_chain<Fn, T>::type;

template<typename Fn, typename T>
struct expect_monadic_transform {
  using type = meta::remove_cv_t<meta::invoke_result_t<Fn, T>>;
};

template<typename Fn>
struct expect_monadic_transform<Fn, void> {
  using type = meta::remove_cv_t<meta::invoke_result_t<Fn>>;
};

template<typename Fn, typename T>
using expect_monadic_transform_t = typename expect_monadic_transform<Fn, T>::type;

template<typename T, typename E>
class ExpectedMonadicOps : public impl::ExpectedStorage<T, E> {
public:
  using impl::ExpectedStorage<T, E>::ExpectedStorage;

public:
  template<typename Fn>
  constexpr auto and_then(Fn&& func) & {
    if constexpr (meta::is_void_v<T>) {
      using U = impl::expect_monadic_chain_t<Fn, void>;
      static_assert(meta::expected_with_error<U, E>,
                    "Fn needs to return an expected with error E");
      if (this->has_value()) {
        return ::ntf::forward<Fn>(func)();
      } else {
        return U{unexpect, this->get_error()};
      }
    } else {
      using U = impl::expect_monadic_chain_t<Fn, decltype(this->get())>;
      static_assert(meta::expected_with_error<U, E>,
                    "Fn needs to return an expected with error E");
      if (this->has_value()) {
        return ::ntf::forward<Fn>(func)(this->get());
      } else {
        return U{unexpect, this->get_error()};
      }
    }
  }

  template<typename Fn>
  constexpr auto and_then(Fn&& func) && {
    if constexpr (meta::is_void_v<T>) {
      using U = impl::expect_monadic_chain_t<Fn, void>;
      static_assert(meta::expected_with_error<U, E>,
                    "Fn needs to return an expected with error E");
      if (this->has_value()) {
        return ::ntf::forward<Fn>(func)();
      } else {
        return U{unexpect, ::ntf::move(this->get_error())};
      }
    } else {
      using U = impl::expect_monadic_chain_t<Fn, decltype(::ntf::move(this->get()))>;
      static_assert(meta::expected_with_error<U, E>,
                    "Fn needs to return an expected with error E");
      if (this->has_value()) {
        return ::ntf::forward<Fn>(func)(::ntf::move(this->get()));
      } else {
        return U{unexpect, ::ntf::move(this->get_error())};
      }
    }
  }

  template<typename Fn>
  constexpr auto and_then(Fn&& func) const& {
    if constexpr (meta::is_void_v<T>) {
      using U = impl::expect_monadic_chain_t<Fn, void>;
      static_assert(meta::expected_with_error<U, E>,
                    "Fn needs to return an expected with error E");
      if (this->has_value()) {
        return ::ntf::forward<Fn>(func)();
      } else {
        return U{unexpect, this->get_error()};
      }
    } else {
      using U = impl::expect_monadic_chain_t<Fn, decltype(this->get())>;
      static_assert(meta::expected_with_error<U, E>,
                    "Fn needs to return an expected with error E");
      if (this->has_value()) {
        return ::ntf::forward<Fn>(func)(this->get());
      } else {
        return U{unexpect, this->get_error()};
      }
    }
  }

  template<typename Fn>
  constexpr auto and_then(Fn&& func) const&& {
    if constexpr (meta::is_void_v<T>) {
      using U = impl::expect_monadic_chain_t<Fn, void>;
      static_assert(meta::expected_with_error<U, E>,
                    "Fn needs to return an expected with error E");
      if (this->has_value()) {
        return ::ntf::forward<Fn>(func)();
      } else {
        return U{unexpect, ::ntf::move(this->get_error())};
      }
    } else {
      using U = impl::expect_monadic_chain_t<Fn, decltype(::ntf::move(this->get()))>;
      static_assert(meta::expected_with_error<U, E>,
                    "Fn needs to return an expected with error E");
      if (this->has_value()) {
        return ::ntf::forward<Fn>(func)(::ntf::move(this->get()));
      } else {
        return U{unexpect, ::ntf::move(this->get_error())};
      }
    }
  }

public:
  template<typename Fn>
  constexpr auto or_else(Fn&& func) & {
    if (this->has_value()) {
      if constexpr (meta::is_void_v<T>) {
        using G = impl::expect_monadic_chain_t<Fn, void>;
        static_assert(meta::expected_with_type<G, T>,
                      "Fn needs to return an expected with value T");
        return G{in_place};
      } else {
        using G = impl::expect_monadic_chain_t<Fn, decltype(this->get_error())>;
        static_assert(meta::expected_with_type<G, T>,
                      "Fn needs to return an expected with value T");
        return G{in_place, this->get()};
      }
    } else {
      return ::ntf::forward<Fn>(func)(this->get_error());
    }
  }

  template<typename Fn>
  constexpr auto or_else(Fn&& func) && {
    if (this->has_value()) {
      if constexpr (meta::is_void_v<T>) {
        using G = impl::expect_monadic_chain_t<Fn, void>;
        static_assert(meta::expected_with_type<G, T>,
                      "Fn needs to return an expected with value T");
        return G{in_place};
      } else {
        using G = impl::expect_monadic_chain_t<Fn, decltype(::ntf::move(this->get_error()))>;
        static_assert(meta::expected_with_type<G, T>,
                      "Fn needs to return an expected with value T");
        return G{in_place, ::ntf::move(this->get())};
      }
    } else {
      return ::ntf::forward<Fn>(func)(::ntf::move(this->get_error()));
    }
  }

  template<typename F>
  constexpr auto or_else(F&& func) const& {
    if (this->has_value()) {
      if constexpr (meta::is_void_v<T>) {
        using G = impl::expect_monadic_chain_t<F, void>;
        static_assert(meta::expected_with_type<G, T>,
                      "Fn needs to return an expected with value T");
        return G{in_place};
      } else {
        using G = impl::expect_monadic_chain_t<F, decltype(this->get_error())>;
        static_assert(meta::expected_with_type<G, T>,
                      "Fn needs to return an expected with value T");
        return G{in_place, this->get()};
      }
    } else {
      return ::ntf::forward<F>(func)(this->get_error());
    }
  }

  template<typename Fn>
  constexpr auto or_else(Fn&& func) const&& {
    if (this->has_value()) {
      if constexpr (meta::is_void_v<T>) {
        using G = impl::expect_monadic_chain_t<Fn, void>;
        static_assert(meta::expected_with_type<G, T>,
                      "Fn needs to return an expected with value T");
        return G{in_place};
      } else {
        using G = impl::expect_monadic_chain_t<Fn, decltype(::ntf::move(this->get_error()))>;
        static_assert(meta::expected_with_type<G, T>,
                      "Fn needs to return an expected with value T");
        return G{in_place, ::ntf::move(this->get())};
      }
    } else {
      return ::ntf::forward<Fn>(func)(::ntf::move(this->get_error()));
    }
  }

public:
  template<typename Fn>
  constexpr auto transform(Fn&& func) & {
    if constexpr (meta::is_void_v<T>) {
      using U = impl::expect_monadic_transform_t<Fn, void>;
      static_assert(!meta::is_reference_v<U>, "Fn can't return a reference type");

      if (this->has_value()) {
        if constexpr (meta::is_void_v<U>) {
          ::ntf::forward<Fn>(func)();
          return Expected<void, E>{in_place};
        } else {
          return Expected<U, E>{in_place, ::ntf::forward<Fn>(func)()};
        }
      } else {
        return Expected<U, E>{unexpect, this->get_error()};
      }
    } else {
      using U = impl::expect_monadic_transform_t<Fn, decltype(this->get())>;
      static_assert(!meta::is_reference_v<U>, "Fn can't return a reference type");

      if (this->has_value()) {
        if constexpr (meta::is_void_v<U>) {
          ::ntf::forward<Fn>(func)(this->get());
          return Expected<void, E>{in_place};
        } else {
          return Expected<U, E>{in_place, ::ntf::forward<Fn>(func)(this->get())};
        }
      } else {
        return Expected<U, E>{unexpect, this->get_error()};
      }
    }
  }

  template<typename Fn>
  constexpr auto transform(Fn&& func) && {
    if constexpr (meta::is_void_v<T>) {
      using U = impl::expect_monadic_transform_t<Fn, void>;
      static_assert(!meta::is_reference_v<U>, "Fn can't return a reference type");

      if (this->has_value()) {
        if constexpr (meta::is_void_v<U>) {
          ::ntf::forward<Fn>(func)();
          return Expected<void, E>{in_place};
        } else {
          return Expected<U, E>{in_place, ::ntf::forward<Fn>(func)()};
        }
      } else {
        return Expected<U, E>{unexpect, ::ntf::move(this->get_error())};
      }
    } else {
      using U = impl::expect_monadic_transform_t<Fn, decltype(::ntf::move(this->get()))>;
      static_assert(!meta::is_reference_v<U>, "Fn can't return a reference type");

      if (this->has_value()) {
        if constexpr (meta::is_void_v<U>) {
          ::ntf::forward<Fn>(func)(::ntf::move(this->get()));
          return Expected<void, E>{in_place};
        } else {
          return Expected<U, E>{in_place, ::ntf::forward<Fn>(func)(::ntf::move(this->get()))};
        }
      } else {
        return Expected<U, E>{unexpect, ::ntf::move(this->get_error())};
      }
    }
  }

  template<typename Fn>
  constexpr auto transform(Fn&& func) const& {
    if constexpr (meta::is_void_v<T>) {
      using U = impl::expect_monadic_transform_t<Fn, void>;
      static_assert(!meta::is_reference_v<U>, "Fn can't return a reference type");

      if (this->has_value()) {
        if constexpr (meta::is_void_v<U>) {
          ::ntf::forward<Fn>(func)();
          return Expected<void, E>{in_place};
        } else {
          return Expected<U, E>{in_place, ::ntf::forward<Fn>(func)()};
        }
      } else {
        return Expected<U, E>{unexpect, this->get_error()};
      }
    } else {
      using U = impl::expect_monadic_transform_t<Fn, decltype(this->get())>;
      static_assert(!meta::is_reference_v<U>, "Fn can't return a reference type");

      if (this->has_value()) {
        if constexpr (meta::is_void_v<U>) {
          ::ntf::forward<Fn>(func)(this->get());
          return Expected<void, E>{in_place};
        } else {
          return Expected<U, E>{in_place, ::ntf::forward<Fn>(func)(this->get())};
        }
      } else {
        return Expected<U, E>{unexpect, this->get_error()};
      }
    }
  }

  template<typename Fn>
  constexpr auto transform(Fn&& func) const&& {
    if constexpr (meta::is_void_v<T>) {
      using U = impl::expect_monadic_transform_t<Fn, void>;
      static_assert(!meta::is_reference_v<U>, "Fn can't return a reference type");

      if (this->has_value()) {
        if constexpr (meta::is_void_v<U>) {
          ::ntf::forward<Fn>(func)();
          return Expected<void, E>{in_place};
        } else {
          return Expected<U, E>{in_place, ::ntf::forward<Fn>(func)()};
        }
      } else {
        return Expected<U, E>{unexpect, ::ntf::move(this->get_error())};
      }
    } else {
      using U = impl::expect_monadic_transform_t<Fn, decltype(::ntf::move(this->get()))>;
      static_assert(!meta::is_reference_v<U>, "Fn can't return a reference type");

      if (this->has_value()) {
        if constexpr (meta::is_void_v<U>) {
          ::ntf::forward<Fn>(func)(::ntf::move(this->get()));
          return Expected<void, E>{in_place};
        } else {
          return Expected<U, E>{in_place, ::ntf::forward<Fn>(func)(::ntf::move(this->get()))};
        }
      } else {
        return Expected<U, E>{unexpect, ::ntf::move(this->get_error())};
      }
    }
  }

public:
  template<typename Fn>
  constexpr auto transform_error(Fn&& func) & {
    if constexpr (meta::is_void_v<T>) {
      using G = impl::expect_monadic_transform_t<Fn, void>;
      static_assert(!meta::is_void_v<G>, "Fn has to return a non void error value");
      static_assert(!meta::is_reference_v<G>, "Fn can't return a reference type");

      if (this->has_value()) {
        return Expected<void, G>{in_place};
      } else {
        return Expected<T, G>{unexpect, ::ntf::forward<Fn>(func)(this->get_error())};
      }
    } else {
      using G = impl::expect_monadic_transform_t<Fn, decltype(this->get_error())>;
      static_assert(!meta::is_void_v<G>, "Fn has to return a non void error value");
      static_assert(!meta::is_reference_v<G>, "Fn can't return a reference type");

      if (this->has_value()) {
        return Expected<T, G>{in_place, this->get()};
      } else {
        return Expected<T, G>{unexpect, ::ntf::forward<Fn>(func)(this->get_error())};
      }
    }
  }

  template<typename Fn>
  constexpr auto transform_error(Fn&& func) && {
    if constexpr (meta::is_void_v<T>) {
      using G = impl::expect_monadic_transform_t<Fn, void>;
      static_assert(!meta::is_void_v<G>, "Fn has to return a non void error value");
      static_assert(!meta::is_reference_v<G>, "Fn can't return a reference type");

      if (this->has_value()) {
        return Expected<void, G>{in_place};
      } else {
        return Expected<T, G>{unexpect, ::ntf::forward<Fn>(func)(::ntf::move(this->get_error()))};
      }
    } else {
      using G = impl::expect_monadic_transform_t<Fn, decltype(::ntf::move(this->get_error()))>;
      static_assert(!meta::is_void_v<G>, "Fn has to return a non void error value");
      static_assert(!meta::is_reference_v<G>, "Fn can't return a reference type");

      if (this->has_value()) {
        return Expected<T, G>{in_place, ::ntf::move(this->get())};
      } else {
        return Expected<T, G>{unexpect, ::ntf::forward<Fn>(func)(::ntf::move(this->get_error()))};
      }
    }
  }

  template<typename Fn>
  constexpr auto transform_error(Fn&& func) const& {
    if constexpr (meta::is_void_v<T>) {
      using G = impl::expect_monadic_transform_t<Fn, void>;
      static_assert(!meta::is_void_v<G>, "Fn has to return a non void error value");
      static_assert(!meta::is_reference_v<G>, "Fn can't return a reference type");

      if (this->has_value()) {
        return Expected<void, G>{in_place};
      } else {
        return Expected<T, G>{unexpect, ::ntf::forward<Fn>(func)(this->get_error())};
      }
    } else {
      using G = impl::expect_monadic_transform_t<Fn, decltype(this->get_error())>;
      static_assert(!meta::is_void_v<G>, "Fn has to return a non void error value");
      static_assert(!meta::is_reference_v<G>, "Fn can't return a reference type");

      if (this->has_value()) {
        return Expected<T, G>{in_place, this->get()};
      } else {
        return Expected<T, G>{unexpect, ::ntf::forward<Fn>(func)(this->get_error())};
      }
    }
  }

  template<typename Fn>
  constexpr auto transform_error(Fn&& func) const&& {
    if constexpr (meta::is_void_v<T>) {
      using G = impl::expect_monadic_transform_t<Fn, void>;
      static_assert(!meta::is_void_v<G>, "Fn has to return a non void error value");
      static_assert(!meta::is_reference_v<G>, "Fn can't return a reference type");

      if (this->has_value()) {
        return Expected<void, G>{in_place};
      } else {
        return Expected<T, G>{unexpect, ::ntf::forward<Fn>(func)(::ntf::move(this->get_error()))};
      }
    } else {
      using G = impl::expect_monadic_transform_t<Fn, decltype(::ntf::move(this->get_error()))>;
      static_assert(!meta::is_void_v<G>, "Fn has to return a non void error value");
      static_assert(!meta::is_reference_v<G>, "Fn can't return a reference type");

      if (this->has_value()) {
        return Expected<T, G>{in_place, ::ntf::move(this->get())};
      } else {
        return Expected<T, G>{unexpect, ::ntf::forward<Fn>(func)(::ntf::move(this->get_error()))};
      }
    }
  }
};

} // namespace impl

template<typename T, typename E>
class Expected : public impl::ExpectedMonadicOps<T, E> {
private:
  using base_t = impl::ExpectedStorage<T, E>;

public:
  using value_type = T;
  using error_type = E;

  template<typename U>
  using rebind = Expected<U, error_type>;

public:
  using impl::ExpectedMonadicOps<T, E>::ExpectedMonadicOps;

public:
  constexpr explicit operator bool() const noexcept { return this->has_value(); }

  constexpr const T* operator->() const {
    NTF_ASSERT(*this, "No object in expected");
    return ::ntf::addressof(this->get());
  }

  constexpr T* operator->() {
    NTF_ASSERT(*this, "No object in expected");
    return ::ntf::addressof(this->get());
  }

  constexpr const T& operator*() const& {
    NTF_ASSERT(*this, "No object in expected");
    return this->get();
  }

  constexpr T& operator*() & {
    NTF_ASSERT(*this, "No object in expected");
    return this->get();
  }

  constexpr const T&& operator*() const&& {
    NTF_ASSERT(*this, "No object in expected");
    return ::ntf::move(this->get());
  }

  constexpr T&& operator*() && {
    NTF_ASSERT(*this, "No object in expected");
    return ::ntf::move(this->get());
  }

public:
  constexpr const T& value() const& {
    NTF_THROW_IF(!*this, BadExpectedAccess<E>(as_const(this->get_error())));
    return this->get();
  }

  constexpr T& value() & {
    NTF_THROW_IF(!*this, BadExpectedAccess<E>(as_const(this->get_error())));
    return this->get();
  }

  constexpr T&& value() && {
    NTF_THROW_IF(!*this, BadExpectedAccess<E>(::ntf::move(this->get_error())));
    return ::ntf::move(this->get());
  }

  constexpr const T&& value() const&& {
    NTF_THROW_IF(!*this, BadExpectedAccess<E>(::ntf::move(this->get_error())));
    return ::ntf::move(this->get());
  }

public:
  constexpr const E& error() const& {
    NTF_ASSERT(!*this, "No error in expected");
    return this->get_error();
  }

  constexpr E& error() & {
    NTF_ASSERT(!*this, "No error in expected");
    return this->get_error();
  }

  constexpr const E&& error() const&& {
    NTF_ASSERT(!*this, "No error in expected");
    return ::ntf::move(this->get_error());
  }

  constexpr E&& error() && {
    NTF_ASSERT(!*this, "No error in expected");
    return ::ntf::move(this->get_error());
  }

public:
  template<typename U = meta::remove_cv_t<T>>
  requires(meta::convertible_to<U, T> && meta::copy_constructible<T>)
  constexpr T value_or(U&& val) const& {
    return *this ? this->get() : static_cast<T>(::ntf::forward<U>(val));
  }

  template<typename U = meta::remove_cv_t<T>>
  requires(meta::convertible_to<T, U> && meta::move_constructible<T>)
  constexpr T value_or(U&& val) && {
    return *this ? ::ntf::move(this->get()) : static_cast<T>(::ntf::forward<U>(val));
  }

  template<typename G = E>
  requires(meta::convertible_to<E, G> && meta::copy_constructible<E>)
  constexpr E error_or(G&& err) const& {
    return !*this ? this->get_error() : static_cast<E>(::ntf::forward<G>(err));
  }

  template<typename G = E>
  requires(meta::convertible_to<E, G> && meta::move_constructible<E>)
  constexpr E error_or(G&& err) && {
    return !*this ? ::ntf::move(this->get_error()) : static_cast<E>(::ntf::forward<G>(err));
  }
};

template<typename E>
class Expected<void, E> : public impl::ExpectedMonadicOps<void, E> {
private:
  using base_t = impl::ExpectedStorage<void, E>;

public:
  using value_type = void;
  using error_type = E;

  template<typename U>
  using rebind = Expected<U, error_type>;

public:
  using impl::ExpectedMonadicOps<void, E>::ExpectedMonadicOps;
  using impl::ExpectedMonadicOps<void, E>::operator=;

public:
  constexpr explicit operator bool() const noexcept { return this->has_value(); }

  constexpr void operator*() const& noexcept {}

  constexpr void operator*() && noexcept {}

public:
  constexpr void value() const& {
    NTF_THROW_IF(!*this, BadExpectedAccess<E>(as_const(this->get_error())));
  }

  constexpr void value() && {
    NTF_THROW_IF(!*this, BadExpectedAccess<E>(::ntf::move(this->get_error())));
  }

public:
  constexpr const E& error() const& {
    NTF_ASSERT(!*this, "No error in expected");
    return this->get_error();
  }

  constexpr E& error() & {
    NTF_ASSERT(!*this, "No error in expected");
    return this->get_error();
  }

  constexpr const E&& error() const&& {
    NTF_ASSERT(!*this, "No error in expected");
    return ::ntf::move(this->get_error());
  }

  constexpr E&& error() && {
    NTF_ASSERT(!*this, "No error in expected");
    return ::ntf::move(this->get_error());
  }

public:
  template<typename G = E>
  requires(meta::convertible_to<E, G> && meta::copy_constructible<E>)
  constexpr E error_or(G&& err) const& {
    return !*this ? this->get_error() : static_cast<E>(::ntf::forward<G>(err));
  }

  template<typename G = E>
  requires(meta::convertible_to<E, G> && meta::move_constructible<E>)
  constexpr E error_or(G&& err) && {
    return !*this ? ::ntf::move(this->get_error()) : static_cast<E>(::ntf::forward<G>(err));
  }
};

template<typename T, typename E, typename U, typename G>
constexpr bool operator==(const Expected<T, E>& lhs, const Expected<U, G>& rhs)
requires(!meta::is_void_v<T> && !meta::is_void_v<U>)
{
  return lhs.has_value() != rhs.has_value()
         ? false
         : (lhs.has_value() ? *lhs == *rhs : lhs.error() == rhs.error());
}

template<typename T, typename E, typename G>
constexpr bool operator==(const Expected<T, E>& lhs, const unexpected<G>& unex)
requires(!meta::is_void_v<T>)
{
  return !lhs.has_value() && static_cast<bool>(lhs.value() == unex.value());
}

template<typename T, typename E, typename U>
constexpr bool operator==(const Expected<T, E>& lhs, const U& val) noexcept
requires(!meta::is_void_v<T>)
{
  return lhs.has_value() && static_cast<bool>(*lhs == val);
}

template<typename E, typename G>
constexpr bool operator==(const Expected<void, E>& lhs, const Expected<void, G>& rhs) noexcept {
  return lhs.has_value() != rhs.has_value()
         ? false
         : lhs.has_value() || static_cast<bool>(lhs.error() == rhs.error());
}

template<typename E, typename G>
constexpr bool operator==(const Expected<void, E>& lhs, const unexpected<G>& unex) noexcept {
  return !lhs.has_value() && static_cast<bool>(lhs.error() == unex.value());
}

} // namespace ntf

#endif // NTF_EXPECTED_HPP_
