#ifndef NTF_UNIQUE_HPP_
#define NTF_UNIQUE_HPP_

#include <ntf/impl/iterator.hpp>
#include <ntf/memory.hpp>

namespace ntf {

template<typename Alloc>
class AllocDelete : private Alloc {
public:
  using value_type = typename Alloc::value_type;
  using pointer = typename Alloc::pointer;
  using size_type = typename Alloc::size_type;
  using difference_type = typename Alloc::difference_type;

public:
  template<typename U>
  using rebind = AllocDelete<U>;

public:
  constexpr AllocDelete() noexcept(meta::nothrow_default_constructible<Alloc>) : Alloc() {}

  constexpr AllocDelete(const Alloc& alloc) noexcept(meta::nothrow_copy_constructible<Alloc>) :
      Alloc(alloc) {}

public:
  constexpr AllocDelete(const AllocDelete&) noexcept(meta::nothrow_copy_constructible<Alloc>) =
    default;
  constexpr ~AllocDelete() noexcept = default;

  template<typename U>
  requires(!meta::same_as<value_type, U>)
  constexpr AllocDelete(const rebind<U>& other) noexcept(meta::nothrow_copy_constructible<Alloc>) :
      Alloc(other.allocator()) {}

public:
  template<typename U = value_type>
  requires(meta::convertible_to<pointer, U*>)
  constexpr void operator()(U* ptr) noexcept {
    static_assert(!meta::is_void_v<value_type>, "Can't delete incomplete type");
    static_assert(sizeof(value_type), "Can't delete incomplete type");
    static_assert(meta::nothrow_destructible<value_type>, "Can't delete throwing type");
    if constexpr (!meta::trivially_destructible<value_type>) {
      destroy_at(ptr);
    }
    Alloc::deallocate(ptr, 1);
  }

  template<typename U = value_type>
  requires(meta::convertible_to<pointer, U*>)
  constexpr void operator()(U* ptr, size_type n) noexcept {
    static_assert(!meta::is_void_v<value_type>, "Can't delete incomplete type");
    static_assert(sizeof(value_type), "Can't delete incomplete type");
    static_assert(meta::nothrow_destructible<value_type>, "Can't delete throwing type");
    if constexpr (!meta::trivially_destructible<value_type>) {
      for (size_type i = 0; i < n; ++i) {
        destroy_offset(ptr, n);
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
using DefaultDelete = AllocDelete<DefaultAlloc<T>>;

template<typename T, typename Deleter = DefaultDelete<T>>
class UniquePtr : private Deleter {
public:
  using element_type = T;
  using deleter_type = Deleter;
  using pointer = T*;

  static_assert(!meta::is_reference_v<T>, "T can't be a reference");
  static_assert(meta::same_as<element_type, typename Deleter::value_type>,
                "Deleter has to delete T");
  static_assert(meta::same_as<pointer, typename Deleter::pointer>, "Deleter has to delete T");

public:
  constexpr UniquePtr() noexcept(meta::nothrow_default_constructible<Deleter>) :
      Deleter(), _ptr(nullptr) {}

  constexpr UniquePtr(nullptr_t) noexcept(meta::nothrow_default_constructible<Deleter>) :
      Deleter(), _ptr(nullptr) {}

  constexpr explicit UniquePtr(const Deleter& del) noexcept(
    meta::nothrow_copy_constructible<Deleter>) : Deleter(del), _ptr(nullptr) {}

  constexpr explicit UniquePtr(pointer ptr) noexcept(
    meta::nothrow_default_constructible<Deleter>) : Deleter(), _ptr(ptr) {}

  constexpr UniquePtr(pointer ptr,
                      const Deleter& del) noexcept(meta::nothrow_copy_constructible<Deleter>) :
      Deleter(del), _ptr(ptr) {}

  constexpr ~UniquePtr() noexcept {
    static_assert(meta::nothrow_destructible<T>, "T has to be nothrow destructible");
    if (_ptr) {
      Deleter::operator()(_ptr);
    }
  }

  constexpr UniquePtr(UniquePtr&& other) noexcept(meta::nothrow_copy_constructible<Deleter>) :
      Deleter(other.deleter()), _ptr(other._ptr) {
    other._ptr = nullptr;
  }

  NTF_NO_COPY(UniquePtr);

public:
  UniquePtr& reset() noexcept {
    if (_ptr) {
      Deleter::operator()(_ptr);
    }
    _ptr = nullptr;
    return *this;
  }

  NTF_NODISCARD pointer release() noexcept {
    pointer p = _ptr;
    _ptr = nullptr;
    return p;
  }

  pointer get() const noexcept { return _ptr; }

  bool empty() const noexcept { return _ptr == nullptr; }

public:
  UniquePtr& operator=(nullptr_t) noexcept {
    reset();
    return *this;
  }

  UniquePtr& operator=(UniquePtr&& other) noexcept(meta::nothrow_move_assignable<Deleter>) {
    if (_ptr) {
      Deleter::operator()(_ptr);
    }

    Deleter::operator=(static_cast<Deleter&&>(other));
    _ptr = nullptr;

    other._ptr = nullptr;

    return *this;
  }

  element_type& operator*() {
    NTF_ASSERT(_ptr, "Dereferencing null UniquePtr");
    return *_ptr;
  }

  const element_type& operator*() const {
    NTF_ASSERT(_ptr, "Dereferencing null UniquePtr");
    return *_ptr;
  }

  pointer operator->() const {
    NTF_ASSERT(_ptr, "Dereferencing null UniquePtr");
    return _ptr;
  }

public:
  explicit operator bool() const noexcept { return !empty(); }

private:
  pointer _ptr;
};

template<typename T, meta::alloc_arg<T> Alloc, typename... Args>
requires(meta::constructible_from<T, Args...>)
auto make_unique(alloc_arg_t, Alloc&& alloc, Args&&... args)
  -> UniquePtr<T, AllocDelete<meta::remove_cvref_t<Alloc>>> {
  T* ptr = alloc.allocate(1);
#ifdef __cpp_exceptions
  try {
#endif
    ptr = NTF_PNEW(ptr) T(forward<Args>(args)...);
#ifdef __cpp_exceptions
  } catch (...) {
    alloc.deallocate(ptr, 1);
    throw;
  }
#endif
  return ptr;
}

template<typename T, meta::mem_arg<T> Mem, typename... Args>
requires(meta::constructible_from<T, Args...>)
auto make_unique(alloc_arg_t, Mem&& mem, Args&&... args) {
  using Alloc = typename meta::remove_cvref_t<Mem>::template bind_alloc<T>;
  return make_unique<T>(alloc_arg, Alloc(mem), forward<Args>(args)...);
}

template<typename T, typename... Args>
requires(meta::constructible_from<T, Args...>)
UniquePtr<T> make_unique(Args&&... args) {
  return make_unique<T>(alloc_arg, DefaultAlloc<T>{}, forward<Args>(args)...);
}

template<typename T, typename Deleter = DefaultDelete<T>>
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

  using reverse_iterator = impl::reverse_iter_wrap<iterator>;
  using const_reverse_iterator = impl::reverse_iter_wrap<const_iterator>;

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

  ~UniqueArray() noexcept {
    static_assert(meta::nothrow_destructible<T>, "T has to be nothrow destructible");
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

template<meta::default_constructible T, meta::alloc_arg<T> Alloc>
auto make_unique_array(alloc_arg_t, Alloc&& alloc, size_t n)
  -> UniqueArray<T, AllocDelete<meta::remove_cvref_t<Alloc>>> {
  AllocDelete<meta::remove_cvref_t<Alloc>> deleter{alloc};
  T* ptr = alloc.allocate(n);
#ifdef __cpp_exceptions
  try {
#endif
    ptr = impl::construct_array(ptr, n);
#ifdef __cpp_exceptions
  } catch (...) {
    alloc.deallocate(ptr, n);
    throw;
  }
#endif
  return UniqueArray<T, AllocDelete<meta::remove_cvref_t<Alloc>>>(ptr, n, deleter);
}

template<meta::default_constructible T, meta::mem_arg<T> Mem>
auto make_unique_array(alloc_arg_t, Mem&& mem, size_t n)
  -> UniqueArray<T, AllocDelete<typename meta::remove_cvref_t<Mem>::template bind_alloc<T>>> {
  using Alloc = typename meta::remove_cvref_t<Mem>::template bind_alloc<T>;
  return make_unique_array<T>(alloc_arg, Alloc{mem}, n);
}

template<meta::default_constructible T>
UniqueArray<T> make_unique_array(size_t n) {
  return make_unique_array<T>(alloc_arg, DefaultAlloc<T>{}, n);
}

template<meta::copy_constructible T, meta::alloc_arg<T> Alloc>
auto make_unique_array(alloc_arg_t, Alloc&& alloc, size_t n, const T& other)
  -> UniqueArray<T, AllocDelete<meta::remove_cvref_t<Alloc>>> {
  AllocDelete<meta::remove_cvref_t<Alloc>> deleter{alloc};
  T* ptr = alloc.allocate(n);
#ifdef __cpp_exceptions
  try {
#endif
    ptr = impl::construct_array(ptr, n, other);
#ifdef __cpp_exceptions
  } catch (...) {
    alloc.deallocate(ptr, n);
    throw;
  }
#endif
  return UniqueArray<T, AllocDelete<meta::remove_cvref_t<Alloc>>>(ptr, n, deleter);
}

template<meta::copy_constructible T, meta::mem_arg<T> Mem>
auto make_unique_array(alloc_arg_t, Mem&& mem, size_t n, const T& other)
  -> UniqueArray<T, AllocDelete<typename meta::remove_cvref_t<Mem>::template bind_alloc<T>>> {
  using Alloc = typename meta::remove_cvref_t<Mem>::template bind_alloc<T>;
  return make_unique_array<T>(alloc_arg, Alloc{mem}, n, other);
}

template<meta::copy_constructible T>
UniqueArray<T> make_unique_array(size_t n, const T& other) {
  return make_unique_array<T>(alloc_arg, DefaultAlloc<T>{}, n, other);
}

template<meta::trivially_constructible T, meta::alloc_arg<T> Alloc>
auto make_unique_array(alloc_arg_t, Alloc&& alloc, uninitialized_t, size_t n)
  -> UniqueArray<T, AllocDelete<meta::remove_cvref_t<Alloc>>> {
  AllocDelete<meta::remove_cvref_t<Alloc>> deleter{alloc};
  T* ptr = alloc.allocate(n);
  return UniqueArray<T, AllocDelete<meta::remove_cvref_t<Alloc>>>(launder(ptr), n, deleter);
}

template<meta::trivially_constructible T, meta::mem_arg<T> Mem>
auto make_unique_array(alloc_arg_t, Mem&& mem, uninitialized_t, size_t n)
  -> UniqueArray<T, AllocDelete<typename meta::remove_cvref_t<Mem>::template bind_alloc<T>>> {
  using Alloc = typename meta::remove_cvref_t<Mem>::template bind_alloc<T>;
  return make_unique_array<T>(alloc_arg, Alloc{mem}, uninitialized, n);
}

template<meta::trivially_constructible T>
UniqueArray<T> make_unique_array(uninitialized_t, size_t n) {
  return make_unique_array<T>(alloc_arg, DefaultAlloc<T>{}, uninitialized, n);
}

} // namespace ntf

#endif // NTF_UNIQUE_HPP_
