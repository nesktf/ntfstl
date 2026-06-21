#ifndef NTF_MEMORY_HPP_
#define NTF_MEMORY_HPP_

#include "./core.hpp"

#include <memory>

namespace ntf {

namespace impl {

template<bool valid, typename T, typename... Args>
constexpr void
rebind_nullable(T& obj, Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>) {
  static_assert(std::is_nothrow_destructible_v<T>, "T has to be nothrow destructible");
  if constexpr (valid) {
    // If is obj a constructed object
    if constexpr (std::is_nothrow_constructible_v<T>) {
      addressof(obj)->~T();
      NTF_PNEW(addressof(obj)) T(forward<Args>(args)...);
    } else {
      T old(move(obj)); // Might throw
      addressof(obj)->~T();
#ifdef __cpp_exceptions
      try {
#endif
        NTF_PNEW(addressof(obj)) T(forward<Args>(args)...);
#ifdef __cpp_exceptions
      } catch (...) {
        NTF_PNEW(addressof(obj)) T(move(old));
        throw;
      }
#endif
    }
  } else {
    NTF_PNEW(addressof(obj)) T(forward<Args>(args)...); // Might throw
  }
}

template<typename T>
requires(std::is_default_constructible_v<T>)
constexpr T* construct_array(T* ptr,
                             size_t n) noexcept(std::is_nothrow_default_constructible_v<T>) {
  if constexpr (std::is_nothrow_default_constructible_v<T>) {
    for (size_t i = 0; i < n; ++i) {
      NTF_PNEW(ptr + i) T();
    }
  } else {
    i64 i = 0;
    DeferFn on_err = [&]() {
      for (; i >= 0; --i) {
        (ptr + i)->~T();
      }
    };
    for (; i < (i64)n; ++i) {
      NTF_PNEW(ptr + i) T();
    }
    on_err.disengage();
  }
  return std::launder(ptr);
}

template<typename T>
requires(std::is_copy_constructible_v<T>)
constexpr T* construct_array(T* ptr, size_t n,
                             const T& obj) noexcept(std::is_nothrow_copy_constructible_v<T>) {
  if constexpr (std::is_nothrow_copy_constructible_v<T>) {
    for (size_t i = 0; i < n; ++i) {
      NTF_PNEW(ptr + i) T();
    }
  } else {
    i64 i = 0;
    DeferFn on_err = [&]() {
      for (; i >= 0; --i) {
        (ptr + i)->~T();
      }
    };
    for (; i < (i64)n; ++i) {
      NTF_PNEW(ptr + i) T(obj);
    }
    on_err.disengage();
  }
  return std::launder(ptr);
}

} // namespace impl

template<size_t Size, size_t Align>
struct AlignedBuffer {
public:
  using size_type = size_t;

  static constexpr size_type BufferSize = Size;
  static constexpr size_type BufferAlign = Align;

public:
  AlignedBuffer() = default;

public:
  template<typename T>
  T* as() {
    return reinterpret_cast<T*>(data);
  }

  template<typename T>
  const T* as() const {
    return reinterpret_cast<const T*>(data);
  }

  template<typename T>
  T* as(size_type i) {
    return reinterpret_cast<T*>(data) + i;
  }

  template<typename T>
  const T* as(size_type i) const {
    return reinterpret_cast<const T*>(data) + i;
  }

  template<typename T>
  T* launder_as() {
    return std::launder(as<T>());
  }

  template<typename T>
  const T* launder_as() const {
    return std::launder(as<T>());
  }

  template<typename T>
  T* launder_as(size_type i) {
    return std::launder(as<T>(i));
  }

  template<typename T>
  const T* launder_as(size_type i) const {
    return std::launder(as<T>(i));
  }

public:
  template<typename T, typename... Args>
  T& construct(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>) {
    return *(NTF_PNEW(as<T>()) T(std::forward<Args>(args)...));
  }

  template<typename T, typename... Args>
  T& construct_offset(size_t offset,
                      Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>) {
    return *(NTF_PNEW(as<T>() + offset) T(std::forward<Args>(args)...));
  }

  template<typename T>
  void destroy() {
    launder_as<T>()->~T();
  }

  template<typename T>
  void destroy_offset(size_t idx) {
    launder_as<T>(idx)->~T();
  }

public:
  alignas(Align) u8 data[Size];
};

template<typename T>
class TypeBuf : private AlignedBuffer<sizeof(T), alignof(T)> {
private:
  using Base = AlignedBuffer<sizeof(T), alignof(T)>;

public:
  template<typename U = T>
  static constexpr bool check_params() noexcept {
    return (alignof(U) <= Base::BufferAlign) && (sizeof(U) == Base::BufferSize);
  }

public:
  constexpr TypeBuf() noexcept = default;

public:
  template<typename... Args>
  T& construct(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>) {
    return Base::template construct<T>(std::forward<Args>(args)...);
  }

  void destroy() noexcept { Base::template destroy<T>(); }

public:
  T* data() noexcept { return Base::template launder_as<T>(); }

  const T* data() const noexcept { return Base::template launder_as<T>(); }

  T* raw_data() noexcept { return Base::template as<T>(); }

  const T* raw_data() const noexcept { return Base::template as<T>(); }

  u8* as_bytes() noexcept { return Base::data; }

  const u8* as_bytes() const noexcept { return Base::data; }

public:
  T& operator*() noexcept { return *data(); }

  const T& operator*() const noexcept { return *data(); }

  T* operator->() noexcept { return data(); }

  const T* operator->() const noexcept { return data(); }
};

template<typename T, size_t N>
class TypeArrayBuf : private AlignedBuffer<N * sizeof(T), alignof(T)> {
private:
  static_assert(N > 0, "N has to be at least 1");
  using Base = AlignedBuffer<N * sizeof(T), alignof(T)>;

public:
  using size_type = typename Base::size_type;

  using iterator = T*;
  using const_iterator = const T*;

  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

public:
  constexpr TypeArrayBuf() noexcept = default;

public:
  template<typename... Args>
  T& construct(size_type i, Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>) {
    return Base::template construct_offset<T>(i, std::forward<Args>(args)...);
  }

  void destroy(size_type i) noexcept { Base::template destroy_offset<T>(i); }

public:
  constexpr size_type size() const noexcept { return N; }

  T* data() noexcept { return Base::template launder_as<T>(); }

  const T* data() const noexcept { return Base::template launder_as<T>(); }

  T* raw_data() noexcept { return Base::template as<T>(); }

  const T* raw_data() const noexcept { return Base::template as<T>(); }

  u8* as_bytes() noexcept { return Base::data; }

  const u8* as_bytes() const noexcept { return Base::data; }

public:
  T& at(size_type idx) {
    NTF_THROW_IF(idx >= size(), std::out_of_range("Index out of range in TypeArrayBuf, was " +
                                                  std::to_string(idx)));
    return data()[idx];
  }

  const T& at(size_type idx) const {
    NTF_THROW_IF(idx >= size(), std::out_of_range("Index out of range in TypeArrayBuf, was " +
                                                  std::to_string(idx)));
    return data()[idx];
  }

  T& operator[](size_type idx) {
    NTF_ASSERT(idx < size(), "Index out of range In TypeArrayBuf");
    return data()[idx];
  }

  const T& operator[](size_type idx) const {
    NTF_ASSERT(idx < size(), "Index out of range In TypeArrayBuf");
    return data()[idx];
  }

public:
  iterator begin() noexcept { return data(); }

  const_iterator begin() const noexcept { return data(); }

  const_iterator cbegin() const noexcept { return data(); }

  iterator end() noexcept { return data() + size(); }

  const_iterator end() const noexcept { return data() + size(); }

  const_iterator cend() const noexcept { return data() + size(); }

  reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }

  const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }

  const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(end()); }

  reverse_iterator rend() noexcept { return reverse_iterator(begin()); }

  const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }

  const_reverse_iterator crend() const noexcept { return const_reverse_iterator(begin()); }
};

template<typename Allocator>
class AllocatorDelete : private Allocator {
public:
  using value_type = std::allocator_traits<Allocator>::value_type;
  using pointer = std::allocator_traits<Allocator>::pointer;
  using size_type = std::allocator_traits<Allocator>::size_type;
  using difference_type = std::allocator_traits<Allocator>::difference_type;

public:
  template<typename U>
  using rebind = AllocatorDelete<U>;

public:
  constexpr AllocatorDelete() noexcept(std::is_nothrow_default_constructible_v<Allocator>) :
      Allocator() {}

  constexpr AllocatorDelete(const Allocator& alloc) noexcept(
    std::is_nothrow_copy_constructible_v<Allocator>) : Allocator(alloc) {}

public:
  constexpr AllocatorDelete(const AllocatorDelete&) noexcept(
    std::is_nothrow_copy_constructible_v<Allocator>) = default;
  constexpr ~AllocatorDelete() noexcept = default;

  template<typename U>
  requires(!std::same_as<value_type, U>)
  constexpr AllocatorDelete(const rebind<U>& other) noexcept(
    std::is_nothrow_copy_constructible_v<Allocator>) : Allocator(other.allocator()) {}

public:
  template<typename U = value_type>
  requires(std::convertible_to<pointer, U*>)
  constexpr void operator()(U* ptr) noexcept {
    static_assert(!std::is_void_v<value_type>, "Can't delete incomplete type");
    static_assert(sizeof(value_type), "Can't delete incomplete type");
    static_assert(std::is_nothrow_destructible_v<value_type>, "Can't delete throwing type");
    if constexpr (!std::is_trivially_destructible_v<value_type>) {
      ptr->~value_type();
    }
    Allocator::deallocate(ptr, 1);
  }

  template<typename U = value_type>
  requires(std::convertible_to<pointer, U*>)
  constexpr void operator()(U* ptr, size_type n) noexcept {
    static_assert(!std::is_void_v<value_type>, "Can't delete incomplete type");
    static_assert(sizeof(value_type), "Can't delete incomplete type");
    static_assert(std::is_nothrow_destructible_v<value_type>, "Can't delete throwing type");
    if constexpr (!std::is_trivially_destructible_v<value_type>) {
      for (size_type i = 0; i < n; ++i) {
        (ptr + n)->~value_type();
      }
    }
    Allocator::deallocate(ptr, n);
  }

public:
  Allocator& allocator() noexcept { return static_cast<Allocator&>(*this); }

  const Allocator& allocator() const noexcept { return static_cast<const Allocator&>(*this); }

public:
  template<typename U>
  constexpr bool operator==(const rebind<U>&) noexcept {
    return true;
  }

  template<typename U>
  constexpr bool operator!=(const rebind<U>&) noexcept {
    return false;
  }
};

} // namespace ntf

#endif // NTF_MEMORY_HPP_
