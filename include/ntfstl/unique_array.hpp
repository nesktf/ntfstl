#pragma once

#include <ntfstl/memory.hpp>

namespace ntf {

namespace impl {

template<typename T, typename DelT>
struct unique_array_del : private DelT {
  unique_array_del() noexcept(std::is_nothrow_default_constructible_v<DelT>) : DelT() {}

  unique_array_del(const DelT& del) noexcept(std::is_nothrow_copy_constructible_v<DelT>) :
      DelT(del) {}

  void delete_array(T* arr,
                    typename DelT::size_type n) noexcept(std::is_nothrow_destructible_v<T>) {
    DelT::operator()(arr, n);
  }

  DelT& get_deleter() noexcept { return static_cast<DelT&>(*this); }

  const DelT& get_deleter() const noexcept { return static_cast<const DelT&>(*this); }
};

template<typename T> // Specialization for delete[]
struct unique_array_del<T, std::default_delete<T[]>> : private std::default_delete<T[]> {
  unique_array_del() noexcept {}

  unique_array_del(const std::default_delete<T[]>&) noexcept {}

  unique_array_del(std::default_delete<T[]>&&) noexcept {}

  void delete_array(T* arr, size_t) noexcept { std::default_delete<T[]>::operator()(arr); }

  std::default_delete<T[]>& get_deleter() noexcept {
    return static_cast<std::default_delete<T[]>&>(*this);
  }

  const std::default_delete<T[]>& get_deleter() const noexcept {
    return static_cast<const std::default_delete<T[]>&>(*this);
  }
};

} // namespace impl

template<typename T, meta::array_deleter_type<T> DelT = ::ntf::mem::default_pool::deleter<T>>
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

  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

private:
  using del_base = impl::unique_array_del<T, DelT>;

public:
  unique_array() noexcept(std::is_nothrow_default_constructible_v<DelT>) :
      del_base(), _arr{nullptr}, _sz{0u} {}

  unique_array(std::nullptr_t) noexcept(std::is_nothrow_default_constructible_v<DelT>) :
      del_base(), _arr{nullptr}, _sz{0u} {}

  explicit unique_array(const DelT& del) noexcept(std::is_nothrow_copy_constructible_v<DelT>) :
      del_base(del), _arr{nullptr}, _sz{0u} {}

  unique_array(pointer arr, size_type n) noexcept(std::is_nothrow_default_constructible_v<DelT>) :
      del_base(), _arr{arr}, _sz{n} {}

  unique_array(pointer arr, size_type n,
               const DelT& del) noexcept(std::is_nothrow_copy_constructible_v<DelT>) :
      del_base(del), _arr{arr}, _sz{n} {}

  unique_array(unique_array&& other) noexcept(std::is_nothrow_move_constructible_v<DelT>) :
      del_base(other.get_deleter()), _arr{other._arr}, _sz{other._sz} {
    other._arr = 0u;
  }

  unique_array(const unique_array&) = delete;

  ~unique_array() noexcept(std::is_nothrow_destructible_v<T>) {
    if (!empty()) {
      del_base::delete_array(_arr, _sz);
    }
  }

public:
  unique_array& assign(pointer arr, size_type size) noexcept {
    if (!empty()) {
      del_base::delete_array(_arr, _sz);
    }

    _arr = arr;
    _sz = size;

    return *this;
  }

  void reset() noexcept { assign(nullptr, 0u); }

  [[nodiscard]] std::pair<pointer, size_type> release() noexcept {
    pointer ptr = get();
    size_type sz = size();
    _sz = 0;
    _arr = nullptr;
    return std::make_pair(ptr, sz);
  }

public:
  unique_array& operator=(std::nullptr_t) noexcept {
    reset();
    return *this;
  }

  unique_array& operator=(unique_array&& other) noexcept(std::is_nothrow_move_assignable_v<DelT>) {
    if (!empty()) {
      del_base::delete_array(_arr, _sz);
    }

    del_base::operator=(static_cast<del_base&&>(other));
    _arr = std::move(other._arr);
    _sz = std::move(other._sz);

    other._arr = nullptr;

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

  value_type& at(size_type idx) noexcept(NTF_NOEXCEPT) {
    NTF_THROW_IF(idx >= size(), std::out_of_range(fmt::format("Index {} out of range", idx)));
    return _arr[idx];
  }

  const value_type& at(size_type idx) const noexcept(NTF_NOEXCEPT) {
    NTF_THROW_IF(idx >= size(), std::out_of_range(fmt::format("Index {} out of range", idx)));
    return _arr[idx];
  }

public:
  size_type size() const noexcept { return _sz; }

  pointer get() noexcept { return _arr; }

  const_pointer get() const noexcept { return _arr; }

  pointer data() noexcept { return _arr; }

  const_pointer data() const noexcept { return _arr; }

  bool empty() const noexcept { return _arr == nullptr; }

  DelT& get_deleter() noexcept { return del_base::get_deleter(); }

  const DelT& get_deleter() const noexcept { del_base::get_deleter(); }

public:
  explicit operator bool() const noexcept { return !empty(); }

public:
  iterator begin() noexcept { return get(); }

  const_iterator begin() const noexcept { return get(); }

  const_iterator cbegin() const noexcept { return get(); }

  iterator end() noexcept { return get() + size(); }

  const_iterator end() const noexcept { return get() + size(); }

  const_iterator cend() const noexcept { return get() + size(); }

  reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }

  const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }

  const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(end()); }

  reverse_iterator rend() noexcept { return reverse_iterator(begin()); }

  const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }

  const_reverse_iterator crend() const noexcept { return const_reverse_iterator(begin()); }

private:
  pointer _arr;
  size_t _sz;
};

template<typename T>
requires(std::is_default_constructible_v<T>)
auto make_unique_arr(size_t n) -> ::ntf::unique_array<T, ::ntf::mem::default_pool::deleter<T>> {
  auto* ptr = ::ntf::mem::default_pool::instance().construct_n<T>(n);
  return ::ntf::unique_array<T, ::ntf::mem::default_pool::deleter<T>>(ptr, n);
}

template<typename T>
requires(std::copy_constructible<T>)
auto make_unique_arr(size_t n, const T& copy)
  -> ::ntf::unique_array<T, ::ntf::mem::default_pool::deleter<T>> {
  auto* ptr = ::ntf::mem::default_pool::instance().construct_n<T>(n, copy);
  return ::ntf::unique_array<T, ::ntf::mem::default_pool::deleter<T>>(ptr, n);
}

template<typename T>
requires(std::is_trivially_constructible_v<T>)
auto make_unique_arr(::ntf::uninitialized_t, size_t n)
  -> ::ntf::unique_array<T, ::ntf::mem::default_pool::deleter<T>> {
  auto* ptr = ::ntf::mem::default_pool::instance().construct_n<T>(::ntf::uninitialized, n);
  return ::ntf::unique_array<T, ::ntf::mem::default_pool::deleter<T>>(ptr, n);
}

template<typename T>
requires(std::is_default_constructible_v<T>)
auto make_unique_arr_pool(size_t n, ::ntf::mem::memory_pool& pool)
  -> ::ntf::unique_array<T, ::ntf::mem::pool_deleter<T>> {
  ::ntf::mem::pool_deleter<T> del(pool);
  T* ptr = pool.allocate(n * sizeof(T), alignof(T));
  NTF_THROW_IF(!ptr, std::bad_alloc());
  for (size_t i = 0; i < n; ++i) {
    new (ptr + i) T();
  }
  return ::ntf::unique_array<T, ::ntf::mem::pool_deleter<T>>(ptr, n, del);
}

template<typename T>
requires(std::copy_constructible<T>)
auto make_unique_arr_pool(size_t n, const T& copy, ::ntf::mem::memory_pool& pool)
  -> ::ntf::unique_array<T, ::ntf::mem::pool_deleter<T>> {
  ::ntf::mem::pool_deleter<T> del(pool);
  T* ptr = pool.allocate(n * sizeof(T), alignof(T));
  NTF_THROW_IF(!ptr, std::bad_alloc());
  for (size_t i = 0; i < n; ++i) {
    new (ptr + i) T(copy);
  }
  return ::ntf::unique_array<T, ::ntf::mem::pool_deleter<T>>(ptr, n, del);
}

template<typename T>
requires(std::is_trivially_constructible_v<T>)
auto make_unique_arr_pool(::ntf::uninitialized_t, size_t n, ::ntf::mem::memory_pool& pool)
  -> ::ntf::unique_array<T, ::ntf::mem::pool_deleter<T>> {
  ::ntf::mem::pool_deleter<T> del(pool);
  T* ptr = pool.allocate(n * sizeof(T), alignof(T));
  NTF_THROW_IF(!ptr, std::bad_alloc());
  return ::ntf::unique_array<T, ::ntf::mem::pool_deleter<T>>(ptr, n, del);
}

template<typename T, typename Alloc>
requires(::ntf::meta::allocator_type<std::remove_cvref_t<Alloc>, T> &&
         !std::same_as<T, std::remove_cvref_t<Alloc>> &&
         !std::same_as<::ntf::mem::memory_pool&, Alloc> && std::is_default_constructible_v<T>)
auto make_unique_arr_alloc(size_t n, Alloc&& alloc)
  -> ::ntf::unique_array<T, ::ntf::mem::allocator_delete<T, std::remove_cvref_t<Alloc>>> {
  ::ntf::mem::allocator_delete<T, std::remove_cvref_t<Alloc>> del(alloc);
  T* ptr = alloc.allocate(n);
  NTF_ASSERT(ptr);
  for (size_t i = 0; i < n; ++i) {
    new (ptr + i) T();
  }
  return ::ntf::unique_array<T, ::ntf::mem::allocator_delete<T, std::remove_cvref_t<Alloc>>>(
    ptr, n, del);
}

template<typename T, typename Alloc>
requires(::ntf::meta::allocator_type<std::remove_cvref_t<Alloc>, T> &&
         !std::same_as<T, std::remove_cvref_t<Alloc>> &&
         !std::same_as<::ntf::mem::memory_pool&, Alloc> && std::copy_constructible<T>)
auto make_unique_arr_alloc(size_t n, const T& copy, Alloc&& alloc)
  -> ::ntf::unique_array<T, ::ntf::mem::allocator_delete<T, std::remove_cvref_t<Alloc>>> {
  ::ntf::mem::allocator_delete<T, std::remove_cvref_t<Alloc>> del(alloc);
  T* ptr = alloc.allocate(n);
  NTF_ASSERT(ptr);
  for (size_t i = 0; i < n; ++i) {
    new (ptr + i) T(copy);
  }
  return ::ntf::unique_array<T, ::ntf::mem::allocator_delete<T, std::remove_cvref_t<Alloc>>>(
    ptr, n, del);
}

template<typename T, typename Alloc>
requires(::ntf::meta::allocator_type<std::remove_cvref_t<Alloc>, T> &&
         !std::same_as<T, std::remove_cvref_t<Alloc>> &&
         !std::same_as<::ntf::mem::memory_pool&, Alloc> && std::is_trivially_constructible_v<T>)
auto make_unique_arr_alloc(::ntf::uninitialized_t, size_t n, Alloc&& alloc)
  -> ::ntf::unique_array<T, ::ntf::mem::allocator_delete<T, std::remove_cvref_t<Alloc>>> {
  ::ntf::mem::allocator_delete<T, std::remove_cvref_t<Alloc>> del(alloc);
  T* ptr = alloc.allocate(n);
  NTF_ASSERT(ptr);
  return ::ntf::unique_array<T, ::ntf::mem::allocator_delete<T, std::remove_cvref_t<Alloc>>>(
    ptr, n, del);
}

} // namespace ntf
