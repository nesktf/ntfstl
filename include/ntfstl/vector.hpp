#pragma once

#include <ntfstl/meta.hpp>
#include <ntfstl/span.hpp>
#include <ntfstl/types.hpp>

#include <ntfstl/memory.hpp>

#include <vector>

namespace ntf {

namespace impl {

template<typename T, size_t Size>
struct inplace_vec_data {
  alignas(T[Size]) u8 _buffer[Size * sizeof(T)];

  const T* data() const noexcept { return reinterpret_cast<const T*>(&_buffer[0]); }

  T* data() noexcept { return reinterpret_cast<T*>(&_buffer[0]); }
};

template<typename T>
void vec_construct_range(T* data,
                         size_t count) noexcept(std::is_nothrow_default_constructible_v<T>) {
  for (size_t i = 0; i < count; ++i) {
    if constexpr (!std::is_nothrow_default_constructible_v<T> &&
                  !std::is_trivially_destructible_v<T>) {
      try {
        new (data + i) T();
      } catch (...) {
        std::destroy_n(data, i);
        NTF_RETHROW();
      }
    } else {
      new (data + i) T();
    }
  }
}

template<typename T>
void vec_construct_range(T* data, size_t count,
                         const T& copy) noexcept(std::is_nothrow_copy_constructible_v<T>) {
  for (size_t i = 0; i < count; ++i) {
    if constexpr (!std::is_nothrow_default_constructible_v<T> &&
                  !std::is_trivially_destructible_v<T>) {
      try {
        new (data + i) T();
      } catch (...) {
        std::destroy_n(data, i);
        NTF_RETHROW();
      }
    } else {
      new (data + i) T(copy);
    }
  }
}

template<typename T, typename InputIt>
void vec_construct_range_it(T* data, InputIt first,
                            InputIt last) noexcept(std::is_nothrow_copy_constructible_v<T>) {
  size_t i = 0;
  for (InputIt it = first; it != last; ++it, ++i) {
    if constexpr (!std::is_nothrow_default_constructible_v<T> &&
                  !std::is_trivially_destructible_v<T>) {
      try {
        new (data + i) T();
      } catch (...) {
        std::destroy_n(data, i);
        NTF_RETHROW();
      }
    } else {
      new (data + i) T(*it);
    }
  }
}

template<typename T, typename InputIt>
void vec_construct_range_truncate(T* data, size_t count, InputIt first,
                                  InputIt last) noexcept(std::is_nothrow_copy_constructible_v<T>) {
  size_t i = 0;
  for (InputIt it = first; it != last && i < count; ++it, ++i) {
    if constexpr (!std::is_nothrow_default_constructible_v<T> &&
                  !std::is_trivially_destructible_v<T>) {
      try {
        new (data + i) T();
      } catch (...) {
        std::destroy_n(data, i);
        NTF_RETHROW();
      }
    } else {
      new (data + i) T(*it);
    }
  }
}

template<size_t InnerCap, typename T>
void vec_array_move_assign(T* dst, T* src, size_t dst_sz,
                           size_t src_sz) noexcept(std::is_nothrow_move_constructible_v<T>) {
  static_assert(std::is_nothrow_destructible_v<T>);

  if constexpr (std::is_nothrow_move_constructible_v<T>) {
    if constexpr (!std::is_trivially_destructible_v<T>) {
      std::destroy_n(dst, dst_sz);
    }
    if constexpr (std::is_trivially_move_constructible_v<T>) {
      std::memcpy(dst, src, src_sz * sizeof(T));
    } else {
      for (size_t i = 0; i < src_sz; ++i) {
        new (dst + i) T(std::move(src[i]));
      }
    }
  } else {
    // Try to copy in a temporary buffer first
    alignas(T[InnerCap]) u8 buffer_data[InnerCap * sizeof(T)];
    T* buffer = reinterpret_cast<T*>(&buffer_data[0]);
    ::ntf::impl::vec_construct_range_it(buffer, src, src + src_sz);

    if constexpr (!std::is_trivially_destructible_v<T>) {
      std::destroy_n(dst, dst_sz);
    }
  }
  // TBD
}

template<typename T, typename Alloc>
class vec_alloc : private Alloc {
public:
  vec_alloc() noexcept(std::is_nothrow_default_constructible_v<Alloc>)
  requires(std::is_default_constructible_v<Alloc>)
      : Alloc() {}

  vec_alloc(const Alloc& alloc) noexcept(std::is_nothrow_copy_constructible_v<Alloc>) :
      Alloc(alloc) {}

public:
  T* allocate(size_t n) { return Alloc::allocate(n); }

  void deallocate(T* ptr, size_t n) noexcept { Alloc::deallocate(ptr, n); }

public:
  Alloc& get_allocator() noexcept { return static_cast<Alloc&>(*this); }

  const Alloc& get_allocator() const noexcept { return static_cast<const Alloc&>(*this); }
};

} // namespace impl

template<typename T, ::ntf::meta::allocator_type_of<T> Alloc = ::ntf::default_alloc<T>>
class vec : private ::ntf::impl::vec_alloc<T, Alloc> {
public:
  using value_type = T;
  using allocator_type = Alloc;
  using size_type = size_t;
  using difference_type = ptrdiff_t;

  using reference = value_type&;
  using const_reference = const value_type&;
  using pointer = value_type*;
  using const_pointer = const value_type*;

  using iterator = value_type*;
  using const_iterator = const value_type*;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  static_assert(std::is_nothrow_destructible_v<value_type>, "T should be nothrow destructible");

private:
  using alloc_t = ::ntf::impl::vec_alloc<T, Alloc>;

public:
  vec() noexcept(std::is_nothrow_default_constructible_v<allocator_type>)
  requires(std::is_default_constructible_v<allocator_type>)
      : alloc_t(), _data(), _size(), _capacity() {}

  vec(const allocator_type& alloc) noexcept(std::is_nothrow_copy_constructible_v<allocator_type>) :
      alloc_t(alloc), _data(), _size(), _capacity() {}

  explicit vec(size_type n)
  requires(std::is_default_constructible_v<allocator_type> &&
           std::is_default_constructible_v<value_type>)
      : alloc_t(), _size(n), _capacity(n) {
    _data = alloc_t::allocate(size());
    if constexpr (!std::is_trivially_default_constructible_v<value_type>) {
      if constexpr (!std::is_nothrow_default_constructible_v<value_type>) {
        ::ntf::impl::vec_construct_range(data(), size());
      } else {
        try {
          ::ntf::impl::vec_construct_range(data(), size());
        } catch (...) {
          alloc_t::deallocate(_data, size());
          NTF_RETHROW();
        }
      }
    }
  }

  explicit vec(size_type n, const allocator_type& alloc)
  requires(std::is_default_constructible_v<value_type>)
      : alloc_t(alloc), _size(n), _capacity(n) {
    _data = alloc_t::allocate(size());
    if constexpr (!std::is_trivially_default_constructible_v<value_type>) {
      if constexpr (!std::is_nothrow_default_constructible_v<value_type>) {
        ::ntf::impl::vec_construct_range(data(), size());
      } else {
        try {
          ::ntf::impl::vec_construct_range(data(), size());
        } catch (...) {
          alloc_t::deallocate(_data, size());
          NTF_RETHROW();
        }
      }
    }
  }

  vec(size_type n, const value_type& val)
  requires(std::is_default_constructible_v<allocator_type> &&
           std::is_copy_constructible_v<value_type>)
      : alloc_t(), _size(n), _capacity(n) {
    _data = alloc_t::allocate(size());
    if constexpr (!std::is_nothrow_copy_constructible_v<value_type>) {
      ::ntf::impl::vec_construct_range(data(), size(), val);
    } else {
      try {
        ::ntf::impl::vec_construct_range(data(), size(), val);
      } catch (...) {
        alloc_t::deallocate(_data, size());
        NTF_RETHROW();
      }
    }
  }

  vec(size_type n, const value_type& val, const allocator_type& alloc)
  requires(std::is_copy_constructible_v<value_type>)
      : alloc_t(alloc), _size(n), _capacity(n) {
    _data = alloc_t::allocate(size());
    if constexpr (!std::is_nothrow_copy_constructible_v<value_type>) {
      ::ntf::impl::vec_construct_range(data(), size(), val);
    } else {
      try {
        ::ntf::impl::vec_construct_range(data(), size(), val);
      } catch (...) {
        alloc_t::deallocate(_data, size());
        NTF_RETHROW();
      }
    }
  }

  vec(std::initializer_list<value_type> il)
  requires(std::is_default_constructible_v<allocator_type> &&
           std::is_copy_constructible_v<value_type>)
      : alloc_t(), _size(il.size()), _capacity(il.size()) {
    _data = alloc_t::allocate(size());
    if constexpr (!std::is_nothrow_copy_constructible_v<value_type>) {
      ::ntf::impl::vec_construct_range_it(data(), il.begin(), il.end());
    } else {
      try {
        ::ntf::impl::vec_construct_range_it(data(), il.begin(), il.end());
      } catch (...) {
        alloc_t::deallocate(_data, size());
        NTF_RETHROW();
      }
    }
  }

  vec(std::initializer_list<value_type> il, const allocator_type& alloc)
  requires(std::is_copy_constructible_v<value_type>)
      : alloc_t(alloc), _size(il.size()), _capacity(il.size()) {
    _data = alloc_t::allocate(size());
    if constexpr (!std::is_nothrow_copy_constructible_v<value_type>) {
      ::ntf::impl::vec_construct_range_it(data(), il.begin(), il.end());
    } else {
      try {
        ::ntf::impl::vec_construct_range_it(data(), il.begin(), il.end());
      } catch (...) {
        alloc_t::deallocate(_data, size());
        NTF_RETHROW();
      }
    }
  }

  template<typename InputIt>
  vec(InputIt first, InputIt last)
  requires(std::is_default_constructible_v<allocator_type> &&
           std::is_copy_constructible_v<value_type>)
      : alloc_t(), _size(std::distance(first, last)), _capacity(_size) {
    _data = alloc_t::allocate(size());
    if constexpr (!std::is_nothrow_copy_constructible_v<value_type>) {
      ::ntf::impl::vec_construct_range_it(data(), first, last);
    } else {
      try {
        ::ntf::impl::vec_construct_range_it(data(), first, last);
      } catch (...) {
        alloc_t::deallocate(_data, size());
        NTF_RETHROW();
      }
    }
  }

  template<std::input_iterator InputIt>
  vec(InputIt first, InputIt last, const allocator_type& alloc)
  requires(std::is_copy_constructible_v<value_type>)
      : alloc_t(alloc), _size(std::distance(first, last)), _capacity(_size) {
    _data = alloc_t::allocate(size());
    if constexpr (!std::is_nothrow_copy_constructible_v<value_type>) {
      ::ntf::impl::vec_construct_range_it(data(), first, last);
    } else {
      try {
        ::ntf::impl::vec_construct_range_it(data(), first, last);
      } catch (...) {
        alloc_t::deallocate(_data, size());
      }
    }
  }

public:
  vec(vec&& other) noexcept(std::is_nothrow_default_constructible_v<allocator_type>)
  requires(std::is_default_constructible_v<allocator_type>)
      : alloc_t(), _data(other._data), _size(other._size), _capacity(other._capacity) {
    other._data = nullptr;
    other._size = 0;
    other._capacity = 0;
  }

  vec(vec&& other) noexcept(std::is_nothrow_copy_constructible_v<allocator_type>)
  requires(!std::is_default_constructible_v<allocator_type>)
      :
      alloc_t(other.get_allocator()), _data(other._data), _size(other._size),
      _capacity(other._capacity) {
    other._data = nullptr;
    other._size = 0;
    other._capacity = 0;
  }

  vec(vec&& other,
      const allocator_type& alloc) noexcept(std::is_nothrow_copy_constructible_v<allocator_type>) :
      alloc_t(alloc), _data(other._data), _size(other._size), _capacity(other._capacity) {
    other._data = nullptr;
    other._size = 0;
    other._capacity = 0;
  }

  vec(const vec& other)
  requires(std::is_default_constructible_v<allocator_type>)
      : alloc_t(), _data(other._data), _size(other._size), _capacity(other._capacity) {
    _copy_construct(other);
  }

  vec(const vec& other)
  requires(!std::is_default_constructible_v<allocator_type>)
      :
      alloc_t(other.get_allocator()), _data(other._data), _size(other._size),
      _capacity(other._capacity) {
    _copy_construct(other);
  }

  vec(const vec& other, const allocator_type& alloc) :
      alloc_t(alloc), _data(other._data), _size(other._size), _capacity(other._capacity) {
    _copy_construct(other);
  }

  ~vec() noexcept {
    if (!empty()) {
      _do_destroy();
    }
  }

public:
  reference push_back(const value_type& obj) {
    // TBD
  }

  reference push_back(value_type&& obj) {
    // TBD
  }

  template<typename... Args>
  reference emplace_back(Args&&... args) {
    // TBD
  }

  void pop_back() {
    // TBD
  }

  void pop_front() {
    // TBD
  }

public:
  iterator insert(const_iterator pos, const T& value) {
    // TBD
  }

  iterator insert(const_iterator pos, T&& value) {
    // TBD
  }

  iterator insert(const_iterator pos, size_type n) {
    // TBD
  }

  iterator insert(const_iterator pos, size_type n, const T& value) {
    // TBD
  }

  template<std::input_iterator InputIt>
  iterator insert(const_iterator pos, InputIt first, InputIt last) {
    // TBD
  }

  iterator erase(iterator pos) {
    // TDB
  }

  iterator erase(const_iterator pos) {
    // TDB
  }

  iterator erase(iterator first, iterator last) {
    // TDB
  }

  iterator erase(const_iterator first, const_iterator end) {
    // TDB
  }

public:
  template<typename F>
  void erase_if(F&& pred) {
    static_assert(std::is_invocable_v<std::remove_cvref_t<F>, value_type&>,
                  "F must be invocable with T&");
    using ret_t = std::invoke_result_t<F, value_type&>;
    static_assert(std::convertible_to<ret_t, bool>, "F must return a boolean value");
    // TBD
  }

  void reserve(size_type n) {
    // TBD
  }

  void clear() noexcept {
    if (!std::is_trivially_destructible_v<value_type>) {
      std::destroy_n(data(), size());
    }
    _size = 0;
  }

  void resize(size_type n)
  requires(std::is_default_constructible_v<value_type>)
  {
    // TBD
  }

  void resize(size_type n, const value_type& val)
  requires(std::is_copy_constructible_v<value_type>)
  {
    // TBD
  }

public:
  vec&
  operator=(vec&& other) noexcept(::ntf::meta::nothrow_move_assignable_allocator<allocator_type>) {
    if (this == &other) {
      return *this;
    }

    if (!empty()) {
      _do_destroy();
    }

    _data = other._data;
    _size = other._size;
    _capacity = other._capacity;

    if constexpr (std::allocator_traits<
                    allocator_type>::propagate_on_container_move_assignment::value) {
      get_allocator() = std::move(other.get_allocator());
    }

    other._data = nullptr;
    other._size = 0;
    other._capacity = 0;

    return *this;
  }

  vec& operator=(const vec& other)
  requires(std::is_copy_constructible_v<value_type>)
  {
    // TBD
    return *this;
  }

  vec& operator=(std::initializer_list<value_type> il)
  requires(std::is_copy_constructible_v<value_type>)
  {
    return assign(il);
  }

  vec& assign(size_type count, const T& value)
  requires(std::is_copy_constructible_v<value_type>)
  {
    // TBD
    return *this;
  }

  vec& assign(size_type count)
  requires(std::is_default_constructible_v<value_type>)
  {
    // TBD
    return *this;
  }

  vec& assign(std::initializer_list<value_type> il)
  requires(std::is_copy_constructible_v<value_type>)
  {
    // TBD
    return *this;
  }

  template<std::input_iterator InputIt>
  vec& assign(InputIt first, InputIt last)
  requires(std::is_copy_constructible_v<value_type>)
  {
    // TBD
    return *this;
  }

public:
  const_pointer data() const noexcept { return _data; }

  pointer data() noexcept { return const_cast<pointer>(std::as_const(*this).data()); }

  const_reference at(size_type idx) const {
    NTF_THROW_IF(idx >= size(),
                 std::out_of_range(fmt::format("Index {} out of range ({})", idx, size())));
    return data()[idx];
  }

  reference at(size_type idx) { return const_cast<reference>(std::as_const(*this).at(idx)); }

  const_pointer at_opt(size_type idx) const noexcept {
    return idx >= size() ? nullptr : data()[idx];
  }

  pointer at_opt(size_type idx) noexcept {
    return const_cast<pointer>(std::as_const(*this).at_opt(idx));
  }

  const_reference operator[](size_type idx) const {
    NTF_ASSERT(idx < size());
    return data()[idx];
  }

  reference operator[](size_type idx) { return const_cast<reference>(std::as_const(*this)[idx]); }

  const_reference front() const {
    NTF_ASSERT(!empty());
    return data()[0];
  }

  reference front() { return const_cast<reference>(std::as_const(*this).front()); }

  const_reference back() const {
    NTF_ASSERT(!empty());
    return data()[size() - 1];
  }

  reference back() { return const_cast<reference>(std::as_const(*this).back()); }

public:
  constexpr size_type max_size() const noexcept {
    return (std::numeric_limits<difference_type>::max()) / sizeof(value_type);
  }

  size_type size() const noexcept { return _size; }

  size_type size_bytes() const noexcept { return size() * sizeof(value_type); }

  size_type capacity() const noexcept { return _capacity; }

  bool empty() const noexcept { return _size == 0; }

  span<value_type> as_span() noexcept { return {data(), size()}; }

  span<const value_type> as_span() const noexcept { return {data(), size()}; }

  allocator_type& get_allocator() noexcept { return alloc_t::get_allocator(); }

  const allocator_type& get_allocator() const noexcept { return alloc_t::get_allocator(); }

public:
  iterator begin() noexcept { return iterator(data()); }

  const_iterator begin() const noexcept { return const_iterator(data()); }

  const_iterator cbegin() const noexcept { return const_iterator(data()); }

  iterator end() noexcept { return iterator(data() + size()); }

  const_iterator end() const noexcept { return iterator(data() + size()); }

  const_iterator cend() const noexcept { return iterator(data() + size()); }

  reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }

  const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }

  const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(cend()); }

  reverse_iterator rend() noexcept { return reverse_iterator(begin()); }

  const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }

  const_reverse_iterator crend() const noexcept { return const_reverse_iterator(begin()); }

private:
  void _do_destroy() noexcept {
    if constexpr (!std::is_trivially_destructible_v<value_type>) {
      for (iterator it = begin(); it != end(); ++it) {
        std::destroy_at(&*it);
      }
    }
    alloc_t::deallocate(_data, capacity());
  }

  void _copy_construct(const vec& other) {
    _data = alloc_t::allocate(size());
    try {
      ::ntf::impl::vec_construct_range_it(data(), other.begin(), other.end());
    } catch (...) {
      alloc_t::deallocate(_data, size());
      NTF_RETHROW();
    }
  }

private:
  pointer _data;
  size_type _size;
  size_type _capacity;
};

template<typename T, size_t DataCap>
class inplace_vec {
public:
  using value_type = T;
  using size_type = size_t;
  using difference_type = ptrdiff_t;

  static constexpr size_type inner_capacity = static_cast<size_type>(DataCap);

  using reference = value_type&;
  using const_reference = const value_type&;
  using pointer = value_type*;
  using const_pointer = const value_type*;

  using iterator = value_type*;
  using const_iterator = const value_type*;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  static_assert(inner_capacity > 0, "DataCap should be greater than 0");
  static_assert(std::is_nothrow_destructible_v<value_type>, "T should be nothrow destructible");

public:
  inplace_vec() noexcept : _size() {}

  explicit inplace_vec(size_type n) noexcept(std::is_nothrow_default_constructible_v<value_type>)
  requires(std::is_default_constructible_v<value_type>)
      : _size(n) {
    if constexpr (!std::is_trivially_default_constructible_v<value_type>) {
      ::ntf::impl::vec_construct_range(data(), size());
    }
  }

  inplace_vec(size_type n, const value_type& val)
  requires(std::is_copy_constructible_v<value_type>)
      : _size(std::min(n, inner_capacity)) {
    ::ntf::impl::vec_construct_range(data(), size(), val);
  }

  inplace_vec(std::initializer_list<value_type> il)
  requires(std::is_copy_constructible_v<value_type>)
      : _size(std::min(il.size(), inner_capacity)) {
    ::ntf::impl::vec_construct_range_truncate(data(), size(), il.begin(), il.end());
  }

  template<std::input_iterator InputIt>
  inplace_vec(InputIt first, InputIt last)
  requires(std::is_copy_constructible_v<value_type>)
      : _size(std::min(std::distance(first, last), inner_capacity)) {
    ::ntf::impl::vec_construct_range_truncate(data(), size(), first, last);
  }

public:
  inplace_vec(inplace_vec&& other) noexcept
  requires(std::is_trivially_move_constructible_v<value_type>)
  = default;

  inplace_vec(inplace_vec&& other) noexcept(std::is_nothrow_move_constructible_v<value_type>)
  requires(std::is_move_constructible_v<value_type>)
      : _size(other._size) {
    ::ntf::impl::vec_construct_range_truncate(data(), size(), other.begin(), other.end());
  }

  inplace_vec(const inplace_vec& other) noexcept
  requires(std::is_trivially_copy_constructible_v<value_type>)
  = default;

  inplace_vec(const inplace_vec& other) noexcept(std::is_nothrow_copy_constructible_v<value_type>)
  requires(!std::is_trivially_copy_constructible_v<value_type> &&
           std::is_copy_constructible_v<value_type>)
      : _size(other._size) {
    ::ntf::impl::vec_construct_range_truncate(data(), size(), other.begin(), other.end());
  }

  ~inplace_vec() noexcept
  requires(!std::is_trivially_destructible_v<value_type>)
  {
    _do_destroy();
  }

  ~inplace_vec() noexcept
  requires(std::is_trivially_destructible_v<value_type>)
  = default;

public:
  reference push_back(const value_type& obj) {
    // TBD
  }

  reference push_back(value_type&& obj) {
    // TBD
  }

  template<typename... Args>
  reference emplace_back(Args&&... args) {
    // TBD
  }

  void pop_back() {
    // TBD
  }

  void pop_front() {
    // TBD
  }

public:
  iterator insert(const_iterator pos, const T& value) {
    // TBD
  }

  iterator insert(const_iterator pos, T&& value) {
    // TBD
  }

  iterator insert(const_iterator pos, size_type n) {
    // TBD
  }

  iterator insert(const_iterator pos, size_type n, const T& value) {
    // TBD
  }

  template<typename InputIt>
  iterator insert(const_iterator pos, InputIt first, InputIt last) {
    // TBD
  }

  iterator erase(iterator pos) {
    // TDB
  }

  iterator erase(const_iterator pos) {
    // TDB
  }

  iterator erase(iterator first, iterator last) {
    // TDB
  }

  iterator erase(const_iterator first, const_iterator end) {
    // TDB
  }

public:
  template<typename F>
  void erase_if(F&& pred) {
    static_assert(std::is_invocable_v<std::remove_cvref_t<F>, value_type&>,
                  "F must be invocable with T&");
    using ret_t = std::invoke_result_t<F, value_type&>;
    static_assert(std::convertible_to<ret_t, bool>, "F must return a boolean value");
    // TBD
  }

  void clear() noexcept {
    if (!std::is_trivially_destructible_v<value_type>) {
      std::destroy_n(data(), size());
    }
    _size = 0;
  }

  void resize(size_type n)
  requires(std::is_default_constructible_v<value_type>)
  {
    // TBD
  }

  void resize(size_type n, const value_type& val)
  requires(std::is_copy_constructible_v<value_type>)
  {
    // TBD
  }

public:
  inplace_vec&
  operator=(inplace_vec&& other) noexcept(std::is_nothrow_move_constructible_v<value_type>)
  requires(std::is_move_constructible_v<value_type> || std::is_copy_constructible_v<value_type>)
  {
    // TBD
    return *this;
  }

  inplace_vec&
  operator=(const inplace_vec&) noexcept(std::is_nothrow_copy_constructible_v<value_type>)
  requires(std::is_copy_constructible_v<value_type>)
  {
    // TBD
    return *this;
  }

  inplace_vec& operator=(std::initializer_list<value_type> il)
  requires(std::is_copy_constructible_v<value_type>)
  {
    return assign(il);
  }

  inplace_vec& assign(size_type count, const T& value)
  requires(std::is_copy_constructible_v<value_type>)
  {
    // TBD
    return *this;
  }

  inplace_vec& assign(size_type count)
  requires(std::is_default_constructible_v<value_type>)
  {
    // TBD
    return *this;
  }

  inplace_vec& assign(std::initializer_list<value_type> il)
  requires(std::is_copy_constructible_v<value_type>)
  {
    // TBD
    return *this;
  }

  template<std::input_iterator InputIt>
  inplace_vec& assign(InputIt first, InputIt last)
  requires(std::is_copy_constructible_v<value_type>)
  {
    // TBD
    return *this;
  }

public:
  const_pointer data() const noexcept { return _data.data(); }

  pointer data() noexcept { return _data.data(); }

  const_reference at(size_type idx) const {
    NTF_THROW_IF(idx >= size(),
                 std::out_of_range(fmt::format("Index {} out of range ({})", idx, size())));
    return data()[idx];
  }

  reference at(size_type idx) { return const_cast<reference>(std::as_const(*this).at(idx)); }

  const_pointer at_opt(size_type idx) const noexcept {
    return idx >= size() ? nullptr : data()[idx];
  }

  pointer at_opt(size_type idx) noexcept {
    return const_cast<pointer>(std::as_const(*this).at_opt(idx));
  }

  const_reference operator[](size_type idx) const {
    NTF_ASSERT(idx < size());
    return data()[idx];
  }

  reference operator[](size_type idx) { return const_cast<reference>(std::as_const(*this)[idx]); }

  const_reference front() const {
    NTF_ASSERT(!empty());
    return data()[0];
  }

  reference front() { return const_cast<reference>(std::as_const(*this).front()); }

  const_reference back() const {
    NTF_ASSERT(!empty());
    return data()[size() - 1];
  }

  reference back() { return const_cast<reference>(std::as_const(*this).back()); }

public:
  constexpr size_type max_size() const noexcept { return inner_capacity; }

  size_type size() const noexcept { return _size; }

  size_type size_bytes() const noexcept { return size() * sizeof(value_type); }

  size_type capacity() const noexcept { return inner_capacity; }

  bool empty() const noexcept { return _size == 0; }

  span<value_type> as_span() noexcept { return {data(), size()}; }

  span<const value_type> as_span() const noexcept { return {data(), size()}; }

public:
  iterator begin() noexcept { return iterator(data()); }

  const_iterator begin() const noexcept { return const_iterator(data()); }

  const_iterator cbegin() const noexcept { return const_iterator(data()); }

  iterator end() noexcept { return iterator(data() + size()); }

  const_iterator end() const noexcept { return iterator(data() + size()); }

  const_iterator cend() const noexcept { return iterator(data() + size()); }

  reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }

  const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }

  const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(cend()); }

  reverse_iterator rend() noexcept { return reverse_iterator(begin()); }

  const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }

  const_reverse_iterator crend() const noexcept { return const_reverse_iterator(begin()); }

private:
  void _do_destroy() noexcept {
    for (iterator it = begin(); it != end(); ++it) {
      std::destroy_at(&*it);
    }
  }

private:
  ::ntf::impl::inplace_vec_data<T, inner_capacity> _data;
  size_type _size;
};

template<typename T, size_t SmallCap,
         ::ntf::meta::allocator_type_of<T> Alloc = ::ntf::default_alloc<T>>
class small_vec : private ::ntf::impl::vec_alloc<T, Alloc> {
public:
  using value_type = T;
  using allocator_type = Alloc;
  using size_type = size_t;
  using difference_type = ptrdiff_t;

  static constexpr size_type inner_capacity = static_cast<size_type>(SmallCap);

  using reference = value_type&;
  using const_reference = const value_type&;
  using pointer = value_type*;
  using const_pointer = const value_type*;

  using iterator = value_type*;
  using const_iterator = const value_type*;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  static_assert(inner_capacity > 0, "SmallCap should be greater than 0");
  static_assert(std::is_nothrow_destructible_v<value_type>, "T should be nothrow destructible");

private:
  using alloc_t = ::ntf::impl::vec_alloc<T, Alloc>;

public:
  small_vec() noexcept(std::is_nothrow_default_constructible_v<allocator_type>)
  requires(std::is_default_constructible_v<allocator_type>)
      : alloc_t(), _alloc_data(), _size(), _capacity() {}

  small_vec(const allocator_type& alloc) noexcept(
    std::is_nothrow_copy_constructible_v<allocator_type>) :
      alloc_t(alloc), _alloc_data(), _size(), _capacity() {}

  explicit small_vec(size_type n)
  requires(std::is_default_constructible_v<allocator_type> &&
           std::is_default_constructible_v<value_type>)
      : alloc_t(), _size(n), _capacity(std::max(n, inner_capacity)) {
    if (!using_small()) {
      _alloc_data = alloc_t::allocate(size());
    }
    if constexpr (!std::is_trivially_default_constructible_v<value_type>) {
      if constexpr (std::is_nothrow_default_constructible_v<value_type>) {
        ::ntf::impl::vec_construct_range(data(), size());
      } else {
        try {
          ::ntf::impl::vec_construct_range(data(), size());
        } catch (...) {
          if (!using_small()) {
            alloc_t::deallocate(_alloc_data, size());
          }
          NTF_RETHROW();
        }
      }
    }
  }

  explicit small_vec(size_type n, const allocator_type& alloc)
  requires(std::is_default_constructible_v<value_type>)
      : alloc_t(alloc), _size(n), _capacity(std::max(n, inner_capacity)) {
    if (!using_small()) {
      _alloc_data = alloc_t::allocate(size());
    }
    if constexpr (!std::is_trivially_default_constructible_v<value_type>) {
      if constexpr (std::is_nothrow_default_constructible_v<value_type>) {
        ::ntf::impl::vec_construct_range(data(), size());
      } else {
        try {
          ::ntf::impl::vec_construct_range(data(), size());
        } catch (...) {
          if (!using_small()) {
            alloc_t::deallocate(_alloc_data, size());
          }
          NTF_RETHROW();
        }
      }
    }
  }

  small_vec(size_type n, const value_type& val)
  requires(std::is_default_constructible_v<allocator_type> &&
           std::is_copy_constructible_v<value_type>)
      : alloc_t(), _size(n), _capacity(std::max(n, inner_capacity)) {
    if (!using_small()) {
      _alloc_data = alloc_t::allocate(size());
    }
    if constexpr (std::is_nothrow_copy_constructible_v<value_type>) {
      ::ntf::impl::vec_construct_range(data(), size(), val);
    } else {
      try {
        ::ntf::impl::vec_construct_range(data(), size(), val);
      } catch (...) {
        if (!using_small()) {
          alloc_t::deallocate(_alloc_data, size());
        }
        NTF_RETHROW();
      }
    }
  }

  small_vec(size_type n, const value_type& val, const allocator_type& alloc)
  requires(std::is_copy_constructible_v<value_type>)
      : alloc_t(alloc), _size(n), _capacity(std::max(n, inner_capacity)) {
    if (!using_small()) {
      _alloc_data = alloc_t::allocate(n);
    }
    if constexpr (std::is_nothrow_copy_constructible_v<value_type>) {
      ::ntf::impl::vec_construct_range(data(), size(), val);
    } else {
      try {
        ::ntf::impl::vec_construct_range(data(), size(), val);
      } catch (...) {
        if (!using_small()) {
          alloc_t::deallocate(_alloc_data, size());
        }
        NTF_RETHROW();
      }
    }
  }

  small_vec(std::initializer_list<value_type> il)
  requires(std::is_default_constructible_v<allocator_type> &&
           std::is_copy_constructible_v<value_type>)
      : alloc_t(), _size(il.size()), _capacity(std::max(il.size(), inner_capacity)) {
    if (!using_small()) {
      _alloc_data = alloc_t::allocate(size());
    }
    if constexpr (std::is_nothrow_copy_constructible_v<value_type>) {
      ::ntf::impl::vec_construct_range_it(data(), il.begin(), il.end());
    } else {
      try {
        ::ntf::impl::vec_construct_range(data(), il.begin(), il.end());
      } catch (...) {
        if (!using_small()) {
          alloc_t::deallocate(_alloc_data, size());
        }
        NTF_RETHROW();
      }
    }
  }

  small_vec(std::initializer_list<value_type> il, const allocator_type& alloc)
  requires(std::is_copy_constructible_v<value_type>)
      : alloc_t(alloc), _size(il.size()), _capacity(std::max(il.size(), inner_capacity)) {
    if (!using_small()) {
      _alloc_data = alloc_t::allocate(size());
    }
    if constexpr (std::is_nothrow_copy_constructible_v<value_type>) {
      ::ntf::impl::vec_construct_range_it(data(), il.begin(), il.end());
    } else {
      try {
        ::ntf::impl::vec_construct_range(data(), il.begin(), il.end());
      } catch (...) {
        if (!using_small()) {
          alloc_t::deallocate(_alloc_data, size());
        }
        NTF_RETHROW();
      }
    }
  }

  template<typename InputIt>
  small_vec(InputIt first, InputIt last)
  requires(std::is_default_constructible_v<allocator_type> &&
           std::is_copy_constructible_v<value_type>)
      : alloc_t(), _size(std::distance(first, last)), _capacity(std::max(_size, inner_capacity)) {
    if (!using_small()) {
      _alloc_data = alloc_t::allocate(size());
    }
    if constexpr (std::is_nothrow_copy_constructible_v<value_type>) {
      ::ntf::impl::vec_construct_range_it(data(), first, last);
    } else {
      try {
        ::ntf::impl::vec_construct_range(data(), first, last);
      } catch (...) {
        if (!using_small()) {
          alloc_t::deallocate(_alloc_data, size());
        }
        NTF_RETHROW();
      }
    }
  }

  template<std::input_iterator InputIt>
  small_vec(InputIt first, InputIt last, const allocator_type& alloc)
  requires(std::is_copy_constructible_v<value_type>)
      :
      alloc_t(alloc), _size(std::distance(first, last)),
      _capacity(std::max(_size, inner_capacity)) {
    if (!using_small()) {
      _alloc_data = alloc_t::allocate(size());
    }
    if constexpr (std::is_nothrow_copy_constructible_v<value_type>) {
      ::ntf::impl::vec_construct_range_it(data(), first, last);
    } else {
      try {
        ::ntf::impl::vec_construct_range(data(), first, last);
      } catch (...) {
        if (!using_small()) {
          alloc_t::deallocate(_alloc_data, size());
        }
        NTF_RETHROW();
      }
    }
  }

public:
  small_vec(small_vec&& other) noexcept(std::is_nothrow_copy_constructible_v<allocator_type> &&
                                        std::is_nothrow_move_constructible_v<value_type>)
  requires(std::is_default_constructible_v<allocator_type>)
      : alloc_t(), _size(other.size()), _capacity(other._capacity) {
    _move_construct(other);
  }

  small_vec(small_vec&& other) noexcept(std::is_nothrow_copy_constructible_v<allocator_type> &&
                                        std::is_nothrow_move_constructible_v<value_type>)
  requires(!std::is_default_constructible_v<allocator_type>)
      : alloc_t(other.get_allocator()), _size(other.size()), _capacity(other._capacity) {
    _move_construct(other);
  }

  small_vec(small_vec&& other, const allocator_type& alloc) noexcept(
    std::is_nothrow_copy_constructible_v<allocator_type> &&
    std::is_nothrow_move_constructible_v<value_type>) :
      alloc_t(alloc), _size(other.size()), _capacity(other._capacity) {
    _move_construct(other);
  }

  small_vec(const small_vec& other)
  requires(std::is_default_constructible_v<allocator_type>)
      : alloc_t(), _size(other.size()), _capacity(other._capacity) {
    _copy_construct(other);
  }

  small_vec(const small_vec& other)
  requires(!std::is_default_constructible_v<allocator_type>)
      : alloc_t(other.get_allocator()), _size(other.size()), _capacity(other._capacity) {
    _copy_construct(other);
  }

  small_vec(const small_vec& other, const allocator_type& alloc) :
      alloc_t(alloc), _size(other.size()), _capacity(other._capacity) {
    _copy_construct(other);
  }

  ~small_vec() noexcept {
    if (!empty()) {
      _do_destroy();
    }
  }

public:
  reference push_back(const value_type& obj) {
    // TBD
  }

  reference push_back(value_type&& obj) {
    // TBD
  }

  template<typename... Args>
  reference emplace_back(Args&&... args) {
    // TBD
  }

  void pop_back() {
    // TBD
  }

  void pop_front() {
    // TBD
  }

public:
  iterator insert(const_iterator pos, const T& value) {
    // TBD
  }

  iterator insert(const_iterator pos, T&& value) {
    // TBD
  }

  iterator insert(const_iterator pos, size_type n) {
    // TBD
  }

  iterator insert(const_iterator pos, size_type n, const T& value) {
    // TBD
  }

  template<std::input_iterator InputIt>
  iterator insert(const_iterator pos, InputIt first, InputIt last) {
    // TBD
  }

  iterator erase(iterator pos) {
    // TDB
  }

  iterator erase(const_iterator pos) {
    // TDB
  }

  iterator erase(iterator first, iterator last) {
    // TDB
  }

  iterator erase(const_iterator first, const_iterator end) {
    // TDB
  }

public:
  template<typename F>
  void erase_if(F&& pred) {
    static_assert(std::is_invocable_v<std::remove_cvref_t<F>, value_type&>,
                  "F must be invocable with T&");
    using ret_t = std::invoke_result_t<F, value_type&>;
    static_assert(std::convertible_to<ret_t, bool>, "F must return a boolean value");
    // TBD
  }

  void reserve(size_type n) {
    // TBD
  }

  void clear() noexcept {
    if (!std::is_trivially_destructible_v<value_type>) {
      std::destroy_n(data(), size());
    }
    _size = 0;
  }

  void resize(size_type n)
  requires(std::is_default_constructible_v<value_type>)
  {
    // TBD
  }

  void resize(size_type n, const value_type& val)
  requires(std::is_copy_constructible_v<value_type>)
  {
    // TBD
  }

public:
  small_vec& operator=(small_vec& other) noexcept(
    ::ntf::meta::nothrow_move_assignable_allocator<allocator_type> &&
    std::is_nothrow_move_constructible_v<value_type>)
  requires(std::is_move_constructible_v<T> || std::is_copy_constructible_v<T>)
  {
    if (this == &other) {
      return *this;
    }

    // TBD
    // if (other.using_small()) {
    //   if constexpr (std::is_move_constructible_v<T>) {
    //     ::ntf::impl::vec_array_move_assign<inner_capacity>(data(), other._get_small(), size(),
    //                                                        other.size());
    //   } else {
    //   }
    // } else {
    //   if (!empty()) {
    //     _do_destroy();
    //   }
    //   _alloc_data = other._alloc_data;
    // }
    _capacity = other._capacity;
    _size = other._size;

    if constexpr (std::allocator_traits<
                    allocator_type>::propagate_on_container_move_assignment::value) {
      get_allocator() = std::move(other.get_allocator());
    }

    other._alloc_data = nullptr;
    other._size = 0;
    other._capacity = 0;

    return *this;
  }

  small_vec& operator=(const small_vec& other)
  requires(std::is_copy_constructible_v<value_type>)
  {
    // TBD
    return *this;
  }

  small_vec& operator=(std::initializer_list<value_type> il)
  requires(std::is_copy_constructible_v<value_type>)
  {
    return assign(il);
  }

  small_vec& assign(size_type count, const T& value)
  requires(std::is_copy_constructible_v<value_type>)
  {
    // TBD
    return *this;
  }

  small_vec& assign(size_type count)
  requires(std::is_default_constructible_v<value_type>)
  {
    // TBD
    return *this;
  }

  small_vec& assign(std::initializer_list<value_type> il)
  requires(std::is_copy_constructible_v<value_type>)
  {
    // TBD
    return *this;
  }

  template<std::input_iterator InputIt>
  small_vec& assign(InputIt first, InputIt last) {
    // TBD
    return *this;
  }

public:
  const_pointer data() const noexcept { return _capacity ? _alloc_data : _small_data.data(); }

  pointer data() noexcept { return const_cast<pointer>(std::as_const(*this).data()); }

  const_reference at(size_type idx) const {
    NTF_THROW_IF(idx >= size(),
                 std::out_of_range(fmt::format("Index {} out of range ({})", idx, size())));
    return data()[idx];
  }

  reference at(size_type idx) { return const_cast<reference>(std::as_const(*this).at(idx)); }

  const_pointer at_opt(size_type idx) const noexcept {
    return idx >= size() ? nullptr : data()[idx];
  }

  pointer at_opt(size_type idx) noexcept {
    return const_cast<pointer>(std::as_const(*this).at_opt(idx));
  }

  const_reference operator[](size_type idx) const {
    NTF_ASSERT(idx < size());
    return data()[idx];
  }

  reference operator[](size_type idx) { return const_cast<reference>(std::as_const(*this)[idx]); }

  const_reference front() const {
    NTF_ASSERT(!empty());
    return data()[0];
  }

  reference front() { return const_cast<reference>(std::as_const(*this).front()); }

  const_reference back() const {
    NTF_ASSERT(!empty());
    return data()[size() - 1];
  }

  reference back() { return const_cast<reference>(std::as_const(*this).back()); }

public:
  constexpr size_type max_size() const noexcept {
    return (std::numeric_limits<difference_type>::max()) / sizeof(value_type);
  }

  size_type size() const noexcept { return _size; }

  size_type size_bytes() const noexcept { return size() * sizeof(value_type); }

  size_type capacity() const noexcept { return _capacity; }

  bool empty() const noexcept { return _size == 0; }

  bool using_small() const noexcept { return _capacity > inner_capacity; }

  span<value_type> as_span() noexcept { return {data(), size()}; }

  span<const value_type> as_span() const noexcept { return {data(), size()}; }

  allocator_type& get_allocator() noexcept { return alloc_t::get_allocator(); }

  const allocator_type& get_allocator() const noexcept { return alloc_t::get_allocator(); }

public:
  iterator begin() noexcept { return iterator(data()); }

  const_iterator begin() const noexcept { return const_iterator(data()); }

  const_iterator cbegin() const noexcept { return const_iterator(data()); }

  iterator end() noexcept { return iterator(data() + size()); }

  const_iterator end() const noexcept { return iterator(data() + size()); }

  const_iterator cend() const noexcept { return iterator(data() + size()); }

  reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }

  const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }

  const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(cend()); }

  reverse_iterator rend() noexcept { return reverse_iterator(begin()); }

  const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }

  const_reverse_iterator crend() const noexcept { return const_reverse_iterator(begin()); }

private:
  void _do_destroy() noexcept {
    if constexpr (!std::is_trivially_destructible_v<value_type>) {
      for (iterator it = begin(); it != end(); ++it) {
        std::destroy_at(&*it);
      }
    }
    if (!using_small()) {
      alloc_t::deallocate(_alloc_data, size());
    }
  }

  void _copy_construct(const small_vec& other) {
    if (!other.using_small()) {
      _alloc_data = alloc_t::allocate(size());
    }

    try {
      ::ntf::impl::vec_construct_range_it(data(), other.begin(), other.end());
    } catch (...) {
      alloc_t::deallocate(_alloc_data);
      NTF_RETHROW();
    }
  }

  void
  _move_construct(small_vec& other) noexcept(std::is_nothrow_move_constructible_v<value_type>) {
    if (other.using_small()) {
      ::ntf::impl::vec_construct_range(data(), other.begin(), other.end());
    } else {
      _alloc_data = other._alloc_data;
    }
    other._alloc_data = nullptr;
    other._capacity = 0;
    other._size = 0;
  }

private:
  union {
    ::ntf::impl::inplace_vec_data<T, inner_capacity> _small_data;
    pointer _alloc_data;
  };

  size_type _size;
  size_type _capacity;
};

namespace meta {

template<typename T, typename Vec>
struct vec_rebinder {
  using type = ::ntf::meta::rebind_first_arg_t<T, Vec>;
};

template<typename T, typename Vec>
using vec_rebinder_t = vec_rebinder<T, Vec>::type;

template<typename T, typename U, typename Alloc>
struct vec_rebinder<T, std::vector<U, Alloc>> {
  using type = std::vector<T, typename std::allocator_traits<Alloc>::template rebind_alloc<T>>;
};

template<typename T, typename U, typename Alloc>
struct vec_rebinder<T, ::ntf::vec<U, Alloc>> {
  using type = ::ntf::vec<T, typename std::allocator_traits<Alloc>::template rebind_alloc<T>>;
};

template<typename T, typename U, size_t VecCap>
struct vec_rebinder<T, ::ntf::inplace_vec<U, VecCap>> {
  using type = ::ntf::inplace_vec<T, VecCap>;
};

template<typename T, typename U, size_t SmallCap, typename Alloc>
struct vec_rebinder<T, ::ntf::small_vec<U, SmallCap, Alloc>> {
  using type =
    ::ntf::small_vec<T, SmallCap, typename std::allocator_traits<Alloc>::template rebind_alloc<T>>;
};

} // namespace meta

// Stable index vector based on https://github.com/johnBuffer/StableIndexVector
template<::ntf::meta::move_swappable_type T, typename Cont = ::ntf::vec<T>>
class stable_vec {
public:
  using data_vec_type = Cont;
  using allocator_type = typename data_vec_type::allocator_type;

  using value_type = T;
  static_assert(std::same_as<typename Cont::value_type, value_type>,
                "Cont must be a container of T");
  using size_type = typename data_vec_type::size_type;

  using reference = value_type&;
  using const_reference = const value_type&;
  using pointer = value_type*;
  using const_pointer = const value_type*;

  using iterator = typename data_vec_type::iterator;
  static_assert(std::contiguous_iterator<iterator>, "Cont must be a contiguous container");

  using const_iterator = typename data_vec_type::const_iterator;
  using reverse_iterator = typename data_vec_type::reverse_iterator;
  using const_reverse_iterator = typename data_vec_type::const_reverse_iterator;

private:
  struct metadata_t {
    u32 ridx = 0;
    u32 current_epoch = 0;
  };

  template<bool IsConst>
  class data_handle_t {
  private:
    using vec_t = std::conditional_t<IsConst, const stable_vec, stable_vec>;
    using data_value_type = std::conditional_t<IsConst, const value_type, value_type>;

  public:
    data_handle_t() noexcept : _id(), _vec() {}

    data_handle_t(epoch_id id, vec_t& vec) noexcept : _id(id), _vec(&vec) {}

  public:
    epoch_id id() const noexcept { return _id; }

    bool empty() const noexcept { return !_vec || !_vec->is_valid(_id); }

    data_value_type& get() const {
      NTF_ASSERT(_vec);
      return (*_vec)[_id];
    }

  public:
    data_value_type* operator->() const {
      NTF_ASSERT(_vec);
      return &(*_vec)[_id];
    }

    data_value_type& operator*() const {
      NTF_ASSERT(_vec);
      return (*_vec)[_id];
    }

  public:
    explicit operator bool() const noexcept { return !empty(); }

  private:
    epoch_id _id;
    vec_t* _vec;

  public:
    friend stable_vec;
  };

public:
  using data_handle = data_handle_t<false>;
  using const_data_handle = data_handle_t<true>;

public:
  stable_vec() : _data(), _metadata(), _indexes() {}

  explicit stable_vec(const allocator_type& alloc) :
      _data(alloc),
      _metadata(
        typename std::allocator_traits<allocator_type>::template rebind_alloc<metadata_t>{alloc}),
      _indexes(typename std::allocator_traits<allocator_type>::template rebind_alloc<u32>{alloc}) {
  }

private:
  epoch_id _find_free_slot() {
    u32 id = -1;
    u32 epoch = 0;
    if (_metadata.size() > _data.size()) {
      // Slot available
      epoch = ++_metadata[_data.size()].current_epoch;
      id = static_cast<u32>(_metadata[_data.size()].ridx);
    } else {
      // Create new slot
      const auto new_id = _data.size();
      _metadata.emplace_back(new_id, 0);
      _indexes.emplace_back(new_id);
      id = static_cast<u32>(new_id);
    }
    NTF_ASSERT(id < _indexes.size());
    _indexes[id] = _data.size();
    return {id, epoch};
  }

public:
  epoch_id push_back(const value_type& object) {
    const auto id = _find_free_slot();
    _data.push_back(object);
    return id;
  }

  epoch_id push_back(value_type&& object) {
    const auto id = _find_free_slot();
    _data.push_back(std::move(object));
    return id;
  }

  template<typename... Args>
  epoch_id emplace_back(Args&&... args) {
    const auto id = _find_free_slot();
    _data.emplace_back(std::forward<Args>(args)...);
    return id;
  }

  void erase(epoch_id id) {
    const auto [index, epoch] = id.as_pair();
    NTF_ASSERT(index < _indexes.size());

    const u32 data_idx = _indexes[index];
    NTF_ASSERT(data_idx < _metadata.size());
    auto& metadata = _metadata[data_idx];
    NTF_ASSERT(metadata.current_epoch == epoch);

    const u32 last_data_idx = static_cast<u32>(_data.size() - 1);
    const u32 last_idx = metadata.ridx;
    ++metadata.current_epoch;

    std::swap(_data[data_idx], _data[last_data_idx]);
    std::swap(_metadata[data_idx], _metadata[last_data_idx]);
    std::swap(_indexes[index], _indexes[last_idx]);

    _data.pop_back();
  }

  void erase(const data_handle& handle) {
    NTF_ASSERT(handle.m_vector == this);
    NTF_ASSERT(!handle.empty());
    erase(handle.id());
  }

  void erase(const const_data_handle& handle) {
    NTF_ASSERT(handle.m_vector == this);
    NTF_ASSERT(!handle.empty());
    erase(handle.id());
  }

  void erase_index(u32 idx) {
    NTF_ASSERT(idx < _metadata.size());
    erase(epoch_id{_metadata[idx].ridx, _metadata[idx].current_epoch});
  }

public:
  template<typename F>
  void erase_if(F&& pred) {
    static_assert(std::is_invocable_v<std::remove_cvref_t<F>, value_type&>,
                  "F must be invocable with T&");
    using ret_t = std::invoke_result_t<F, value_type&>;
    static_assert(std::convertible_to<ret_t, bool>, "F must return a boolean value");
    for (u32 i = 0; i < _data.size();) {
      if (std::invoke(pred, _data[i])) {
        erase_index(i);
      } else {
        ++i;
      }
    }
  }

  void reserve(size_t size) {
    _data.reserve(size);
    _metadata.reserve(size);
    _indexes.reserve(size);
  }

  void clear() {
    _data.clear();
    for (auto& m : _metadata) {
      ++m.current_epoch;
    }
  }

  bool is_valid(epoch_id id) const {
    return id.index() < _indexes.size() &&
           _metadata[_indexes[id.index()]].current_epoch == id.epoch();
  }

  data_handle make_handle(epoch_id id) {
    NTF_ASSERT(is_valid(id));
    return {id, *this};
  }

  const_data_handle make_handle(epoch_id id) const {
    NTF_ASSERT(is_valid(id));
    return {id, *this};
  }

  u32 data_index_of(epoch_id id) const {
    NTF_ASSERT(id.index() < _indexes.size());
    return _indexes[id.index()];
  }

  data_handle make_index_handle(u32 idx) {
    NTF_ASSERT(idx < size());
    const auto& meta = _metadata[idx];
    epoch_id id(meta.ridx, meta.current_epoch);
    return {id, *this};
  }

  const_data_handle make_index_handle(u32 idx) const {
    NTF_ASSERT(idx < size());
    const auto& meta = _metadata[idx];
    epoch_id id(meta.ridx, meta.current_epoch);
    return {id, *this};
  }

  epoch_id next_id() const noexcept {
    if (_metadata.size() > _data.size()) {
      const auto& meta = _metadata[_data.size()];
      return {meta.ridx, meta.current_epoch + 1};
    } else {
      return {static_cast<u32>(_data.size()), 0};
    }
  }

public:
  const_pointer data() const noexcept { return _data.data(); }

  pointer data() noexcept { return _data.data(); }

  const_reference at(epoch_id id) const {
    NTF_THROW_IF(!is_valid(id),
                 std::out_of_range(fmt::format("Invalid id ({}:{})", id.index(), id.epoch())));
    return _data[_indexes[id.index()]];
  }

  reference at(epoch_id id) { return const_cast<T&>(std::as_const(*this).at(id)); }

  const_pointer at_opt(epoch_id id) const noexcept {
    if (!is_valid(id)) {
      return nullptr;
    }
    return &_data[_indexes[id.index()]];
  }

  pointer at_opt(epoch_id id) noexcept { return const_cast<T*>(std::as_const(*this).at_opt(id)); }

  const_reference operator[](epoch_id id) const {
    NTF_ASSERT(is_valid(id));
    return _data[_indexes[id.index()]];
  }

  reference operator[](epoch_id id) { return const_cast<reference>(std::as_const(*this)[id]); }

  const_reference front() const {
    NTF_ASSERT(!empty());
    return _data.front();
  }

  reference front() { return const_cast<reference>(std::as_const(*this).front()); }

  const_reference back() const {
    NTF_ASSERT(!empty());
    return _data.back();
  }

  reference back() { return const_cast<reference>(std::as_const(*this).back()); }

public:
  constexpr size_type max_size() const noexcept { return _data.max_size(); }

  size_type size() const { return _data.size(); }

  bool empty() const { return _data.empty(); }

  size_type capacity() const { return _data.capacity(); }

  span<value_type> as_span() noexcept { return {_data.data(), _data.size()}; }

  span<const value_type> as_span() const noexcept { return {_data.data(), _data.size()}; }

public:
  iterator begin() noexcept { return _data.begin(); }

  const_iterator begin() const noexcept { return _data.begin(); }

  const_iterator cbegin() const noexcept { return _data.cbegin(); }

  iterator end() noexcept { return _data.end(); }

  const_iterator end() const noexcept { return _data.end(); }

  const_iterator cend() const noexcept { return _data.cend(); }

  reverse_iterator rbegin() noexcept { return _data.rbegin(); }

  const_reverse_iterator rbegin() const noexcept { return _data.rbegin(); }

  const_reverse_iterator crbegin() const noexcept { return _data.crbegin(); }

  reverse_iterator rend() noexcept { return _data.rend(); }

  const_reverse_iterator rend() const noexcept { return _data.rend(); }

  const_reverse_iterator crend() const noexcept { return _data.crend(); }

private:
  data_vec_type _data;
  ::ntf::meta::vec_rebinder_t<metadata_t, data_vec_type> _metadata;
  ::ntf::meta::vec_rebinder_t<u32, data_vec_type> _indexes;
};

} // namespace ntf
