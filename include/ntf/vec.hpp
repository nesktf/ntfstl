#ifndef NTF_VEC_HPP
#define NTF_VEC_HPP

#include <ntf/memory.hpp>
#include <ntf/impl/iterator.hpp>

#ifndef NTF_NO_STD
#include <initializer_list>
#endif

namespace ntf {

template<typename T, size_t N>
class InplaceVec {
public:
  static_assert(N > 0, "Invalid size");
  using value_type = T;
  using size_type = size_t;
  using difference_type = ptrdiff_t;
  using iterator = T*;
  using const_iterator = const T*;
  using reverse_iterator = impl::reverse_iter_wrap<iterator>;
  using const_reverse_iterator = impl::reverse_iter_wrap<const_iterator>;

public:
  InplaceVec() noexcept : _size() {}

#ifndef NTF_NO_STD
  InplaceVec(std::initializer_list<T> il) : _size(il.size()) {
    size_type i = 0;
    for (const auto& v : il) {
      _construct(i++, v);
    }
  }

  template<typename It>
  InplaceVec(It first, It last)
  requires(std::input_iterator<meta::remove_cvref_t<It>>)
      : _size() {
    for (; first != last; ++first, ++_size) {
      _construct(_size, *first);
    }
  }
#endif

  InplaceVec(size_type n, const T& val) noexcept(meta::nothrow_copy_constructible<T>) :
      _size(n >= capacity() ? capacity() - 1 : n) {
    for (size_type i = 0; i < _size; ++i) {
      _construct(i, val);
    }
  }

  InplaceVec(const InplaceVec& other) noexcept
  requires(meta::trivially_copy_constructible<T>)
  = default;

  InplaceVec(InplaceVec&& other) noexcept
  requires(meta::trivially_move_constructible<T>)
  = default;

  InplaceVec(const InplaceVec& other) noexcept(meta::nothrow_copy_constructible<T>)
  requires(!meta::trivially_copy_constructible<T>)
      : _size(other.size()) {
    for (size_type i = 0; i < other.size(); ++i) {
      _construct(i, other[i]);
    }
  }

  InplaceVec(InplaceVec&& other) noexcept(meta::nothrow_move_constructible<T>)
  requires(!meta::trivially_move_constructible<T>)
      : _size(other.size()) {
    for (size_type i = 0; i < other.size(); ++i) {
      _construct(i, move(other[i]));
    }
  }

  ~InplaceVec()
  requires(!meta::trivially_destructible<T>)
  {
    _destroy_range(begin(), end());
  }

  ~InplaceVec()
  requires(meta::trivially_destructible<T>)
  = default;

public:
  // Im lazy
  InplaceVec& operator=(const InplaceVec&) = delete;
  InplaceVec& operator=(InplaceVec&&) = delete;

public:
  template<typename... Args>
  T& emplace_back(Args&&... args) {
    NTF_THROW_IF(_size == capacity(), BadAlloc());
    return *_construct(_size++, forward<Args>(args)...);
  }

  T& push_back(const T& obj) { return emplace_back(obj); }

  T& push_back(T&& obj) { return emplace_back(move(obj)); }

  template<typename... Args>
  T& unchecked_emplace_back(Args&&... args) {
    NTF_ASSERT(_size < capacity());
    return *_construct(_size++, forward<Args>(args)...);
  }

  T& unchecked_push_back(const T& obj) { return unchecked_emplace_back(obj); }

  T& unchecked_push_back(T&& obj) { return unchecked_emplace_back(move(obj)); }

  template<typename... Args>
  T* try_emplace_back(Args&&... args) {
    if (_size == capacity()) {
      return nullptr;
    }
    return _construct(_size++, forward<Args>(args)...);
  }

  T* try_push_back(const T& obj) { return try_emplace_back(obj); }

  T* try_push_back(T&& obj) { return try_emplace_back(move(obj)); }

  void pop_back() {
    if (empty()) {
      return;
    }
    if constexpr (!meta::trivially_destructible<T>) {
      (_ptr() + _size)->~T();
    }
    _size--;
  }

  void clear() {
    if constexpr (!meta::trivially_destructible<T>) {
      _destroy_range(begin(), end());
    }
    _size = 0;
  }

  static void reserve(size_type n) { NTF_ASSERT(n < capacity()); }

  static void shrink_to_fit() noexcept {}

  void resize(size_type n, const T& val) {
    if (n == _size) {
      return;
    }

    if (n < _size) {
      if constexpr (!meta::trivially_destructible<T>) {
        _destroy_range(begin() + n, begin() + _size);
      }
      _size = n;
    } else {
      n = n >= capacity() ? capacity() - 1 : n;
      for (size_t i = _size; i < n; ++i) {
        _construct(i, val);
      }
    }
  }

  void resize(size_type n) {
    if (n == _size) {
      return;
    }

    if (n < _size) {
      if constexpr (!meta::trivially_destructible<T>) {
        _destroy_range(begin() + n, begin() + _size);
      }
      _size = n;
    } else {
      n = n >= capacity() ? capacity() - 1 : n;
      for (size_t i = _size; i < n; ++i) {
        _construct(i);
      }
    }
  }

public:
  T& back() {
    NTF_ASSERT(!empty());
    return _ptr()[_size - 1];
  }

  T& front() {
    NTF_ASSERT(!empty());
    return *_ptr();
  }

  const T& back() const {
    NTF_ASSERT(!empty());
    return _ptr()[_size - 1];
  }

  const T& front() const {
    NTF_ASSERT(!empty());
    return *_ptr();
  }

  T& operator[](size_type i) {
    NTF_ASSERT(i < _size);
    return _ptr()[i];
  }

  const T& operator[](size_type i) const {
    NTF_ASSERT(i < _size);
    return _ptr()[i];
  }

  T& at(size_type i) {
    NTF_THROW_IF(i >= size(), BadAlloc());
    return _ptr()[i];
  }

  const T& at(size_type i) const {
    NTF_THROW_IF(i >= size(), BadAlloc());
    return _ptr()[i];
  }

public:
  size_type size() const noexcept { return _size; }

  bool empty() const noexcept { return _size == 0; }

  static constexpr size_type capacity() noexcept { return N; }

  static constexpr size_type max_size() noexcept { return N; }

public:
  iterator begin() { return _ptr(); }

  const_iterator begin() const { return _ptr(); }

  const_iterator cbegin() const { return _ptr(); }

  reverse_iterator rbegin() { return _ptr(); }

  const_reverse_iterator rbegin() const { return _ptr(); }

  const_reverse_iterator crbegin() const { return _ptr(); }

  iterator end() { return _ptr() + _size; }

  const_iterator end() const { return _ptr() + _size; }

  const_iterator cend() const { return _ptr() + _size; }

  reverse_iterator rend() { return _ptr() + _size; }

  const_reverse_iterator rend() const { return _ptr() + _size; }

  const_reverse_iterator crend() const { return _ptr() + _size; }

private:
  static void _destroy_range(iterator first, iterator last) {
    for (; first != last; ++first) {
      first->~T();
    }
  }

  template<typename... Args>
  T* _construct(size_type i, Args&&... args) {
    NTF_ASSERT(i < capacity());
    return NTF_PNEW(reinterpret_cast<T*>(_data) + i) T(forward<Args>(args)...);
  }

  T* _ptr() { return launder(reinterpret_cast<T*>(_data)); }
  const T* _ptr() const { return launder(reinterpret_cast<const T*>(_data)); }

private:
  alignas(T) uint8_t _data[sizeof(T) * N];
  size_type _size;
};

} // namespace ntf

#endif // NTF_VEC_HPP
