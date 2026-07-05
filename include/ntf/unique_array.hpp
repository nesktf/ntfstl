#ifndef NTF_UNIQUE_ARRAY_HPP_
#define NTF_UNIQUE_ARRAY_HPP_

#include <ntf/concepts.hpp>
#include <ntf/memory.hpp>

namespace ntf {

namespace impl {

template<meta::nothrow_default_constructible T>
constexpr T* construct_array(T* ptr, size_t n) noexcept(meta::nothrow_default_constructible<T>) {
  if constexpr (meta::nothrow_default_constructible<T>) {
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
  return launder(ptr);
}

template<meta::copy_constructible T>
constexpr T* construct_array(T* ptr, size_t n,
                             const T& obj) noexcept(meta::nothrow_copy_constructible<T>) {
  if constexpr (meta::nothrow_copy_constructible<T>) {
    for (size_t i = 0; i < n; ++i) {
      NTF_PNEW(ptr + i) T(obj);
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
  return launder(ptr);
}

} // namespace impl

template<typename Alloc>
class ArrayDelete : private Alloc {
public:
  using value_type = typename Alloc::value_type;
  using pointer = typename Alloc::pointer;
  using size_type = typename Alloc::size_type;
  using difference_type = typename Alloc::difference_type;

public:
  template<typename U>
  using rebind = ArrayDelete<U>;

public:
  constexpr ArrayDelete() noexcept(meta::nothrow_default_constructible<Alloc>) : Alloc() {}

  constexpr ArrayDelete(const Alloc& alloc) noexcept(meta::nothrow_copy_constructible<Alloc>) :
      Alloc(alloc) {}

public:
  constexpr ArrayDelete(const ArrayDelete&) noexcept(meta::nothrow_copy_constructible<Alloc>) =
    default;
  constexpr ~ArrayDelete() noexcept = default;

  template<typename U>
  requires(!meta::same_as<value_type, U>)
  constexpr ArrayDelete(const rebind<U>& other) noexcept(meta::nothrow_copy_constructible<Alloc>) :
      Alloc(other.allocator()) {}

public:
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
    Alloc::deallocate(ptr, n);
  }

public:
  Alloc& allocator() noexcept { return static_cast<Alloc&>(*this); }

  const Alloc& allocator() const noexcept { return static_cast<const Alloc&>(*this); }

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

template<typename T, typename Deleter>
requires(!meta::is_reference_v<T>)
class UniqueArray : private Deleter {
public:
  using value_type = T;
  using deleter_type = Deleter;
  using size_type = size_t;

  using pointer = T*;
  using const_pointer = const T*;

  static_assert(meta::same_as<value_type, typename Deleter::value_type>,
                "Deleter has to delete T");
  static_assert(std::is_same_v<pointer, typename Deleter::pointer>, "Deleter has to delete T");

  using iterator = pointer;
  using const_iterator = const_pointer;

  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  struct ArrayData {
    pointer ptr;
    size_type size;
  };

public:
  UniqueArray() noexcept(meta::nothrow_default_constructible<Deleter>) :
      Deleter(), _data{nullptr}, _count{0u} {}

  UniqueArray(std::nullptr_t) noexcept(meta::nothrow_default_constructible<Deleter>) :
      Deleter(), _data{nullptr}, _count{0u} {}

  explicit UniqueArray(const Deleter& del) noexcept(meta::nothrow_copy_constructible<Deleter>) :
      Deleter(del), _data{nullptr}, _count{0u} {}

  UniqueArray(pointer arr, size_type n) noexcept(meta::nothrow_default_constructible<Deleter>) :
      Deleter(), _data{arr}, _count{n} {}

  UniqueArray(pointer arr, size_type n,
              const Deleter& del) noexcept(meta::nothrow_copy_constructible<Deleter>) :
      Deleter(del), _data{arr}, _count{n} {}

  UniqueArray(UniqueArray&& other) noexcept(meta::nothrow_move_constructible<Deleter>) :
      Deleter(other.deleter()), _data{other._data}, _count{other._count} {
    other._data = 0u;
  }

  UniqueArray(const UniqueArray&) = delete;

  ~UniqueArray() noexcept(meta::nothrow_destructible<T>) {
    if (!empty()) {
      Deleter::operator()(_data, _count);
    }
  }

public:
  UniqueArray& assign(pointer arr, size_type size) noexcept {
    if (!empty()) {
      Deleter::operator()(_data, _count);
    }

    _data = arr;
    _count = size;

    return *this;
  }

  void reset() noexcept { assign(nullptr, 0u); }

  NTF_NODISCARD ArrayData release() noexcept {
    pointer ptr = data();
    size_type sz = size();
    _count = 0;
    _data = nullptr;
    return {ptr, sz};
  }

public:
  UniqueArray& operator=(std::nullptr_t) noexcept {
    reset();
    return *this;
  }

  UniqueArray&
  operator=(UniqueArray&& other) noexcept(std::is_nothrow_move_assignable_v<Deleter>) {
    if (!empty()) {
      Deleter::operator()(_data, _count);
    }

    Deleter::operator=(static_cast<Deleter&&>(other));
    _data = move(other._data);
    _count = move(other._count);

    other._data = nullptr;

    return *this;
  }

  UniqueArray& operator=(const UniqueArray&) = delete;

  value_type& operator[](size_type idx) {
    NTF_ASSERT(idx < size(), "Index out of range in UniqueArray");
    return data()[idx];
  }

  const value_type& operator[](size_type idx) const {
    NTF_ASSERT(idx < size(), "Index out of range in UniqueArray");
    return data()[idx];
  }

  value_type& at(size_type idx) {
    NTF_THROW_IF(idx >= size(), std::out_of_range("Index out of range in UniqueArray, was " +
                                                  std::to_string(idx)));
    return _data[idx];
  }

  const value_type& at(size_type idx) const {
    NTF_THROW_IF(idx >= size(), std::out_of_range("Index out of range in UniqueArray, was " +
                                                  std::to_string(idx)));
    return _data[idx];
  }

public:
  size_type size() const noexcept { return _count; }

  pointer data() noexcept { return _data; }

  const_pointer data() const noexcept { return _data; }

  bool empty() const noexcept { return _data == nullptr; }

  Deleter& deleter() noexcept { return static_cast<Deleter&>(*this); }

  const Deleter& deleter() const noexcept { return static_cast<const Deleter&>(*this); }

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

public:
  explicit operator bool() const noexcept { return !empty(); }

private:
  pointer _data;
  size_type _count;
};

template<typename T, typename Alloc = DefaultAlloc<T>>
requires(meta::default_constructible<T> && !meta::same_as<T, std::remove_cvref_t<Alloc>>)
auto make_unique_array(size_t n, Alloc&& alloc = {})
  -> UniqueArray<T, ArrayDelete<std::remove_cvref_t<Alloc>>> {
  ArrayDelete<std::remove_cvref_t<Alloc>> deleter{alloc};
  T* ptr = alloc.allocate(n);
  NTF_THROW_IF(!ptr, std::bad_alloc());
  impl::construct_array(ptr, n);
  return UniqueArray<T, ArrayDelete<std::remove_cvref_t<Alloc>>>(ptr, n, deleter);
}

template<typename T, typename Alloc = std::allocator<T>>
requires(std::copy_constructible<T>)
auto make_unique_array(size_t n, const T& other, Alloc&& alloc = {})
  -> UniqueArray<T, ArrayDelete<std::remove_cvref_t<Alloc>>> {
  ArrayDelete<std::remove_cvref_t<Alloc>> deleter{alloc};
  T* ptr = alloc.allocate(n);
  NTF_THROW_IF(!ptr, std::bad_alloc());
  impl::construct_array(ptr, n, other);
  return UniqueArray<T, ArrayDelete<std::remove_cvref_t<Alloc>>>(ptr, n, deleter);
}

template<typename T, typename Alloc = std::allocator<T>>
requires(std::is_trivially_constructible_v<T>)
auto make_unique_array(uninitialized_t, size_t n, Alloc&& alloc = {})
  -> UniqueArray<T, ArrayDelete<std::remove_cvref_t<Alloc>>> {
  ArrayDelete<std::remove_cvref_t<Alloc>> deleter{alloc};
  T* ptr = alloc.allocate(n);
  NTF_THROW_IF(!ptr, std::bad_alloc());
  return UniqueArray<T, ArrayDelete<std::remove_cvref_t<Alloc>>>(ptr, n, deleter);
}

} // namespace ntf

#endif // NTF_UNIQUE_ARRAY_HPP_
