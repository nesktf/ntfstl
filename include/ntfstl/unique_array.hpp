#pragma once

#include <ntfstl/allocator.hpp>

namespace ntf {

namespace meta {

template<typename Deleter, typename T>
concept array_deleter_type = requires(Deleter& del, T* arr, size_t n) {
  noexcept(del(arr, n));
  { del(arr, n) } -> std::same_as<void>;
} || std::same_as<Deleter, std::default_delete<T[]>>;

} // namespace meta


namespace impl {

template<typename T, typename DelT>
struct unique_array_del : private DelT {
  unique_array_del() :
    DelT{} {}

  unique_array_del(const DelT& del) :
    DelT{del} {}

  unique_array_del(DelT&& del) :
    DelT{std::move(del)} {}

  void _delete_array(T* arr, size_t sz) noexcept {
    DelT::operator()(arr, sz);
  }

  DelT& _get_deleter() noexcept { return static_cast<DelT&>(*this); }
};

template<typename T> // Specialization for delete[]
struct unique_array_del<T, std::default_delete<T[]>> : private std::default_delete<T[]> {
  unique_array_del() noexcept {}

  unique_array_del(const std::default_delete<T[]>&) noexcept {}

  unique_array_del(std::default_delete<T[]>&&) noexcept {}

  void _delete_array(T* arr, size_t) noexcept {
    std::default_delete<T[]>::operator()(arr);
  }

  std::default_delete<T[]>& _get_deleter() noexcept {
    return static_cast<std::default_delete<T[]>&>(*this);
  }
};

} // namespace impl

template<typename T, meta::array_deleter_type<T> DelT = default_alloc_del<T>>
requires(!std::is_reference_v<T>)
class unique_array : private impl::unique_array_del<T, DelT> {
public:
  using value_type = T;
  using deleter_type = DelT;
  using size_type = size_t;

  using pointer = T*;
  using const_pointer = const T*;

  using iterator = pointer;
  using const_iterator = const_pointer;

private:
  using del_base = impl::unique_array_del<T, DelT>;

public:
  unique_array()
  noexcept(std::is_nothrow_default_constructible_v<DelT>) :
    del_base{},
    _arr{nullptr}, _sz{0u} {}

  unique_array(std::nullptr_t)
  noexcept(std::is_nothrow_default_constructible_v<DelT>) :
    del_base{},
    _arr{nullptr}, _sz{0u} {}

  explicit unique_array(const DelT& del)
  noexcept(std::is_nothrow_copy_constructible_v<DelT>) :
    del_base{del},
    _arr{nullptr}, _sz{0u} {}

  explicit unique_array(DelT&& del)
  noexcept(std::is_nothrow_move_constructible_v<DelT>) :
    del_base{std::move(del)},
    _arr{nullptr}, _sz{0u} {}

  unique_array(size_type size, pointer arr)
  noexcept(std::is_nothrow_default_constructible_v<DelT>) :
    del_base{},
    _arr{arr}, _sz{size} {}

  unique_array(size_type size, pointer arr, const DelT& del)
  noexcept(std::is_nothrow_copy_constructible_v<DelT>) :
    del_base{del},
    _arr{arr}, _sz{size} {}

  unique_array(size_type size, pointer arr, DelT&& del)
  noexcept(std::is_nothrow_move_constructible_v<DelT>) :
    del_base{std::move(del)},
    _arr{arr}, _sz{size} {}
  
  template<typename Alloc = std::allocator<T>>
  requires(meta::allocator_type<std::remove_cvref_t<Alloc>, T> && std::copy_constructible<T>)
  explicit unique_array(size_type size, const T& copy_obj = {}, Alloc&& alloc = {}) :
    del_base{alloc}, _sz{size}
  {
    try {
      _arr = alloc.allocate(size);
      for (size_t i = 0; i < size; ++i) {
        new (_arr+i) T(copy_obj);
      }
    } catch (...) {
      alloc.deallocate(_arr, size);
      throw;
    }
  }

  template<typename Alloc = std::allocator<T>>
  requires(meta::allocator_type<std::remove_cvref_t<Alloc>, T>)
  explicit unique_array(uninitialized_t, size_type size, Alloc&& alloc = {}) :
    del_base{alloc}, _sz{size}
  {
    try {
      _arr = alloc.allocate(size);
    } catch (...) {
      alloc.deallocate(_arr, size);
      throw;
    }
  }

  unique_array(unique_array&& other)
  noexcept(std::is_nothrow_move_constructible_v<DelT>) :
    del_base{static_cast<del_base&&>(other)},
    _arr{std::move(other._arr)}, _sz{std::move(other._sz)}
  {
    other._sz = 0u;
  }

  unique_array(const unique_array&) = delete;

  ~unique_array() noexcept { reset(); }

public:
  template<typename Alloc = std::allocator<T>>
  requires(meta::allocator_type<std::remove_cvref_t<Alloc>, T> && std::copy_constructible<T>)
  static auto from_allocator(
    size_type size, const T& copy_obj = {}, Alloc&& alloc = {}
  ) -> unique_array<T, allocator_delete<T, std::remove_cvref_t<Alloc>>>{
    using del_t = allocator_delete<T, std::remove_cvref_t<Alloc>>;

    pointer arr = nullptr;
    size_type sz = size;
    try {
      arr = alloc.allocate(sz);
      if (arr) {
        for (size_t i = 0; i < sz; ++i) {
          new (arr+i) T(copy_obj);
        }
      } else {
        sz = 0u;
      }
    } catch (...) {
      alloc.deallocate(arr, sz);
      throw;
    }
    return {sz, arr, del_t{std::forward<Alloc>(alloc)}};
  }

  template<typename Alloc = std::allocator<T>>
  requires(meta::allocator_type<std::remove_cvref_t<Alloc>, T>)
  static auto from_allocator(
    uninitialized_t, size_type size, Alloc&& alloc = {}
  ) -> unique_array<T, allocator_delete<T, std::remove_cvref_t<Alloc>>> {
    using del_t = allocator_delete<T, std::remove_cvref_t<Alloc>>;

    pointer arr = nullptr;
    size_type sz = size;
    try {
      arr = alloc.allocate(sz);
      if (!arr) {
        sz = 0u;
      }
    } catch (...) {
      alloc.deallocate(arr, sz);
      throw;
    }
    return {sz, arr, del_t{std::forward<Alloc>(alloc)}};
  }

public:
  void reset(size_type size, pointer arr) noexcept {
    if (!empty()) {
      del_base::_delete_array(_arr, _sz);
    }
    _arr = arr;
    _sz = size;
  }

  void reset() noexcept { reset(0u, nullptr); }

  [[nodiscard]] std::pair<pointer, size_type> release() noexcept {
    pointer ptr = get();
    size_type sz = size();
    _sz = 0;
    return std::make_pair(ptr, sz);
  }

  template<typename F>
  void for_each(F&& fun) {
    if (!empty()) {
      return;
    }
    for (auto it = begin(); it != end(); ++it) {
      fun(*it);
    }
  }

  template<typename F>
  void for_each(F&& fun) const {
    if (!empty()) {
      return;
    }
    for (auto it = begin(); it != end(); ++it) {
      fun(*it);
    }
  }

public:
  unique_array& operator=(std::nullptr_t) noexcept {
    reset();
    return *this;
  }

  unique_array& operator=(unique_array&& other)
    noexcept(std::is_nothrow_move_assignable_v<DelT>)
  {
    if (!empty()) {
      del_base::_delete_array(_arr, _sz);
    }

    del_base::operator=(static_cast<del_base&&>(other));
    _arr = std::move(other._arr);
    _sz = std::move(other._sz);

    other._arr = nullptr;
    other._sz = 0u;

    return *this;
  }

  unique_array& operator=(const unique_array&) = delete;

  value_type& operator[](size_type idx) {
    NTF_ASSERT(idx < size());
    return get()[idx];
  }
  const value_type& operator[](size_type idx) const {
    NTF_ASSERT(idx < size());
    return get()[idx];
  }

  pointer at(size_type idx) noexcept {
    if (empty() || idx >= size()) {
      return nullptr;
    }
    return get()+idx;
  }

  const_pointer at(size_type idx) const noexcept {
    if (empty() || idx >= size()) {
      return nullptr;
    }
    return get()+idx;
  }

public:
  size_type size() const noexcept { return _sz; }

  pointer get() noexcept { return _arr; }
  const_pointer get() const noexcept { return _arr; }

  pointer data() noexcept { return _arr; }
  const_pointer data() const noexcept { return _arr; }

  bool empty() const noexcept { return _sz == 0u; }
  explicit operator bool() const noexcept { return !empty(); }

  DelT& get_deleter() noexcept { return del_base::_get_deleter(); }
  const DelT& get_deleter() const noexcept { del_base::_get_deleter(); }

  iterator begin() noexcept { return get(); }
  const_iterator begin() const noexcept { return get(); }
  const_iterator cbegin() const noexcept { return get(); }

  iterator end() noexcept { return get()+size(); }
  const_iterator end() const noexcept { return get()+size(); }
  const_iterator cend() const noexcept { return get()+size(); }

private:
  pointer _arr;
  size_t _sz;
};

} // namespace ntf
