#pragma once

#include <ntfstl/core.hpp>

namespace ntf {

template<typename T>
requires(!std::is_void_v<T> && !std::is_reference_v<T>)
class ptr_view {
public:
  using element_type = T;
  using value_type = std::remove_cvref_t<T>;

  using pointer = T*;
  using const_pointer = const T*;
  using reference = T&;
  using const_reference = const T&;

public:
  constexpr ptr_view() noexcept = default;

  constexpr ptr_view(std::nullptr_t) noexcept : _ptr{nullptr} {}

  constexpr ptr_view(pointer ptr) noexcept : _ptr{ptr} {}

  template<typename U>
  requires(std::is_convertible_v<U*, T*>)
  constexpr ptr_view(U& obj) noexcept : _ptr{std::addressof(obj)} {}

  template<typename U>
  requires(std::is_convertible_v<U*, T*> && !std::same_as<U, T>)
  constexpr ptr_view(U* ptr) noexcept : _ptr(ptr) {}

  template<typename U>
  requires(std::is_convertible_v<U*, T*>)
  constexpr ptr_view(const ptr_view<U>& other) noexcept : _ptr{other.data()} {}

  constexpr ptr_view(const ptr_view&) noexcept = default;
  constexpr ptr_view(ptr_view&&) noexcept = default;

  constexpr ~ptr_view() noexcept = default;

public:
  constexpr reference get() const {
    NTF_ASSERT(_ptr, "Invalid pointer");
    return *_ptr;
  }

  constexpr pointer ptr() const noexcept { return _ptr; }

  constexpr pointer data() const noexcept { return _ptr; }

public:
  [[nodiscard]] constexpr bool empty() const { return _ptr == nullptr; }

  constexpr explicit operator bool() const { return !empty(); }

  constexpr pointer operator->() const {
    NTF_ASSERT(_ptr, "Invalid pointer");
    return _ptr;
  }

  constexpr reference operator*() const {
    NTF_ASSERT(_ptr, "Invalid pointer");
    return *_ptr;
  }

  constexpr ptr_view& operator=(std::nullptr_t) noexcept {
    _ptr = nullptr;
    return *this;
  }

  constexpr ptr_view& operator=(pointer ptr) noexcept {
    _ptr = ptr;
    return *this;
  }

  template<typename U>
  requires(std::is_convertible_v<U*, T*> && !std::same_as<U, T>)
  constexpr ptr_view& operator=(U* ptr) {
    _ptr = ptr;
    return *this;
  }

  template<typename U>
  requires(std::is_convertible_v<U*, T*>)
  constexpr ptr_view& operator=(U& obj) noexcept {
    _ptr = std::addressof(obj);
    return *this;
  }

  template<typename U>
  requires(std::is_convertible_v<U*, T*>)
  constexpr ptr_view& operator=(const ptr_view<U>& other) noexcept {
    _ptr = other.data();
    return *this;
  }

  constexpr ptr_view& operator=(const ptr_view&) noexcept = default;
  constexpr ptr_view& operator=(ptr_view&&) noexcept = default;

public:
  operator reference() const { return get(); }

private:
  T* _ptr;
};

template<typename T>
requires(!std::is_void_v<T> && !std::is_reference_v<T>)
class ref_view {
public:
  using element_type = T;
  using value_type = std::remove_cvref_t<T>;

  using pointer = T*;
  using const_pointer = const T*;
  using reference = T&;
  using const_reference = const T&;

public:
  constexpr ref_view(reference obj) noexcept : _ptr(std::addressof(obj)) {}

  template<typename U>
  requires(std::is_convertible_v<U*, T*>)
  constexpr ref_view(U& obj) noexcept : _ptr(std::addressof(obj)) {}

  constexpr explicit ref_view(pointer ptr) : _ptr(ptr) {
    NTF_THROW_IF(!_ptr, std::runtime_error("Assigning nullptr to ref_view"));
  }

  template<typename U>
  requires(std::is_convertible_v<U*, T*> && !std::same_as<U, T>)
  constexpr ref_view(U* ptr) : _ptr(ptr) {
    NTF_THROW_IF(!_ptr, std::runtime_error("Assigning nullptr to ref_view"));
  }

  template<typename U>
  requires(std::is_convertible_v<U*, T*>)
  constexpr ref_view(const ref_view<U>& other) noexcept : _ptr{other.data()} {}

  constexpr ref_view(const ref_view&) noexcept = default;
  constexpr ref_view(ref_view&&) noexcept = default;

  constexpr ~ref_view() noexcept = default;

public:
  constexpr reference get() const noexcept { return *_ptr; }

  constexpr pointer ptr() const noexcept { return _ptr; }

  constexpr pointer data() const noexcept { return _ptr; }

public:
  constexpr pointer operator->() const noexcept { return _ptr; }

  constexpr reference operator*() const noexcept { return *_ptr; }

  constexpr ref_view& operator=(pointer ptr) {
    NTF_THROW_IF(!ptr, std::runtime_error("Assigning nullptr to ref_view"));
    _ptr = ptr;
    return *this;
  }

  template<typename U>
  requires(std::is_convertible_v<U*, T*> && !std::same_as<U, T>)
  constexpr ref_view& operator=(U* ptr) {
    NTF_THROW_IF(!ptr, std::runtime_error("Assigning nullptr to ref_view"));
    _ptr = ptr;
    return *this;
  }

  template<typename U>
  requires(std::is_convertible_v<U*, T*>)
  constexpr ref_view& operator=(U& obj) noexcept {
    _ptr = std::addressof(obj);
    return *this;
  }

  template<typename U>
  requires(std::is_convertible_v<U*, T*>)
  constexpr ref_view& operator=(const ref_view<U>& other) noexcept {
    _ptr = other.ptr();
    return *this;
  }

  constexpr ref_view& operator=(const ref_view&) noexcept = default;
  constexpr ref_view& operator=(ref_view&&) noexcept = default;

public:
  operator reference() const noexcept { return get(); }

private:
  T* _ptr;
};

} // namespace ntf
