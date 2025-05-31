#pragma once

#include <ntfstl/expected.hpp>

namespace ntf {

namespace meta {

template<typename T>
concept allocable_type = !std::is_void_v<T> && !std::is_reference_v<T> && is_complete<T>;

template<typename T >
concept allocator_pool_type = requires(T& pool, size_t sz, size_t align, void* ptr) {
  { pool.allocate(sz, align) } -> std::same_as<void*>;
  { pool.deallocate(ptr, sz) } -> std::same_as<void>;
};

} // namespace meta

namespace impl {

template<typename Derived>
struct mempool_ops {
  template<typename T>
  constexpr T* allocate_uninited(size_t n) {
    auto& self = static_cast<Derived&>(*this);
    return static_cast<T*>(self.allocate(n*sizeof(T), alignof(T)));
  }

  template<typename T, typename... Args>
  constexpr T* construct(Args&&... args) {
    T* ptr = mempool_ops::template allocate_uninited<T>(1u);
    new (ptr) T(std::forward<Args>(args)...);
    return ptr;
  }

  template<typename T, typename U, typename... Args>
  constexpr T* construct(std::initializer_list<U> il, Args&&... args) {
    T* ptr = mempool_ops::template allocate_uninited<T>(1u);
    new (ptr) T(il, std::forward<Args>(args)...);
    return ptr;
  }

  template<typename T>
  constexpr void destroy(T* ptr) {
    auto& self = static_cast<Derived&>(*this);
    std::destroy_at(ptr);
    self.deallocate(ptr, sizeof(T));
  }
};

} // namespace impl

constexpr uint64 kibs(uint64 count) { return count << 10; }
constexpr uint64 mibs(uint64 count) { return count << 20; }
constexpr uint64 gibs(uint64 count) { return count << 30; }
constexpr uint64 tibs(uint64 count) { return count << 40; }

constexpr size_t align_fw_adjust(void* ptr, size_t align) noexcept {
  uintptr_t iptr = std::bit_cast<uintptr_t>(ptr);
  // return ((iptr - 1u + align) & -align) - iptr;
  return align - (iptr & (align - 1u));
}

constexpr void* ptr_add(void* p, uintptr_t sz) noexcept {
  return std::bit_cast<void*>(std::bit_cast<uintptr_t>(p)+sz);
}

template<meta::allocable_type T, meta::allocator_pool_type MemPool>
class allocator_adaptor : public impl::mempool_ops<allocator_adaptor<T, MemPool>> {
public:
  using value_type = T;
  using pointer = T*;
  using size_type = size_t;
  using difference_Type = ptrdiff_t;

  template<typename U>
  using rebind = allocator_adaptor<U, MemPool>;

private:
  template<meta::allocable_type U, meta::allocator_pool_type MemPoolU>
  friend class allocator_adaptor;

public:
  constexpr allocator_adaptor(MemPool& pool) noexcept :
    _pool{std::addressof(pool)} {}

  constexpr allocator_adaptor(const allocator_adaptor& other) noexcept :
    _pool{other._pool} {}

  template<typename U>
  requires(!std::same_as<T, U>)
  constexpr allocator_adaptor(const rebind<U>& other) noexcept :
    _pool{other._pool} {}

public:
  constexpr pointer allocate(size_type n) {
    void* ptr = _pool->allocate(n*sizeof(value_type), alignof(value_type));
    NTF_THROW_IF(!ptr, std::bad_alloc);
    return reinterpret_cast<pointer>(ptr);
  }

  constexpr void deallocate(pointer ptr, size_type n) {
    _pool->deallocate(ptr, n*sizeof(value_type));
  }

public:
  constexpr bool operator==(const allocator_adaptor& other) const noexcept {
    if constexpr (meta::has_operator_equals<MemPool>) {
      return get_pool() == other.get_pool();
    } else {
      return _pool == other._pool;
    }
  }

  template<typename U>
  requires(!std::same_as<T, U>)
  constexpr bool operator==(const rebind<U>& other) const noexcept {
    if constexpr (meta::has_operator_equals<MemPool>) {
      return get_pool() == other.get_pool();
    } else {
      return _pool == other._pool;
    }
  }

  constexpr bool operator!=(const allocator_adaptor& other) const noexcept {
    if constexpr (meta::has_operator_nequals<MemPool>) {
      return get_pool() != other.get_pool();
    } else {
      return _pool != other._pool;
    }
  }

  template<typename U>
  requires(!std::same_as<T, U>)
  constexpr bool operator!=(const rebind<U>& other) const noexcept {
    if constexpr (meta::has_operator_nequals<MemPool>) {
      return get_pool() != other.get_pool();
    } else {
      return _pool != other._pool;
    }
  }

public:
  const MemPool& get_pool() const { return *_pool; }
  MemPool& get_pool() { return *_pool; }

private:
  MemPool* _pool;
};

using malloc_fn_t = void*(*)(void* user_ptr, size_t size, size_t align);
using free_fn_t = void(*)(void* user_ptr, void* mem, size_t size);

class virtual_mem_pool : public impl::mempool_ops<virtual_mem_pool> {
public:
  template<typename T>
  using adaptor = allocator_adaptor<T, virtual_mem_pool>;

public:
  virtual_mem_pool(void* user_ptr, malloc_fn_t malloc_fun, free_fn_t free_fun) noexcept :
    _user_ptr{user_ptr}, _malloc{malloc_fun}, _free{free_fun} {}

public:
  void* allocate(size_t size, size_t align) {
    NTF_ASSERT(_malloc, "Null malloc function");
    void* ptr = std::invoke(_malloc, _user_ptr, size, align);
    return ptr;
  }

  void deallocate(void* mem, size_t size) {
    NTF_ASSERT(_free, "Null free function");
    std::invoke(_free, _user_ptr, mem, size);
  }

public:
  template<meta::allocator_pool_type P>
  static virtual_mem_pool from_pool(P& pool);

public:
  template<typename T>
  adaptor<T> make_adaptor() noexcept { return adaptor<T>{*this}; }

private:
  void* _user_ptr;
  malloc_fn_t _malloc;
  free_fn_t _free;
};
static_assert(meta::allocator_pool_type<virtual_mem_pool>);

namespace meta {

template<typename P>
concept has_virtual_pool_wrapper = requires(P pool) {
  { pool.to_virtual() } -> std::same_as<virtual_mem_pool>;
};

} // namespace meta

template<meta::allocator_pool_type P>
virtual_mem_pool virtual_mem_pool::from_pool(P& mem_pool) {
  if constexpr(meta::has_virtual_pool_wrapper<P>) {
    return mem_pool.to_virtual();
  } else {
    return virtual_mem_pool{
      static_cast<void*>(std::addressof(mem_pool)),
      +[](void* user_ptr, size_t size, size_t align) -> void* {
        P& pool = *std::launder(reinterpret_cast<P*>(user_ptr));
        pool.allocate(size, align);
      },
      +[](void* user_ptr, void* mem, size_t size) -> void {
        P& pool = *std::launder(reinterpret_cast<P*>(user_ptr));
        pool.deallocate(mem, size);
      }
    };
  }
}

class malloc_pool : public impl::mempool_ops<malloc_pool> {
public:
  template<typename T>
  using adaptor = allocator_adaptor<T, malloc_pool>;

public:
  malloc_pool() noexcept = default;

public:
  void* allocate(size_t size, size_t align) const noexcept {
    return std::aligned_alloc(align, size);
  }

  void deallocate(void* mem, size_t n) const noexcept {
    NTF_UNUSED(n);
    std::free(mem);
  }

public:
  template<typename T>
  adaptor<T> make_adaptor() noexcept { return adaptor<T>{*this}; }

public:
  static virtual_mem_pool to_virtual() noexcept {
    return virtual_mem_pool{
      nullptr,
      +[](void* user_ptr, size_t size, size_t align) noexcept -> void* {
        NTF_UNUSED(user_ptr);
        return std::aligned_alloc(align, size);
      },
      +[](void* user_ptr, void* mem, size_t size) noexcept -> void {
        NTF_UNUSED(user_ptr);
        NTF_UNUSED(size);
        std::free(mem);
      }
    };
  }
};
static_assert(meta::allocator_pool_type<malloc_pool>);

class arena_block_manager : public impl::mempool_ops<arena_block_manager> {
public:
  arena_block_manager(void* block, size_t block_sz) noexcept :
    _block{block},
    _used{0u}, _allocated{block_sz} {}

public:
  void clear() noexcept;

  void* allocate(size_t size, size_t alignment) noexcept;
  void deallocate(void*, size_t) noexcept {}

public:
  size_t size() const noexcept { return _used; }
  size_t capacity() const noexcept { return _allocated; }
  void* data() { return _block; }

protected:
  void* _block;
  size_t _used;
  size_t _allocated;
};

class fixed_arena : public arena_block_manager {
public:
  template<typename T>
  using adaptor = allocator_adaptor<T, fixed_arena>;

private:
  fixed_arena(void* base, size_t sz) noexcept :
    arena_block_manager{base, sz} {}

public:
  static expected<fixed_arena, error<void>> from_size(size_t size) noexcept;

public:
  template<typename T>
  adaptor<T> make_adaptor() noexcept { return adaptor<T>{*this}; }

public:
  NTF_DECLARE_MOVE_ONLY(fixed_arena);
};
static_assert(meta::allocator_pool_type<fixed_arena>);

class linked_arena : public impl::mempool_ops<linked_arena> {
public:
  template<typename T>
  using adaptor = allocator_adaptor<T, linked_arena>;

private:
  linked_arena(void* block, size_t block_sz) noexcept :
    _block{block}, _block_used{0u},
    _total_used{0u}, _allocated{block_sz} {}

public:
  static expected<linked_arena, error<void>> from_size(size_t size) noexcept;

public:
  void* allocate(size_t size, size_t alignment) noexcept;
  void deallocate(void*, size_t) noexcept {}

public:
  void clear() noexcept;

public:
  template<typename T>
  adaptor<T> make_adaptor() noexcept { return adaptor<T>{*this}; }

public:
  size_t size() const noexcept { return _total_used; }
  size_t capacity() const noexcept { return _allocated; }

private:
  void* _block;
  size_t _block_used;
  size_t _total_used, _allocated;

public:
  NTF_DECLARE_MOVE_ONLY(linked_arena);
};
static_assert(meta::allocator_pool_type<linked_arena>);

template<size_t buffer_sz>
requires(buffer_sz > 0u)
class static_arena {
public:
  template<typename T>
  using adaptor = allocator_adaptor<T, static_arena>;

  static constexpr size_t BUFFER_SIZE = buffer_sz;

public:
  constexpr static_arena() noexcept :
    _used{0u} {}

public:
  constexpr void* allocate(size_t size, size_t alignment) noexcept {
    const size_t available = BUFFER_SIZE-_used;
    const size_t padding = align_fw_adjust(ptr_add(data(), _used), alignment);
    const size_t required = padding+size;
    if (available < required) {
      return nullptr;
    }
    void* ptr = ptr_add(data(), _used+padding);
    _used += required;
    return ptr;
  }
  constexpr void deallocate(void*, size_t) noexcept {}

  constexpr void clear() noexcept { _used = 0u; }

  template<typename T>
  constexpr adaptor<T> make_adaptor() noexcept { return adaptor<T>{*this}; }

public:
  constexpr size_t size() const noexcept { return _used; }
  constexpr size_t capacity() const noexcept { return BUFFER_SIZE; }
  constexpr void* data() noexcept { return static_cast<void*>(&_buffer[0]); }
    
private:
  uint8 _buffer[BUFFER_SIZE];
  size_t _used;
};

} // namespace ntf
