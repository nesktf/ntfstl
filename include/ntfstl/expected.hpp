#pragma once

#include <ntfstl/concepts.hpp>
#include <ntfstl/error.hpp>
#include <ntfstl/types.hpp>

namespace ntf {

template<typename>
class bad_expected_access;

template<>
class bad_expected_access<void> : public std::exception {
public:
  bad_expected_access() = default;

public:
  const char* what() const noexcept override { return "bad_expected_access"; }
};

template<typename E>
class bad_expected_access : public bad_expected_access<void> {
public:
  explicit bad_expected_access(E err) : _err{std::move(err)} {}

public:
  E& error() & noexcept { return _err; }

  const E& error() const& noexcept { return _err; }

  E&& error() && noexcept { return std::move(_err); }

  const E&& error() const&& noexcept { return std::move(_err); }

private:
  E _err;
};

template<meta::not_void E>
class unexpected {
public:
  unexpected() = delete;

  template<meta::is_forwarding<E> U = E>
  constexpr unexpected(U&& err) noexcept(std::is_nothrow_constructible_v<E, U>) :
      _err(std::forward<U>(err)) {}

  template<typename... Args>
  requires(std::constructible_from<E, Args...>)
  constexpr explicit unexpected(Args&&... args) noexcept(
    std::is_nothrow_constructible_v<E, Args...>) : _err(std::forward<Args>(args)...) {}

  template<typename U, typename... Args>
  requires(std::constructible_from<E, std::initializer_list<U>, Args...>)
  constexpr explicit unexpected(std::initializer_list<U> list, Args&&... args) noexcept(
    std::is_nothrow_constructible_v<E, std::initializer_list<U>, Args...>) :
      _err(list, std::forward<Args>(args)...) {}

public:
  constexpr E& error() & noexcept { return _err; };

  constexpr const E& error() const& noexcept { return _err; }

  constexpr E&& error() && noexcept { return std::move(_err); };

  constexpr const E&& error() const&& noexcept { return std::move(_err); }

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
unexpected<typename std::decay_t<E>> make_unexpected(E&& e) {
  return unexpected<typename std::decay_t<E>>{std::forward<E>(e)};
}

NTF_DECLARE_TAG_TYPE(unexpect);

namespace impl {

template<typename T, typename E>
class expected_storage {
private:
  static constexpr bool _triv_destr_val = std::is_trivially_destructible_v<T>;
  static constexpr bool _triv_destr_err = std::is_trivially_destructible_v<E>;

  static constexpr bool _triv_movec_val = std::is_trivially_move_constructible_v<T>;
  static constexpr bool _triv_movec_err = std::is_trivially_move_constructible_v<E>;

  static constexpr bool _triv_copyc_val = std::is_trivially_copy_constructible_v<T>;
  static constexpr bool _triv_copyc_err = std::is_trivially_copy_constructible_v<E>;

  static constexpr bool _triv_movea_val = std::is_trivially_move_assignable_v<T>;
  static constexpr bool _triv_movea_err = std::is_trivially_move_assignable_v<E>;

  static constexpr bool _triv_copya_val = std::is_trivially_copy_assignable_v<T>;
  static constexpr bool _triv_copya_err = std::is_trivially_copy_assignable_v<E>;

public:
  template<meta::is_forwarding<T> U = T>
  requires(std::is_default_constructible_v<T>)
  constexpr expected_storage() noexcept(std::is_nothrow_default_constructible_v<T>) :
      _value(), _valid(true) {}

  template<typename... Args>
  requires(std::constructible_from<T, Args...>)
  constexpr expected_storage(in_place_t,
                             Args&&... arg) noexcept(std::is_nothrow_constructible_v<T, Args...>) :
      _value(std::forward<Args>(arg)...), _valid(true) {}

  template<typename U, typename... Args>
  requires(std::constructible_from<T, std::initializer_list<U>, Args...>)
  constexpr expected_storage(in_place_t, std::initializer_list<U> l, Args&&... arg) noexcept(
    std::is_nothrow_constructible_v<T, std::initializer_list<U>, Args...>) :
      _value(l, std::forward<Args>(arg)...), _valid(true) {}

  template<typename... Args>
  requires(std::constructible_from<E, Args...>)
  constexpr expected_storage(unexpect_t,
                             Args&&... arg) noexcept(std::is_nothrow_constructible_v<E, Args...>) :
      _error(std::forward<Args>(arg)...), _valid(false) {}

  template<typename U, typename... Args>
  requires(std::constructible_from<E, std::initializer_list<U>, Args...>)
  constexpr expected_storage(unexpect_t, std::initializer_list<U> l, Args&&... arg) noexcept(
    std::is_nothrow_constructible_v<E, std::initializer_list<U>, Args...>) :
      _error(l, std::forward<Args>(arg)...), _valid(false) {}

  template<meta::is_forwarding<T> U>
  constexpr expected_storage(U&& val) noexcept(meta::is_nothrow_forward_constructible<T>) :
      _value(std::forward<U>(val)), _valid(true) {}

  template<typename U = std::remove_cv_t<T>>
  constexpr explicit(!std::is_convertible_v<U, T>)
    expected_storage(U&& val) noexcept(std::is_nothrow_constructible_v<T, U>) :
      _value(std::forward<U>(val)), _valid(true) {}

  template<typename G>
  constexpr explicit(!std::is_convertible_v<const G&, E>) expected_storage(
    const unexpected<G>& unex) noexcept(std::is_nothrow_constructible_v<E, const G&>) :
      _error(unex.error()), _valid(false) {}

  template<typename G>
  constexpr explicit(!std::is_convertible_v<G, E>)
    expected_storage(unexpected<E>&& unex) noexcept(std::is_nothrow_constructible_v<E, G>) :
      _error(std::move(unex).error()), _valid(false) {}

public:
  constexpr expected_storage(expected_storage&& other) noexcept
  requires(std::move_constructible<T> && std::move_constructible<E> && _triv_movec_val &&
           _triv_movec_err)
  = default;

  constexpr expected_storage(expected_storage&& other) noexcept(
    std::is_nothrow_move_constructible_v<T> && std::is_nothrow_move_constructible_v<E>)
  requires(std::move_constructible<T> && std::move_constructible<E>)
      : _valid(std::move(other._valid)) {
    if (other._valid) {
      new (std::addressof(_value)) T(std::move(other._value));
    } else {
      new (std::addressof(_error)) E(std::move(other._error));
    }
  }

public:
  constexpr expected_storage(const expected_storage& other) noexcept
  requires(std::copy_constructible<T> && std::copy_constructible<E> && _triv_copyc_val &&
           _triv_copyc_err)
  = default;

  constexpr expected_storage(const expected_storage& other) noexcept(
    std::is_nothrow_copy_constructible_v<T> && std::is_nothrow_copy_constructible_v<E>)
  requires(std::copy_constructible<T> && std::copy_constructible<E>)
      : _valid(other._valid) {
    if (other._valid) {
      new (std::addressof(_value)) T(other._value);
    } else {
      new (std::addressof(_error)) T(other._error);
    }
  }

public:
  constexpr expected_storage& operator=(expected_storage&& other) noexcept
  requires(std::is_move_assignable_v<T> && std::is_move_assignable_v<E> && _triv_movea_val &&
           _triv_movea_err)
  = default;

  constexpr expected_storage&
  operator=(expected_storage& other) noexcept(std::is_nothrow_move_assignable_v<T> &&
                                              std::is_nothrow_move_assignable_v<E>)
  requires(std::is_move_assignable_v<T> && std::is_move_assignable_v<E> &&
           std::is_move_constructible_v<T> && std::is_move_constructible_v<E>)
  {
    if (_valid) {
      if (other._valid) {
        _value = std::move(other._value);
      } else {
        std::destroy_at(std::addressof(_value));
        // TODO: Handle case where E construction throws
        new (std::addressof(_error)) E(std::move(other._error));
      }
    } else {
      if (other._valid) {
        std::destroy_at(std::addressof(_error));
        // TODO: Handle case where T construction throws
        new (std::addressof(_value)) T(std::move(other._value));
      } else {
        _error = std::move(other._error);
      }
    }
    _valid = std::move(other._valid);
    return *this;
  }

public:
  constexpr expected_storage& operator=(const expected_storage& other) noexcept
  requires(std::is_copy_assignable_v<T> && std::is_copy_assignable_v<E> && _triv_copya_val &&
           _triv_copya_err)
  = default;

  constexpr expected_storage&
  operator=(const expected_storage& other) noexcept(std::is_nothrow_copy_assignable_v<T> &&
                                                    std::is_nothrow_copy_assignable_v<E>)
  requires(std::is_copy_assignable_v<T> && std::is_copy_assignable_v<E> &&
           std::is_copy_constructible_v<T> && std::is_copy_constructible_v<E>)
  {
    if (_valid) {
      if (other._valid) {
        _value = other._value;
      } else {
        std::destroy_at(std::addressof(_value));
        // TODO: Handle case where E construction throws
        new (std::addressof(_error)) E(other._error);
      }
    } else {
      if (other._valid) {
        std::destroy_at(std::addressof(_error));
        // TODO: Handle case where T construction throws
        new (std::addressof(_value)) T(other._value);
      } else {
        _error = other._error;
      }
    }
    _valid = other._valid;
    return *this;
  }

public:
  constexpr ~expected_storage() noexcept
  requires(_triv_destr_val && _triv_destr_err)
  = default;

  constexpr ~expected_storage() noexcept
  requires(!_triv_destr_val || !_triv_destr_err)
  {
    // Assume both T and E destructors don't throw
    if constexpr (_triv_destr_val && !_triv_destr_err) {
      if (!_valid) {
        std::destroy_at(&_error);
      }
    } else if constexpr (!_triv_destr_val && _triv_destr_err) {
      if (_valid) {
        std::destroy_at(&_value);
      }
    } else {
      if (_valid) {
        std::destroy_at(&_value);
      } else {
        std::destroy_at(&_error);
      }
    }
  }

public:
  template<typename... Args>
  constexpr void emplace(Args&&... args) {
    if (!_valid) {
      std::destroy_at(std::addressof(_error));
    }
    // TODO: Handle case where T construction throws
    new (std::addressof(_value)) T(std::forward<Args>(args)...);
    _valid = true;
  }

  template<typename U, typename... Args>
  constexpr void emplace(std::initializer_list<U> il, Args&&... args) {
    if (!_valid) {
      std::destroy_at(std::addressof(_error));
    }
    // TODO: Handle case where T construction throws
    new (std::addressof(_value)) T(il, std::forward<Args>(args)...);
    _valid = true;
  }

  template<typename... Args>
  constexpr void emplace_error(Args&&... args) {
    if (_valid) {
      std::destroy_at(std::addressof(_value));
    }
    // TODO: Handle case where E construction throws
    new (std::addressof(_error)) E(std::forward<Args>(args)...);
    _valid = false;
  }

  template<typename U, typename... Args>
  constexpr void emplace_error(std::initializer_list<U> il, Args&&... args) {
    if (_valid) {
      std::destroy_at(std::addressof(_value));
    }
    // TODO: Handle case where E construction throws
    new (std::addressof(_error)) E(il, std::forward<Args>(args)...);
    _valid = false;
  }

protected:
  constexpr bool has_value() const noexcept { return _valid; }

  constexpr T& get() & noexcept { return _value; }

  constexpr T&& get() && noexcept { return std::move(_value); }

  constexpr const T& get() const& noexcept { return _value; }

  constexpr const T&& get() const&& noexcept { return std::move(_value); }

  constexpr E& get_error() & noexcept { return _error; }

  constexpr E&& get_error() && noexcept { return std::move(_error); }

  constexpr const E& get_error() const& noexcept { return _error; }

  constexpr const E&& get_error() const&& noexcept { return std::move(_error); }

private:
  union {
    T _value;
    E _error;
  };

  bool _valid;
};

template<typename E>
class expected_storage<void, E> {
private:
  static constexpr bool _triv_destr = std::is_trivially_destructible_v<E>;
  static constexpr bool _triv_movec = std::is_trivially_move_constructible_v<E>;
  static constexpr bool _triv_copyc = std::is_trivially_copy_constructible_v<E>;
  static constexpr bool _triv_movea = std::is_trivially_move_assignable_v<E>;
  static constexpr bool _triv_copya = std::is_trivially_copy_assignable_v<E>;

public:
  constexpr expected_storage() noexcept : _valid(true) {}

  constexpr expected_storage(in_place_t) noexcept : _valid(true) {}

  template<typename... Args>
  requires(std::constructible_from<E, Args...>)
  constexpr expected_storage(unexpect_t,
                             Args&&... arg) noexcept(std::is_nothrow_constructible_v<E, Args...>) :
      _error(std::forward<Args>(arg)...), _valid(false) {}

  template<typename U, typename... Args>
  requires(std::constructible_from<E, std::initializer_list<U>, Args...>)
  constexpr expected_storage(unexpect_t, std::initializer_list<U> l, Args&&... arg) noexcept(
    std::is_nothrow_constructible_v<E, std::initializer_list<U>, Args...>) :
      _error(l, std::forward<Args>(arg)...), _valid(false) {}

  template<meta::is_forwarding<unexpected<E>> G>
  constexpr expected_storage(G&& unex) noexcept(meta::is_nothrow_forward_constructible<G>) :
      _error(std::forward<G>(unex).value()), _valid(false) {}

public:
  constexpr expected_storage(expected_storage&& other) noexcept
  requires(std::move_constructible<E> && _triv_movec)
  = default;

  constexpr expected_storage(expected_storage&& other) noexcept(
    std::is_nothrow_move_constructible_v<E>)
  requires(std::move_constructible<E> && !_triv_movec)
      : _valid(std::move(other._valid)) {
    if (!other._valid) {
      new (std::addressof(_error)) E(std::move(other._error));
    }
  }

public:
  constexpr expected_storage(const expected_storage& other) noexcept
  requires(std::copy_constructible<E> && _triv_copyc)
  = default;

  constexpr expected_storage(const expected_storage& other) noexcept(
    std::is_nothrow_copy_constructible_v<E>)
  requires(std::copy_constructible<E> && !_triv_copyc)
      : _valid(other._valid) {
    if (!other._valid) {
      new (std::addressof(_error)) E(other._error);
    }
  }

public:
  constexpr expected_storage& operator=(expected_storage&& other) noexcept
  requires(std::is_move_assignable_v<E> && _triv_movea)
  = default;

  constexpr expected_storage&
  operator=(expected_storage&& other) noexcept(std::is_nothrow_move_constructible_v<E> &&
                                               !_triv_movea) {
    const auto do_swap = [&]() {
      if (_valid) {
        if (!other._valid) {
          new (std::addressof(_error)) E(std::move(other._error));
        }
      } else {
        if (other._valid) {
          std::destroy_at(std::addressof(_error));
        } else {
          _error = std::move(other._error);
        }
      }
    };

    if constexpr (!std::is_nothrow_move_constructible_v<E>) {
      try {
        do_swap();
      } catch (...) {
        if (!_valid) {
          std::destroy_at(std::addressof(_error));
        }
        _valid = true;
        NTF_RETHROW();
      }
    } else {
      do_swap();
    }
    _valid = std::move(other._valid);
    return *this;
  }

public:
  constexpr expected_storage& operator=(const expected_storage& other) noexcept
  requires(std::is_copy_assignable_v<E> && _triv_copya)
  = default;

  constexpr expected_storage&
  operator=(const expected_storage& other) noexcept(std::is_nothrow_copy_assignable_v<E> &&
                                                    !_triv_copya) {
    const auto do_swap = [&]() {
      if (_valid) {
        if (!other._valid) {
          new (std::addressof(_error)) E(other._error);
        }
      } else {
        if (other._valid) {
          std::destroy_at(std::addressof(_error));
        } else {
          _error = other._error;
        }
      }
    };

    if constexpr (!std::is_nothrow_copy_constructible_v<E>) {
      try {
        do_swap();
      } catch (...) {
        if (!_valid) {
          std::destroy_at(std::addressof(_error));
        }
        _valid = true;
        NTF_RETHROW();
      }
    } else {
      do_swap();
    }
    _valid = std::move(other._valid);
    return *this;
  }

public:
  constexpr ~expected_storage() noexcept
  requires(_triv_destr)
  = default;

  constexpr ~expected_storage() noexcept(std::is_nothrow_destructible_v<E>)
  requires(!_triv_destr)
  {
    if (!_valid) {
      std::destroy_at(std::addressof(_error));
    }
  }

protected:
  template<typename... Args>
  constexpr void emplace() noexcept {
    if (!_valid) {
      std::destroy_at(std::addressof(_error));
    }
    _valid = true;
  }

  template<typename... Args>
  constexpr void emplace_error(Args&&... args) noexcept(std::is_nothrow_constructible_v<E>) {
    if (!_valid) {
      std::destroy_at(std::addressof(_error));
    }
    if constexpr (!std::is_nothrow_constructible_v<E>) {
      try {
        new (std::addressof(_error)) E(std::forward<Args>(args)...);
        _valid = false;
      } catch (...) {
        NTF_RETHROW();
        _valid = true;
      }
    } else {
      new (std::addressof(_error)) E(std::forward<Args>(args)...);
      _valid = false;
    }
  }

  constexpr bool has_value() const noexcept { return _valid; }

  void get() const noexcept {}

  constexpr E& get_error() & noexcept { return _error; }

  constexpr E&& get_error() && noexcept { return std::move(_error); }

  constexpr const E& get_error() const& noexcept { return _error; }

  constexpr const E&& get_error() const&& noexcept { return std::move(_error); }

private:
  union {
    char _dummy;
    E _error;
  };

  bool _valid;
};

} // namespace impl

template<typename T, typename E>
class expected;

namespace meta {

NTF_DEFINE_TEMPLATE_CHECKER(expected);

template<typename Exp, typename T>
struct expected_check_t : public std::false_type {};

template<typename T>
struct expected_check_t<expected<T, void>, T> : public std::false_type {};

template<typename T, typename E>
struct expected_check_t<expected<T, E>, T> : public std::true_type {};

template<typename Exp, typename T>
constexpr bool expected_check_t_v = expected_check_t<Exp, T>::value;

template<typename Exp, typename T>
concept expected_with_type = expected_check_t_v<Exp, T>;

template<typename Exp, typename E>
struct expected_check_e : public std::false_type {};

template<typename T>
struct expected_check_e<expected<T, void>, void> : public std::false_type {};

template<typename T, typename E>
struct expected_check_e<expected<T, E>, E> : public std::true_type {};

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
  using type = expected<T, Err>;
};

} // namespace meta

template<typename T, typename E>
class expected : public impl::expected_storage<T, E> {
private:
  using base_t = impl::expected_storage<T, E>;

public:
  using value_type = T;
  using error_type = E;

  template<typename U>
  using rebind = expected<U, error_type>;

public:
  using impl::expected_storage<T, E>::expected_storage;
  using impl::expected_storage<T, E>::operator=;

public:
  constexpr bool has_value() const noexcept { return base_t::has_value(); }

public:
  constexpr explicit operator bool() const noexcept { return base_t::has_value(); }

  constexpr const T* operator->() const
  requires(meta::not_void<T>)
  {
    NTF_ASSERT(*this, "No object in expected");
    return std::addressof(this->get());
  }

  constexpr T* operator->()
  requires(meta::not_void<T>)
  {
    NTF_ASSERT(*this, "No object in expected");
    return std::addressof(this->get());
  }

  constexpr const T& operator*() const&
  requires(meta::not_void<T>)
  {
    NTF_ASSERT(*this, "No object in expected");
    return this->get();
  }

  constexpr T& operator*() &
  requires(meta::not_void<T>)
  {
    NTF_ASSERT(*this, "No object in expected");
    return this->get();
  }

  constexpr const T&& operator*() const&&
  requires(meta::not_void<T>)
  {
    NTF_ASSERT(*this, "No object in expected");
    return std::move(this->get());
  }

  constexpr T&& operator*() &&
  requires(meta::not_void<T>)
  {
    NTF_ASSERT(*this, "No object in expected");
    return std::move(this->get());
  }

  constexpr void operator*() const& noexcept
  requires(std::is_void_v<T>)
  {}

  constexpr void operator*() && noexcept
  requires(std::is_void_v<T>)
  {}

public:
  constexpr const T& value() const&
  requires(meta::not_void<T>)
  {
    NTF_THROW_IF(!*this, ::ntf::bad_expected_access<E>, std::as_const(this->get_error()));
    return this->get();
  }

  constexpr T& value() &
  requires(meta::not_void<T>)
  {
    NTF_THROW_IF(!*this, ::ntf::bad_expected_access<E>, std::as_const(this->get_error()));
    return this->get();
  }

  constexpr T&& value() &&
  requires(meta::not_void<T>)
  {
    NTF_THROW_IF(!*this, ::ntf::bad_expected_access<E>, std::move(this->get_error()));
    return std::move(this->get());
  }

  constexpr const T&& value() const&&
  requires(meta::not_void<T>)
  {
    NTF_THROW_IF(!*this, ::ntf::bad_expected_access<E>, std::move(this->get_error()));
    return std::move(this->get());
  }

  constexpr void value() const&
  requires(std::is_void_v<T>)
  {
    NTF_THROW_IF(!*this, ::ntf::bad_expected_access<E>, std::as_const(this->get_error()));
  }

  constexpr void value() &&
  requires(std::is_void_v<T>)
  {
    NTF_THROW_IF(!*this, ::ntf::bad_expected_access<E>, std::move(this->get_error()));
  }

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
    return std::move(this->get_error());
  }

  constexpr E&& error() && {
    NTF_ASSERT(!*this, "No error in expected");
    return std::move(this->get_error());
  }

public:
  template<typename U = std::remove_cv_t<T>>
  requires(std::convertible_to<T, U> && std::is_copy_constructible_v<T> && meta::not_void<T>)
  constexpr T value_or(U&& val) const& {
    return *this ? this->get() : static_cast<T>(std::forward<U>(val));
  }

  template<typename U = std::remove_cv_t<T>>
  requires(std::convertible_to<T, U> && std::is_move_constructible_v<T> && meta::not_void<T>)
  constexpr T value_or(U&& val) && {
    return *this ? std::move(this->get()) : static_cast<T>(std::forward<U>(val));
  }

  template<typename G = E>
  requires(std::convertible_to<E, G> && std::is_copy_constructible_v<E>)
  constexpr E error_or(G&& err) const& {
    return !*this ? this->get_error() : static_cast<E>(std::forward<G>(err));
  }

  template<typename G = E>
  requires(std::convertible_to<E, G> && std::is_move_constructible_v<E>)
  constexpr E error_or(G&& err) && {
    return !*this ? std::move(this->get_error()) : static_cast<E>(std::forward<G>(err));
  }

private:
  template<typename F, typename U>
  using func_invoke_t =
    std::conditional_t<std::is_void_v<T>, std::remove_cvref_t<std::invoke_result<F>>,
                       std::remove_cvref_t<std::invoke_result<F, U>>>;

public:
  template<typename F>
  constexpr auto and_then(F&& func) & {
    using U = func_invoke_t<F, decltype(this->get())>;
    static_assert(meta::expected_with_error<U, E>, "F needs to return an expected with error E");

    if (this->has_value()) {
      if constexpr (std::is_void_v<T>) {
        return std::invoke(std::forward<F>(func));
      } else {
        return std::invoke(std::forward<F>(func), this->get());
      }
    } else {
      return U{unexpect, this->get_error()};
    }
  }

  template<typename F>
  constexpr auto and_then(F&& func) && {
    using U = func_invoke_t<F, decltype(std::move(this->get()))>;
    static_assert(meta::expected_with_error<U, E>, "F needs to return an expected with error E");

    if (this->has_value()) {
      if constexpr (std::is_void_v<T>) {
        return std::invoke(std::forward<F>(func));
      } else {
        return std::invoke(std::forward<F>(func), std::move(this->get()));
      }
    } else {
      return U{unexpect, std::move(this->get_error())};
    }
  }

  template<typename F>
  constexpr auto and_then(F&& func) const& {
    using U = func_invoke_t<F, decltype(this->get())>;
    static_assert(meta::expected_with_error<U, E>, "F needs to return an expected with error E");

    if (this->has_value()) {
      if constexpr (std::is_void_v<T>) {
        return std::invoke(std::forward<F>(func));
      } else {
        return std::invoke(std::forward<F>(func), this->get());
      }
    } else {
      return U{unexpect, this->get_error()};
    }
  }

  template<typename F>
  constexpr auto and_then(F&& func) const&& {
    using U = func_invoke_t<F, decltype(std::move(this->get()))>;
    static_assert(meta::expected_with_error<U, E>, "F needs to return an expected with error E");

    if (this->has_value()) {
      if constexpr (std::is_void_v<T>) {
        return std::invoke(std::forward<F>(func));
      } else {
        return std::invoke(std::forward<F>(func), std::move(this->get()));
      }
    } else {
      return U{unexpect, std::move(this->get_error())};
    }
  }

public:
  template<typename F>
  constexpr auto or_else(F&& func) & {
    using G = func_invoke_t<F, decltype(this->get_error())>;
    static_assert(meta::expected_with_error<G, E>, "F needs to return an expected with error E");

    if (this->has_value()) {
      if constexpr (std::is_void_v<T>) {
        return G{in_place};
      } else {
        return G{in_place, this->get()};
      }
    } else {
      return std::invoke(std::forward<F>(func), this->get_error());
    }
  }

  template<typename F>
  constexpr auto or_else(F&& func) && {
    using G = func_invoke_t<F, decltype(std::move(this->get_error()))>;
    static_assert(meta::expected_with_error<G, E>, "F needs to return an expected with error E");

    if (this->has_value()) {
      if constexpr (std::is_void_v<T>) {
        return G{in_place};
      } else {
        return G{in_place, std::move(this->get())};
      }
    } else {
      return std::invoke(std::forward<F>(func), std::move(this->get_error()));
    }
  }

  template<typename F>
  constexpr auto or_else(F&& func) const& {
    using G = func_invoke_t<F, decltype(this->get_error())>;
    static_assert(meta::expected_with_error<G, E>, "F needs to return an expected with error E");

    if (this->has_value()) {
      if constexpr (std::is_void_v<T>) {
        return G{in_place};
      } else {
        return G{in_place, this->get()};
      }
    } else {
      return std::invoke(std::forward<F>(func), this->get_error());
    }
  }

  template<typename F>
  constexpr auto or_else(F&& func) const&& {
    using G = func_invoke_t<F, decltype(std::move(this->get_error()))>;
    static_assert(meta::expected_with_error<G, E>, "F needs to return an expected with error E");

    if (this->has_value()) {
      if constexpr (std::is_void_v<T>) {
        return G{in_place};
      } else {
        return G{in_place, std::move(this->get())};
      }
    } else {
      return std::invoke(std::forward<F>(func), std::move(this->get_error()));
    }
  }

public:
  template<typename F>
  constexpr auto transform(F&& func) & {
    using U = func_invoke_t<F, decltype(this->get())>;
    static_assert(!std::is_reference_v<U>, "F can't return a reference type");

    if (this->has_value()) {
      if constexpr (std::is_void_v<U>) {
        if constexpr (std::is_void_v<T>) {
          std::invoke(std::forward<F>(func));
        } else {
          std::invoke(std::forward<F>(func), this->get());
        }
        return expected<void, E>{in_place};
      } else {
        if constexpr (std::is_void_v<T>) {
          return expected<U, E>{in_place, std::invoke(std::forward<F>(func))};
        } else {
          return expected<U, E>{in_place, std::invoke(std::forward<F>(func), this->get())};
        }
      }
    } else {
      return expected<U, E>{unexpect, this->get_error()};
    }
  }

  template<typename F>
  constexpr auto transform(F&& func) && {
    using U = func_invoke_t<F, decltype(std::move(this->get()))>;
    static_assert(!std::is_reference_v<U>, "F can't return a reference type");

    if (this->has_value()) {
      if constexpr (std::is_void_v<U>) {
        if constexpr (std::is_void_v<T>) {
          std::invoke(std::forward<F>(func));
        } else {
          std::invoke(std::forward<F>(func), std::move(this->get()));
        }
        return expected<void, E>{in_place};
      } else {
        if constexpr (std::is_void_v<T>) {
          return expected<U, E>{in_place, std::invoke(std::forward<F>(func))};
        } else {
          return expected<U, E>{in_place,
                                std::invoke(std::forward<F>(func), std::move(this->get()))};
        }
      }
    } else {
      return expected<U, E>{unexpect, this->get_error()};
    }
  }

  template<typename F>
  constexpr auto transform(F&& func) const& {
    using U = func_invoke_t<F, decltype(this->get())>;
    static_assert(!std::is_reference_v<U>, "F can't return a reference type");

    if (this->has_value()) {
      if constexpr (std::is_void_v<U>) {
        if constexpr (std::is_void_v<T>) {
          std::invoke(std::forward<F>(func));
        } else {
          std::invoke(std::forward<F>(func), this->get());
        }
        return expected<void, E>{in_place};
      } else {
        if constexpr (std::is_void_v<T>) {
          return expected<U, E>{in_place, std::invoke(std::forward<F>(func))};
        } else {
          return expected<U, E>{in_place, std::invoke(std::forward<F>(func), this->get())};
        }
      }
    } else {
      return expected<U, E>{unexpect, this->get_error()};
    }
  }

  template<typename F>
  constexpr auto transform(F&& func) const&& {
    using U = func_invoke_t<F, decltype(std::move(this->get()))>;
    static_assert(!std::is_reference_v<U>, "F can't return a reference type");

    if (this->has_value()) {
      if constexpr (std::is_void_v<U>) {
        if constexpr (std::is_void_v<T>) {
          std::invoke(std::forward<F>(func));
        } else {
          std::invoke(std::forward<F>(func), std::move(this->get()));
        }
        return expected<void, E>{in_place};
      } else {
        if constexpr (std::is_void_v<T>) {
          return expected<U, E>{in_place, std::invoke(std::forward<F>(func))};
        } else {
          return expected<U, E>{in_place,
                                std::invoke(std::forward<F>(func), std::move(this->get()))};
        }
      }
    } else {
      return expected<U, E>{unexpect, this->get_error()};
    }
  }

public:
  template<typename F>
  constexpr auto transform_error(F&& func) & {
    using G = func_invoke_t<F, decltype(this->get_error())>;
    static_assert(!std::is_void_v<G>, "F has to return a non void error value");
    static_assert(!std::is_reference_v<G>, "F can't return a reference type");
    if (this->has_value()) {
      if constexpr (std::is_void_v<T>) {
        return expected<void, G>{in_place};
      } else {
        return expected<T, G>{in_place, this->get()};
      }
    } else {
      return expected<T, G>{unexpect, std::invoke(std::forward<F>(func), this->get_error())};
    }
  }

  template<typename F>
  constexpr auto transform_error(F&& func) && {
    using G = func_invoke_t<F, decltype(std::move(this->get_error()))>;
    static_assert(!std::is_void_v<G>, "F has to return a non void error value");
    static_assert(!std::is_reference_v<G>, "F can't return a reference type");
    if (this->has_value()) {
      if constexpr (std::is_void_v<T>) {
        return expected<void, G>{in_place};
      } else {
        return expected<T, G>{in_place, std::move(this->get())};
      }
    } else {
      return expected<T, G>{unexpect,
                            std::invoke(std::forward<F>(func), std::move(this->get_error()))};
    }
  }
};

template<typename T, typename E, typename U, typename G>
constexpr bool operator==(const expected<T, E>& lhs, const expected<U, G>& rhs)
requires(meta::not_void<T> && meta::not_void<U>)
{
  return lhs.has_value() != rhs.has_value()
         ? false
         : (lhs.has_value() ? *lhs == *rhs : lhs.error() == rhs.error());
}

template<typename T, typename E, typename G>
constexpr bool operator==(const expected<T, E>& lhs, const unexpected<G>& unex)
requires(meta::not_void<T>)
{
  return !lhs.has_value() && static_cast<bool>(lhs.value() == unex.value());
}

template<typename T, typename E, typename U>
constexpr bool operator==(const expected<T, E>& lhs, const U& val) noexcept
requires(meta::not_void<T>)
{
  return lhs.has_value() && static_cast<bool>(*lhs == val);
}

template<typename E, typename U, typename G>
constexpr bool operator==(const expected<void, E>& lhs, const expected<U, G>& rhs) noexcept
requires(std::is_void_v<U>)
{
  return lhs.has_value() != rhs.has_value()
         ? false
         : lhs.has_value() || static_cast<bool>(lhs.error() == rhs.error());
}

template<typename E, typename G>
constexpr bool operator==(const expected<void, E>& lhs, const unexpected<G>& unex) noexcept {
  return !lhs.has_value() && static_cast<bool>(lhs.error() == unex.value());
}

} // namespace ntf
