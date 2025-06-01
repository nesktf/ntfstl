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

// Common construction operations
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

// Thin wrapper for using memory pools in standard containers directly
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

typedef void*(*malloc_fn_t)(void* user_ptr, size_t size, size_t align);
typedef void(*free_fn_t)(void* user_ptr, void* mem, size_t size);

// For checking default size in virtual_allocator, and maybe C interoperability
struct malloc_funcs {
  void* user_ptr;
  malloc_fn_t mem_alloc;
  free_fn_t mem_free;
};

// Thin wrapper for a memory pool, meant to be used in virtual_allocator or similar
template<meta::allocator_pool_type MemPoolT>
class mempool_view : public impl::mempool_ops<mempool_view<MemPoolT>> {
public:
  constexpr mempool_view(MemPoolT& pool) noexcept :
    _pool{std::addressof(pool)} {}

public:
  constexpr void* allocate(size_t size, size_t align) {
    return _pool->allocate(size, align);
  }

  constexpr void deallocate(void* ptr, size_t size){
    _pool->deallocate(ptr, size);
  }

public:
  MemPoolT& get_pool() noexcept { return *_pool; }
  const MemPoolT& get_pool() const noexcept { return *_pool; }

private:
  MemPoolT* _pool;
};

// Default stateless memory pool
class malloc_pool : public impl::mempool_ops<malloc_pool> {
public:
  template<typename T>
  using adaptor_type = allocator_adaptor<T, malloc_pool>;

public:
  malloc_pool() noexcept = default;

public:
  void* allocate(size_t size, size_t align) const noexcept;
  void deallocate(void* mem, size_t size) const noexcept;

public:
  static void* malloc_fn(void* user_ptr, size_t size, size_t align) noexcept;
  static void free_fn(void* user_ptr, void* mem, size_t size) noexcept;

public:
  template<typename T>
  adaptor_type<T> make_adaptor() noexcept { return adaptor_type<T>{*this}; }
};
static_assert(meta::allocator_pool_type<malloc_pool>);

// Fixed size arena
class fixed_arena : public impl::mempool_ops<fixed_arena> {
public:
  template<typename T>
  using adaptor_type = allocator_adaptor<T, fixed_arena>;

public:
  fixed_arena(void* user_ptr, free_fn_t free_fn,
              void* block, size_t block_sz) noexcept;

public:
  static expected<fixed_arena, std::bad_alloc> from_size(size_t size) noexcept;
  static expected<fixed_arena, std::bad_alloc> from_extern(
    void* user_ptr, malloc_fn_t malloc_fn, free_fn_t free_fn, size_t size
  ) noexcept;

public:
  void* allocate(size_t size, size_t align);
  void deallocate(void* mem, size_t size);

  void clear() noexcept;

public:
  size_t size() const noexcept { return _used; }
  size_t capacity() const noexcept { return _allocated; }
  void* data() { return _block; }

public:
  template<typename T>
  adaptor_type<T> make_adaptor() noexcept { return adaptor_type<T>{*this}; }

private:
  void _free_block() noexcept;

private:
  void* _user_ptr;
  free_fn_t _free;
  void* _block;
  size_t _used;
  size_t _allocated;

public:
  NTF_DECLARE_MOVE_ONLY(fixed_arena);
};
static_assert(meta::allocator_pool_type<fixed_arena>);

// Growing arena
class linked_arena : public impl::mempool_ops<linked_arena> {
public:
  template<typename T>
  using adaptor_type = allocator_adaptor<T, linked_arena>;

public:
  linked_arena(void* user_ptr, malloc_fn_t malloc_fn, free_fn_t free_fn,
               void* block, size_t block_sz) noexcept;

public:
  static expected<linked_arena, std::bad_alloc> from_size(size_t size) noexcept;
  static expected<linked_arena, std::bad_alloc> from_extern(
    void* user_ptr, malloc_fn_t malloc_fn, free_fn_t free_fn, size_t size
  ) noexcept;

public:
  void* allocate(size_t size, size_t alignment) noexcept;
  void deallocate(void* mem, size_t size) noexcept;

public:
  void clear() noexcept;

public:
  template<typename T>
  adaptor_type<T> make_adaptor() noexcept { return adaptor_type<T>{*this}; }

public:
  size_t size() const noexcept { return _total_used; }
  size_t capacity() const noexcept { return _allocated; }

private:
  bool _try_acquire_block(size_t size, size_t align) noexcept;
  void _free_blocks() noexcept;

private:
  void* _user_ptr;
  malloc_fn_t _malloc;
  free_fn_t _free;
  void* _block;
  size_t _block_used;
  size_t _total_used;
  size_t _allocated;

public:
  NTF_DECLARE_MOVE_ONLY(linked_arena);
};
static_assert(meta::allocator_pool_type<linked_arena>);

// Arena in the stack
template<size_t buffer_sz, size_t max_align = alignof(uint8)>
requires(buffer_sz > 0u)
class stack_arena {
public:
  template<typename T>
  using adaptor_type = allocator_adaptor<T, stack_arena>;

  static constexpr size_t BUFFER_SIZE = buffer_sz;

public:
  constexpr stack_arena() noexcept :
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
  constexpr adaptor_type<T> make_adaptor() noexcept { return adaptor_type<T>{*this}; }

public:
  constexpr size_t size() const noexcept { return _used; }
  constexpr size_t capacity() const noexcept { return BUFFER_SIZE; }
  constexpr void* data() noexcept { return static_cast<void*>(&_buffer[0]); }
    
private:
  alignas(max_align) uint8 _buffer[BUFFER_SIZE];
  size_t _used;
};

} // namespace ntf
