#ifndef NTF_UNIQUE_HPP_
#define NTF_UNIQUE_HPP_

#include <ntf/func_util.hpp>
#include <ntf/memory.hpp>

namespace ntf {

namespace impl {

template<meta::nothrow_default_constructible T>
constexpr T* construct_array(T* ptr, size_t n) noexcept(meta::nothrow_default_constructible<T>) {
  if constexpr (meta::nothrow_default_constructible<T>) {
    for (size_t i = 0; i < n; ++i) {
      construct_offset(ptr, i);
    }
  } else {
    i64 i = 0;
    DeferFn on_err = [&]() {
      for (; i >= 0; --i) {
        destroy_offset(ptr, i);
      }
    };
    for (; i < (i64)n; ++i) {
      construct_offset(ptr, i);
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
      construct_offset(ptr, i, obj);
    }
  } else {
    i64 i = 0;
    DeferFn on_err = [&]() {
      for (; i >= 0; --i) {
        destroy_offset(ptr, i);
      }
    };
    for (; i < (i64)n; ++i) {
      construct_offset(ptr, i, obj);
    }
    on_err.disengage();
  }
  return launder(ptr);
}

} // namespace impl

template<typename Alloc>
class AllocArrayDelete : private Alloc {
public:
  using value_type = typename Alloc::value_type;
  using pointer = typename Alloc::pointer;
  using size_type = typename Alloc::size_type;
  using difference_type = typename Alloc::difference_type;

public:
  template<typename U>
  using rebind = AllocArrayDelete<U>;

public:
  constexpr AllocArrayDelete() noexcept(meta::nothrow_default_constructible<Alloc>) : Alloc() {}

  constexpr AllocArrayDelete(const Alloc& alloc) noexcept(
    meta::nothrow_copy_constructible<Alloc>) : Alloc(alloc) {}

public:
  constexpr AllocArrayDelete(const AllocArrayDelete&) noexcept(
    meta::nothrow_copy_constructible<Alloc>) = default;
  constexpr ~AllocArrayDelete() noexcept = default;

  template<typename U>
  requires(!meta::same_as<value_type, U>)
  constexpr AllocArrayDelete(const rebind<U>& other) noexcept(
    meta::nothrow_copy_constructible<Alloc>) : Alloc(other.allocator()) {}

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
  constexpr bool operator==(const rebind<U>& other) const noexcept {
    return allocator() == other.allocator();
  }

  template<typename U>
  constexpr bool operator!=(const rebind<U>& other) const noexcept {
    return !(*this == other);
  }
};

template<typename T>
using DefaultArrayDelete = AllocArrayDelete<DefaultAlloc<T>>;

template<typename T, typename Deleter = DefaultArrayDelete<T>>
class UniqueArray : private Deleter {
public:
  using value_type = T;
  using deleter_type = Deleter;
  using size_type = size_t;

  using pointer = T*;
  using const_pointer = const T*;

  static_assert(!meta::is_reference_v<T>, "T can't be a reference");
  static_assert(meta::same_as<value_type, typename Deleter::value_type>,
                "Deleter has to delete T");
  static_assert(meta::same_as<pointer, typename Deleter::pointer>, "Deleter has to delete T");

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

  UniqueArray(nullptr_t) noexcept(meta::nothrow_default_constructible<Deleter>) :
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
    other._data = nullptr;
    other._count = 0;
  }

  ~UniqueArray() noexcept(meta::nothrow_destructible<T>) {
    if (!empty()) {
      Deleter::operator()(_data, _count);
    }
  }

  NTF_NO_COPY(UniqueArray);

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
    ArrayData ret{data(), size()};
    _count = 0;
    _data = nullptr;
    return ret;
  }

public:
  UniqueArray& operator=(nullptr_t) noexcept {
    reset();
    return *this;
  }

  UniqueArray& operator=(UniqueArray&& other) noexcept(meta::nothrow_move_assignable<Deleter>) {
    if (!empty()) {
      Deleter::operator()(_data, _count);
    }

    Deleter::operator=(static_cast<Deleter&&>(other));
    _data = move(other._data);
    _count = move(other._count);

    other._data = nullptr;

    return *this;
  }

  value_type& operator[](size_type idx) {
    NTF_ASSERT(idx < size(), "Index out of range in UniqueArray");
    return data()[idx];
  }

  const value_type& operator[](size_type idx) const {
    NTF_ASSERT(idx < size(), "Index out of range in UniqueArray");
    return data()[idx];
  }

  value_type& at(size_type idx) {
    NTF_THROW_IF(idx >= size(), MsgException("Index out of range in UniqueArray"));
    return data()[idx];
  }

  const value_type& at(size_type idx) const {
    NTF_THROW_IF(idx >= size(), MsgException("Index out of range in UniqueArray"));
    return data()[idx];
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

template<meta::default_constructible T>
UniqueArray<T> make_unique_array(size_t n) {
  T* ptr = DefaultAlloc<T>{}.allocate(n);
  impl::construct_array(ptr, n);
  return UniqueArray<T>(ptr, n);
}

template<meta::default_constructible T, meta::alloc_arg<T> Alloc>
auto make_unique_array(size_t n, Alloc&& alloc)
  -> UniqueArray<T, AllocArrayDelete<meta::remove_cvref_t<Alloc>>> {
  AllocArrayDelete<meta::remove_cvref_t<Alloc>> deleter{alloc};
  T* ptr = alloc.allocate(n);
  impl::construct_array(ptr, n);
  return UniqueArray<T, AllocArrayDelete<meta::remove_cvref_t<Alloc>>>(ptr, n, deleter);
}

template<meta::default_constructible T, meta::mem_arg<T> Mem>
auto make_unique_array(size_t n, Mem&& mem)
  -> UniqueArray<T, AllocArrayDelete<typename meta::remove_cvref_t<Mem>::template bind_alloc<T>>> {
  using Alloc = typename meta::remove_cvref_t<Mem>::template bind_alloc<T>;
  return make_unique_array(n, Alloc(mem));
}

template<typename T, typename Alloc = std::allocator<T>>
requires(std::copy_constructible<T>)
auto make_unique_array(size_t n, const T& other, Alloc&& alloc = {})
  -> UniqueArray<T, AllocArrayDelete<std::remove_cvref_t<Alloc>>> {
  AllocArrayDelete<std::remove_cvref_t<Alloc>> deleter{alloc};
  T* ptr = alloc.allocate(n);
  NTF_THROW_IF(!ptr, std::bad_alloc());
  impl::construct_array(ptr, n, other);
  return UniqueArray<T, AllocArrayDelete<std::remove_cvref_t<Alloc>>>(ptr, n, deleter);
}

template<typename T, typename Alloc = std::allocator<T>>
requires(std::is_trivially_constructible_v<T>)
auto make_unique_array(uninitialized_t, size_t n, Alloc&& alloc = {})
  -> UniqueArray<T, AllocArrayDelete<std::remove_cvref_t<Alloc>>> {
  AllocArrayDelete<std::remove_cvref_t<Alloc>> deleter{alloc};
  T* ptr = alloc.allocate(n);
  NTF_THROW_IF(!ptr, std::bad_alloc());
  return UniqueArray<T, AllocArrayDelete<std::remove_cvref_t<Alloc>>>(ptr, n, deleter);
}

} // namespace ntf

#endif // NTF_UNIQUE_HPP_
