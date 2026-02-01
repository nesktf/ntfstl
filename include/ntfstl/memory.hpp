#pragma once

#include <ntfstl/expected.hpp>
#include <ntfstl/meta.hpp>

#include <concepts>
#include <functional>
#include <type_traits>

namespace ntf::meta {

template<typename Pool>
concept memory_pool_type =
  requires(Pool& pool, const Pool& const_pool, typename Pool::size_type size,
           typename Pool::size_type alignment, void* ptr) {
    { pool.allocate(size, alignment) } -> std::same_as<void*>;
    { pool.deallocate(ptr, size) } -> std::same_as<void>;
    { const_pool.is_equal(const_pool) } -> std::same_as<bool>;
    requires noexcept(const_pool.is_equal(const_pool));
    requires noexcept(pool.deallocate(ptr, size));
  };

template<typename Pool>
concept bulk_memory_pool_type =
  memory_pool_type<Pool> && requires(Pool& pool, typename Pool::size_type size,
                                     typename Pool::size_type alignment, void* ptr) {
    {
      pool.bulk_allocate(size, alignment)
    } -> std::same_as<std::pair<void*, typename Pool::size_type>>;
    { pool.bulk_deallocate(ptr, size) } -> std::same_as<void>;
    requires noexcept(pool.bulk_deallocate(ptr, size));
  };

template<typename Alloc, typename T>
concept allocator_type = requires(Alloc& alloc, const Alloc& const_alloc,
                                  std::add_pointer_t<T> ptr, typename Alloc::size_type n) {
  { alloc.allocate(n) } -> std::convertible_to<std::add_pointer_t<T>>;
  { alloc.deallocate(ptr, n) } -> std::same_as<void>;
  { const_alloc == const_alloc } -> std::same_as<bool>;
  requires std::same_as<typename Alloc::value_type, T>;
};

template<typename Deleter, typename T>
concept array_deleter_type = requires(Deleter& del, T* arr, typename Deleter::size_type n) {
  { std::invoke(del, arr, n) } -> std::same_as<void>;
} || std::same_as<Deleter, std::default_delete<T[]>>;

template<typename Deleter, typename T>
concept pointer_deleter_type = requires(Deleter& del, T* ptr) {
  { std::invoke(del, ptr) } -> std::same_as<void>;
};

// Avoid using std::allocator_traits for templates with non-type arguments
template<typename Alloc, typename T>
struct rebind_alloc;

template<typename Alloc, typename T>
requires requires() { typename Alloc::template rebind<T>; }
struct rebind_alloc<Alloc, T> {
  using type = typename Alloc::template rebind<T>;
};

template<typename Alloc, typename T>
struct rebind_alloc : public rebind_first_arg<Alloc, T> {};

template<typename Alloc, typename T>
using rebind_alloc_t = typename rebind_alloc<Alloc, T>::type;

template<typename Deleter, typename T>
struct rebind_deleter;

template<typename Deleter, typename T>
requires requires() { typename Deleter::template rebind<T>; }
struct rebind_deleter<Deleter, T> {
  using type = typename Deleter::template rebind<T>;
};

template<typename Deleter, typename T>
struct rebind_deleter : public rebind_first_arg<Deleter, T> {};

template<typename Deleter, typename T>
using rebind_deleter_t = rebind_deleter<Deleter, T>::type;

} // namespace ntf::meta

namespace ntf::impl {

template<typename T, typename Arena>
class arena_allocator {
public:
  using arena_type = Arena;
  using value_type = T;
  using pointer = T*;
  using size_type = typename Arena::size_type;
  using difference_type = typename Arena::difference_type;

  template<typename U>
  using rebind = arena_allocator<U, arena_type>;

public:
  constexpr arena_allocator(arena_type& arena) noexcept : _arena(&arena) {}

  constexpr arena_allocator(const arena_allocator& arena) noexcept = default;

  template<typename U>
  requires(!std::same_as<T, U>)
  constexpr arena_allocator(const rebind<U>& other) noexcept : _arena(other.get_arena()) {}

public:
  constexpr pointer allocate(size_type n) {
    return static_cast<pointer>(_arena->allocate(n * sizeof(value_type), alignof(value_type)));
  }

  constexpr void deallocate(pointer ptr, size_type n) noexcept {
    NTF_UNUSED(ptr);
    NTF_UNUSED(n);
  }

public:
  constexpr arena_type& get_arena() const noexcept { return *_arena; }

public:
  template<typename U>
  constexpr bool operator==(const rebind<U>& other) const noexcept {
    return _arena->is_equal(other.get_arena());
  }

  template<typename U>
  constexpr bool operator!=(const rebind<U>& other) const noexcept {
    return !(*this == other);
  }

private:
  arena_type* _arena;
};

template<typename T, typename Arena>
class arena_deleter {
public:
  using arena_type = Arena;
  using value_type = T;
  using pointer = T*;
  using size_type = typename Arena::size_type;
  using difference_type = typename Arena::difference_type;

public:
  template<typename U>
  using rebind = arena_deleter<U, arena_type>;

public:
  constexpr arena_deleter(arena_type& arena) noexcept : _arena(&arena) {}

  constexpr arena_deleter(const arena_deleter& deleter) noexcept = default;

  template<typename U>
  requires(!std::same_as<T, U>)
  constexpr arena_deleter(const rebind<U>& other) noexcept : _arena(other.get_arena()) {}

public:
  template<typename U = T>
  requires(std::convertible_to<T*, U*>)
  constexpr void operator()(U* ptr) noexcept(std::is_nothrow_destructible_v<T>) {
    static_assert(meta::is_complete<T>, "Can't delete incomplete type");
    if constexpr (!std::is_trivially_destructible_v<T>) {
      std::destroy_at(static_cast<T*>(ptr));
    }
  }

  template<typename U = T>
  requires(std::convertible_to<T*, U*>)
  constexpr void operator()(U* ptr, size_type n) noexcept(std::is_nothrow_destructible_v<T>) {
    static_assert(meta::is_complete<T>, "Can't delete incomplete type");
    if constexpr (!std::is_trivially_destructible_v<T>) {
      std::destroy_n(static_cast<T*>(ptr), n);
    }
  }

public:
  template<typename U>
  constexpr bool operator==(const rebind<U>& other) noexcept {
    return _arena->is_equal(other.get_arena());
  }

  template<typename U>
  constexpr bool operator!=(const rebind<U>& other) noexcept {
    return !(*this == other);
  }

public:
  constexpr arena_type& get_arena() const noexcept { return *_arena; }

private:
  arena_type* _arena;
};

template<typename Derived, typename size_type>
struct memory_pool_ops {
  template<typename T, typename... Args>
  T* construct(Args&&... args) {
    auto& self = static_cast<Derived&>(*this);
    T* ptr = static_cast<T*>(self.allocate(sizeof(T), alignof(T)));
    NTF_THROW_IF(!ptr, std::bad_alloc());
    new (ptr) T(std::forward<Args>(args)...);
    return ptr;
  }

  template<typename T>
  requires(std::copy_constructible<T>)
  T* construct_n(size_type n, const T& copy) {
    auto& self = static_cast<Derived&>(*this);
    T* ptr = static_cast<T*>(self.allocate(n * sizeof(T), alignof(T)));
    NTF_THROW_IF(!ptr, std::bad_alloc());
    for (size_type i = 0; i < n; ++i) {
      new (ptr + i) T(copy);
    }
    return ptr;
  }

  template<typename T>
  requires(std::is_default_constructible_v<T>)
  T* construct_n(size_type n) {
    auto& self = static_cast<Derived&>(*this);
    T* ptr = static_cast<T*>(self.allocate(n * sizeof(T), alignof(T)));
    NTF_THROW_IF(!ptr, std::bad_alloc());
    for (size_type i = 0; i < n; ++i) {
      new (ptr + i) T();
    }
    return ptr;
  }

  template<typename T>
  requires(std::is_trivially_constructible_v<T>)
  T* construct_n(ntf::uninitialized_t, size_type n) {
    auto& self = static_cast<Derived&>(*this);
    T* ptr = static_cast<T*>(self.allocate(n * sizeof(T), alignof(T)));
    NTF_THROW_IF(!ptr, std::bad_alloc());
    return ptr;
  }

  template<typename T>
  void destroy(T* ptr) noexcept(std::is_nothrow_destructible_v<T>) {
    auto& self = static_cast<Derived&>(*this);
    std::destroy_at(ptr);
    self.deallocate(static_cast<void*>(ptr), sizeof(ptr));
  }

  template<typename T>
  void destroy_n(T* ptr, size_type n) noexcept(std::is_nothrow_destructible_v<T>) {
    auto& self = static_cast<Derived&>(*this);
    std::destroy_n(ptr, n);
    self.deallocate(static_cast<void*>(ptr), sizeof(ptr));
  }
};

template<typename T, typename Alloc>
struct allocator_delete_dealloc : private Alloc {
  allocator_delete_dealloc() = default;

  allocator_delete_dealloc(const Alloc& alloc) noexcept(noexcept(Alloc(alloc))) : Alloc(alloc) {}

  template<typename U>
  allocator_delete_dealloc(
    std::in_place_type_t<U>,
    const ::ntf::meta::rebind_alloc_t<Alloc, U>& other) noexcept(noexcept(Alloc(other))) :
      Alloc(other) {}

  void deallocate(T* ptr, typename std::allocator_traits<Alloc>::size_type count) noexcept(
    noexcept(Alloc::deallocate(ptr, count))) {
    Alloc::deallocate(ptr, count);
  }

  Alloc& get() noexcept { return static_cast<Alloc&>(*this); }

  const Alloc& get() const noexcept { return static_cast<const Alloc&>(*this); }
};

} // namespace ntf::impl

namespace ntf::mem {

constexpr inline u64 kibs(u64 count) noexcept {
  return count << 10;
}

constexpr inline u64 mibs(u64 count) noexcept {
  return count << 20;
}

constexpr inline u64 gibs(u64 count) noexcept {
  return count << 30;
}

constexpr inline u64 tibs(u64 count) noexcept {
  return count << 40;
}

constexpr inline size_t align_fw_adjust(void* ptr, size_t align) noexcept {
  uintptr_t iptr = std::bit_cast<uintptr_t>(ptr);
  // return ((iptr - 1u + align) & -align) - iptr;
  return align - (iptr & (align - 1u));
}

constexpr inline void* ptr_add(void* p, uintptr_t sz) noexcept {
  return std::bit_cast<void*>(std::bit_cast<uintptr_t>(p) + sz);
}

template<typename T, typename Alloc>
class allocator_delete : private ::ntf::impl::allocator_delete_dealloc<T, Alloc> {
public:
  using size_type = typename std::allocator_traits<Alloc>::size_type;
  using difference_type = typename std::allocator_traits<Alloc>::difference_type;
  using allocator_type = Alloc;

public:
  template<typename U>
  using rebind = allocator_delete<U, ::ntf::meta::rebind_alloc_t<Alloc, U>>;

private:
  using base_t = ::ntf::impl::allocator_delete_dealloc<T, Alloc>;

public:
  allocator_delete() noexcept(std::is_nothrow_default_constructible_v<Alloc>)
  requires(std::is_default_constructible_v<Alloc>)
      : base_t() {}

  allocator_delete(const allocator_type& alloc) noexcept(
    std::is_nothrow_copy_constructible_v<Alloc>)
  requires(std::copy_constructible<Alloc>)
      : base_t(alloc) {}

  allocator_delete(const allocator_delete& other) noexcept(
    std::is_nothrow_copy_constructible_v<Alloc>)
  requires(std::copy_constructible<Alloc>)
      : base_t(other.get_allocator()) {}

  template<typename U>
  requires(!std::same_as<T, U>)
  allocator_delete(const rebind<U>& other) noexcept(noexcept(base_t(std::in_place_type_t<U>{},
                                                                    other.get_allocator()))) :
      base_t(std::in_place_type_t<U>{}, other.get_allocator()) {}

public:
  template<typename U = T>
  requires(std::convertible_to<T*, U*>)
  void operator()(U* ptr) noexcept(std::is_nothrow_destructible_v<T> &&
                                   noexcept(base_t::deallocate(static_cast<T*>(ptr)))) {
    static_assert(meta::is_complete<T>, "Can't delete incomplete type");
    if constexpr (!std::is_trivially_destructible_v<T>) {
      std::destroy_at(static_cast<T*>(ptr));
    }
    base_t::deallocate(ptr, 1u);
  }

  template<typename U = T>
  requires(std::convertible_to<T*, U*>)
  void operator()(U* ptr,
                  size_type n) noexcept(std::is_nothrow_destructible_v<T> &&
                                        noexcept(base_t::deallocate(static_cast<T*>(ptr), n))) {
    static_assert(meta::is_complete<T>, "Can't delete incomplete type");
    if constexpr (!std::is_trivially_destructible_v<T>) {
      std::destroy_n(static_cast<T*>(ptr), n);
    }
    base_t::deallocate(ptr, n);
  }

public:
  allocator_type& get_allocator() noexcept { return base_t::get(); }

  const allocator_type& get_allocator() const noexcept { return base_t::get(); }
};

struct memory_pool {
  virtual ~memory_pool() = default;
  virtual void* allocate(size_t size, size_t alignment) = 0;
  virtual void deallocate(void* ptr, size_t size) noexcept = 0;
  virtual std::pair<void*, size_t> bulk_allocate(size_t size, size_t alignment) = 0;
  virtual void bulk_deallocate(void* ptr, size_t size) noexcept = 0;

  virtual bool is_equal(const memory_pool& other) const noexcept { return true; }
};

template<typename T>
class pool_allocator {
public:
  using value_type = T;
  using pointer = T*;
  using size_type = size_t;
  using difference_type = ptrdiff_t;

  template<typename U>
  using rebind = pool_allocator<U>;

public:
  pool_allocator(pool_allocator& pool) noexcept : _pool(&pool) {}

  pool_allocator(const pool_allocator& other) noexcept = default;

  template<typename U>
  requires(!std::same_as<T, U>)
  pool_allocator(const rebind<U>& other) noexcept : _pool(other.get_pool()) {}

public:
  pointer allocate(size_type n) {
    void* ptr = _pool->allocate(n * sizeof(value_type), alignof(value_type));
    NTF_THROW_IF(!ptr, std::bad_alloc());
    return static_cast<pointer>(ptr);
  }

  void deallocate(pointer ptr, size_type n) noexcept {
    _pool->deallocate(static_cast<void*>(ptr), n * sizeof(value_type));
  }

public:
  memory_pool& get_pool() const noexcept { return *_pool; }

public:
  template<typename U>
  constexpr bool operator==(const rebind<U>& other) const noexcept {
    return _pool->is_equal(other.get_pool());
  }

  template<typename U>
  bool operator!=(const rebind<U>& other) const noexcept {
    return !(*this == other);
  }

private:
  memory_pool* _pool;
};

template<typename T>
class pool_deleter {
public:
  using value_type = T;
  using pointer = T*;
  using size_type = size_t;
  using difference_type = ptrdiff_t;

public:
  template<typename U>
  using rebind = pool_deleter<U>;

public:
  constexpr pool_deleter(memory_pool& pool) noexcept : _pool(&pool) {}

  constexpr pool_deleter(const pool_deleter& deleter) noexcept = default;

  template<typename U>
  requires(!std::same_as<T, U>)
  constexpr pool_deleter(const rebind<U>& other) noexcept : _pool(&other.get_pool()) {}

public:
  template<typename U = T>
  requires(std::convertible_to<T*, U*>)
  constexpr void operator()(U* ptr) noexcept(std::is_nothrow_destructible_v<T>) {
    static_assert(meta::is_complete<T>, "Can't delete incomplete type");
    if constexpr (!std::is_trivially_destructible_v<T>) {
      std::destroy_at(ptr);
    }
    _pool->deallocate(static_cast<void*>(ptr), sizeof(T));
  }

  template<typename U = T>
  requires(std::convertible_to<T*, U*>)
  constexpr void operator()(U* ptr, size_type n) noexcept(std::is_nothrow_destructible_v<T>) {
    static_assert(meta::is_complete<T>, "Can't delete incomplete type");
    if constexpr (!std::is_trivially_destructible_v<T>) {
      std::destroy_n(ptr, n);
    }
    _pool->deallocate(static_cast<void*>(ptr), sizeof(T));
  }

public:
  template<typename U>
  constexpr bool operator==(const rebind<U>& other) noexcept {
    return _pool->is_equal(other.get_pool());
  }

  template<typename U>
  constexpr bool operator!=(const rebind<U>& other) noexcept {
    return !(*this == other);
  }

public:
  constexpr memory_pool& get_pool() const noexcept { return *_pool; }

private:
  memory_pool* _pool;
};

class default_pool :
    public memory_pool,
    public ::ntf::impl::memory_pool_ops<default_pool, size_t> {
public:
  using size_type = size_t;
  using difference_type = ptrdiff_t;

public:
  template<typename T>
  class allocator {
  public:
    using value_type = T;
    using pointer = T*;
    using size_type = default_pool::size_type;
    using difference_type = default_pool::difference_type;

    template<typename U>
    using rebind = allocator<U>;

  public:
    constexpr allocator() noexcept = default;

    constexpr allocator(const allocator& alloc) noexcept = default;

    template<typename U>
    requires(!std::same_as<T, U>)
    constexpr allocator(const rebind<U>&) noexcept {};

  public:
    constexpr pointer allocate(size_type n) {
      return static_cast<pointer>(
        default_pool::instance().allocate(n * sizeof(value_type), alignof(value_type)));
    }

    constexpr void deallocate(pointer ptr, size_type n) noexcept {
      default_pool::instance().deallocate(static_cast<void*>(ptr), n * sizeof(value_type));
    }

  public:
    template<typename U>
    constexpr bool operator==(const rebind<U>&) const noexcept {
      return true;
    }

    template<typename U>
    constexpr bool operator!=(const rebind<U>&) const noexcept {
      return false;
    }
  };

  template<typename T>
  using deleter = ::ntf::mem::allocator_delete<T, allocator<T>>;

  /*
  template<typename T>
  class deleter {
  public:
    using value_type = T;
    using pointer = T*;
    using size_type = default_pool::size_type;
    using difference_type = default_pool::difference_type;

  public:
    template<typename U>
    using rebind = deleter<U>;

  public:
    constexpr deleter() noexcept = default;

    constexpr deleter(const deleter& deleter) noexcept = default;

    template<typename U>
    requires(!std::same_as<T, U>)
    constexpr deleter(const rebind<U>& other) noexcept {}

  public:
    template<typename U = T>
    requires(std::convertible_to<T*, U*>)
    constexpr void operator()(U* ptr) noexcept(std::is_nothrow_destructible_v<T>) {
      static_assert(meta::is_complete<T>, "Can't delete incomplete type");
      if constexpr (!std::is_trivially_destructible_v<T>) {
        std::destroy_at(ptr);
      }
      default_pool::instance().deallocate(static_cast<void*>(ptr), sizeof(T));
    }

    template<typename U = T>
    requires(std::convertible_to<T*, U*>)
    constexpr void operator()(U* ptr, size_type n) noexcept(std::is_nothrow_destructible_v<T>) {
      static_assert(meta::is_complete<T>, "Can't delete incomplete type");
      if constexpr (!std::is_trivially_destructible_v<T>) {
        std::destroy_n(ptr, n);
      }
      default_pool::instance().deallocate(static_cast<void*>(ptr), sizeof(T));
    }

  public:
    template<typename U>
    constexpr bool operator==(const rebind<U>& other) noexcept {
      return true;
    }

    template<typename U>
    constexpr bool operator!=(const rebind<U>& other) noexcept {
      return false;
    }
  };
  */

private:
  default_pool() noexcept = default;

public:
  static default_pool& instance() noexcept;
  static size_type total_allocated() noexcept;
  static size_type total_bulk_allocated() noexcept;

public:
  void* allocate(size_type size, size_type alignment) override;
  void deallocate(void* ptr, size_type size) noexcept override;
  std::pair<void*, size_type> bulk_allocate(size_type size, size_type alignment) override;
  void bulk_deallocate(void* ptr, size_type size) noexcept override;
};

static_assert(::ntf::meta::memory_pool_type<default_pool>);
static_assert(::ntf::meta::bulk_memory_pool_type<default_pool>);

size_t system_page_size() noexcept;

class growing_arena : public ::ntf::impl::memory_pool_ops<growing_arena, size_t> {
public:
  using size_type = size_t;
  using difference_type = ptrdiff_t;

public:
  template<typename T>
  using allocator = ::ntf::impl::arena_allocator<T, growing_arena>;

  template<typename T>
  using deleter = ::ntf::impl::arena_deleter<T, growing_arena>;

private:
  struct create_t {};

public:
  growing_arena(create_t, void* data, size_type block_size) noexcept;
  growing_arena(size_type initial_size);
  growing_arena(memory_pool& pool, size_type initial_size);

  NTF_DECLARE_MOVE_ONLY(growing_arena);

public:
  static ntf::expected<growing_arena, std::bad_alloc>
  with_initial_size(size_type initial_size) noexcept;

  static ntf::expected<growing_arena, std::bad_alloc> using_pool(memory_pool& pool,
                                                                 size_type initial_size) noexcept;

public:
  void* allocate(size_type size, size_type alignment);

  void deallocate(void* ptr, size_type size) noexcept {
    NTF_UNUSED(ptr);
    NTF_UNUSED(size);
  }

  bool is_equal(const growing_arena& other) const noexcept;

  void clear() noexcept;

public:
  size_type used() const noexcept { return _used; }

  size_type allocated() const noexcept { return _allocated; }

private:
  void* _data;
  size_type _used;
  size_type _allocated;
};

static_assert(::ntf::meta::memory_pool_type<growing_arena>);

class fixed_arena : public ::ntf::impl::memory_pool_ops<fixed_arena, size_t> {
public:
  using size_type = size_t;
  using difference_type = ptrdiff_t;

public:
  template<typename T>
  using allocator = ::ntf::impl::arena_allocator<T, fixed_arena>;

  template<typename T>
  using deleter = ::ntf::impl::arena_deleter<T, fixed_arena>;

private:
  struct create_t {};

public:
  fixed_arena(create_t, void* data, size_type block_size) noexcept;
  fixed_arena(size_type capacity);
  fixed_arena(memory_pool& pool, size_type capacity);

  NTF_DECLARE_MOVE_ONLY(fixed_arena);

public:
  static ntf::expected<fixed_arena, std::bad_alloc> with_capacity(size_type capacity) noexcept;

  static ntf::expected<fixed_arena, std::bad_alloc> using_pool(memory_pool& pool,
                                                               size_type capcity) noexcept;

public:
  void* allocate(size_type size, size_type alignment);

  void deallocate(void* ptr, size_type size) noexcept {
    NTF_UNUSED(ptr);
    NTF_UNUSED(size);
  }

  bool is_equal(const fixed_arena& other) const noexcept;

  void clear() noexcept;

public:
  size_type used() const noexcept { return _used; }

  size_type allocated() const noexcept { return _allocated; }

private:
  void* _data;
  size_type _used;
  size_type _allocated;
};

static_assert(::ntf::meta::memory_pool_type<fixed_arena>);

template<size_t BufferSize, size_t MaxAlign = alignof(std::max_align_t)>
requires(BufferSize > 0u && MaxAlign > 0u)
class stack_arena :
    public ::ntf::impl::memory_pool_ops<stack_arena<BufferSize, MaxAlign>, size_t> {
public:
  using size_type = size_t;
  using difference_type = ptrdiff_t;

  static constexpr size_type buffer_size = BufferSize;
  static constexpr size_type max_alignment = MaxAlign;

public:
  template<typename T>
  using allocator = ::ntf::impl::arena_allocator<T, fixed_arena>;

  template<typename T>
  using deleter = ::ntf::impl::arena_deleter<T, fixed_arena>;

public:
  constexpr stack_arena() noexcept : _used() {}

public:
  constexpr void* allocate(size_type size, size_type alignment) {
    void* ptr = static_cast<void*>(&_buffer[0]);
    const size_type available = buffer_size - _used;
    const size_type padding = align_fw_adjust(ptr_add(ptr, _used), alignment);
    const size_type required = padding + size;
    NTF_THROW_IF(available < required, std::bad_alloc());
    ptr = ptr_add(ptr, _used + padding);
    _used += required;
    return ptr;
  }

  constexpr void deallocate(void* ptr, size_type size) noexcept {
    NTF_UNUSED(ptr);
    NTF_UNUSED(size);
  }

  constexpr bool is_equal(const stack_arena& other) const noexcept { return true; }

  constexpr void clear() noexcept { _used = 0; }

public:
  constexpr size_type used() const noexcept { return _used; }

  constexpr size_type capacity() const noexcept { return buffer_size; }

private:
  alignas(max_alignment) u8 _buffer[buffer_size];
  size_type _used;
};

} // namespace ntf::mem
