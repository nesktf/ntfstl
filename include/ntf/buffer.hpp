#ifndef NTF_BUFFER_HPP_
#define NTF_BUFFER_HPP_

#include <ntf/impl/iterator.hpp>
#include <ntf/memory.hpp>

namespace ntf {

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
    return launder(as<T>());
  }

  template<typename T>
  const T* launder_as() const {
    return launder(as<T>());
  }

  template<typename T>
  T* launder_as(size_type i) {
    return launder(as<T>(i));
  }

  template<typename T>
  const T* launder_as(size_type i) const {
    return launder(as<T>(i));
  }

public:
  template<typename T, typename... Args>
  T& construct(Args&&... args) noexcept(meta::nothrow_constructible<T, Args...>) {
    return *(NTF_PNEW(as<T>()) T(::ntf::forward<Args>(args)...));
  }

  template<typename T, typename... Args>
  T& construct_offset(size_t offset,
                      Args&&... args) noexcept(meta::nothrow_constructible<T, Args...>) {
    return *(NTF_PNEW(as<T>() + offset) T(::ntf::forward<Args>(args)...));
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
  T& construct(Args&&... args) noexcept(meta::nothrow_constructible<T, Args...>) {
    return Base::template construct<T>(::ntf::forward<Args>(args)...);
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

  using reverse_iterator = impl::reverse_iter_wrap<iterator>;
  using const_reverse_iterator = impl::reverse_iter_wrap<const_iterator>;

public:
  constexpr TypeArrayBuf() noexcept = default;

public:
  template<typename... Args>
  T& construct(size_type i, Args&&... args) noexcept(meta::nothrow_constructible<T, Args...>) {
    return Base::template construct_offset<T>(i, ::ntf::forward<Args>(args)...);
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
    NTF_THROW_IF(idx >= size(), MsgException("Index out of range in TypeArrayBuf"));
    return data()[idx];
  }

  const T& at(size_type idx) const {
    NTF_THROW_IF(idx >= size(), MsgException("Index out of range in TypeArrayBuf"));
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

} // namespace ntf

#endif // NTF_BUFFER_HPP_
