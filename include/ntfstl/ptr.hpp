#pragma once

#include <ntfstl/core.hpp>

namespace ntf {

template<typename T>
class weak_ptr {
public:
  using element_type = T;
  using value_type = std::remove_cvref_t<T>;

  using pointer = T*;
  using const_pointer = const T*;
  using reference = T&;
  using const_reference = const T&;

public:
  constexpr weak_ptr() noexcept = default;

  constexpr weak_ptr(std::nullptr_t) noexcept :
    _ptr{nullptr} {}

  constexpr weak_ptr(pointer ptr) noexcept :
    _ptr{ptr} {}

  template<typename U>
  requires(std::is_convertible_v<U*, T*>)
  constexpr weak_ptr(U& obj) noexcept :
    _ptr{std::addressof(obj)} {}

  template<typename U>
  requires(std::is_convertible_v<U*, T*>)
  constexpr weak_ptr(const weak_ptr<U>& other) noexcept :
    _ptr{other.data()} {}

  constexpr weak_ptr(const weak_ptr&) noexcept = default;
  constexpr weak_ptr(weak_ptr&&) noexcept = default;

  constexpr ~weak_ptr() noexcept = default;

public:
  constexpr reference get() const noexcept(NTF_ASSERT_NOEXCEPT) { 
    NTF_ASSERT(_ptr, "Invalid pointer");
    return *_ptr;
  }
  constexpr pointer data() const noexcept { return _ptr; }

public:
  [[nodiscard]] constexpr bool empty() const { return _ptr == nullptr; }
  constexpr explicit operator bool() const { return !empty(); }

  constexpr pointer operator->() const noexcept(NTF_ASSERT_NOEXCEPT) {
    NTF_ASSERT(_ptr, "Invalid pointer");
    return _ptr;
  }
  constexpr reference operator*() const noexcept(NTF_ASSERT_NOEXCEPT) {
    NTF_ASSERT(_ptr, "Invalid pointer");
    return *_ptr;
  }

  constexpr weak_ptr& operator=(std::nullptr_t) noexcept {
    _ptr = nullptr;
    return *this;
  }

  constexpr weak_ptr& operator=(pointer ptr) noexcept {
    _ptr = ptr;
    return *this;
  }

  template<typename U>
  requires(std::is_convertible_v<U*, T*>)
  constexpr weak_ptr& operator=(U& obj) noexcept {
    _ptr = std::addressof(obj);
    return *this;
  }

  template<typename U>
  requires(std::is_convertible_v<U*, T*>)
  constexpr weak_ptr& operator=(const weak_ptr<U>& other) noexcept {
    _ptr = other.data();
    return *this;
  }

  constexpr weak_ptr& operator=(const weak_ptr&) noexcept = default;
  constexpr weak_ptr& operator=(weak_ptr&&) noexcept = default;

private:
  T* _ptr;
};

template<typename T>
using weak_cptr = weak_ptr<const std::remove_cvref_t<T>>;

} // namespace ntf
