#pragma once

#include <ntfstl/expected.hpp>

namespace ntf {

namespace meta {

template<typename T>
concept allocable_type = !std::is_void_v<T> && !std::is_reference_v<T> && is_complete<T>;

template<typename T >
concept allocator_pool_type = requires(T& a, size_t sz, size_t align, void* ptr, const T& b) {
  { a.allocate(sz, align) } -> std::same_as<void*>;
  { a.deallocate(ptr, sz) } -> std::same_as<void>;
  { b.is_equal(b) } -> std::convertible_to<bool>;
  requires noexcept(b.is_equal(b));
  requires noexcept(a.deallocate(ptr, sz));
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

typedef void*(*malloc_fn_t)(void* user_ptr, size_t size, size_t align);
typedef void(*free_fn_t)(void* user_ptr, void* mem, size_t size);

// For checking default size in virtual_allocator, and maybe C interoperability
struct malloc_funcs {
  void* user_ptr;
  malloc_fn_t mem_alloc;
  free_fn_t mem_free;
};

template<meta::allocator_pool_type P>
malloc_funcs make_pool_funcs(P& pool) noexcept {
  return {
    .user_ptr = std::addressof(pool),
    .mem_alloc = +[](void* user_ptr, size_t size, size_t align) -> void* {
      auto& mem_pool = *std::launder(reinterpret_cast<P*>(user_ptr));
      return mem_pool.allocate(size, align);
    },
    .mem_free = +[](void* user_ptr, void* mem, size_t size) -> void {
      auto& mem_pool = *std::launder(reinterpret_cast<P*>(user_ptr));
      mem_pool.deallocate(mem, size);
    },
  };
}

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

  constexpr void deallocate(void* ptr, size_t size) noexcept {
    _pool->deallocate(ptr, size);
  }

  constexpr bool is_equal(const mempool_view& other) const noexcept {
    return _pool->is_equal(other);
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
  malloc_pool() noexcept = default;

public:
  void* allocate(size_t size, size_t align) const noexcept;
  void deallocate(void* mem, size_t size) const noexcept;
  bool is_equal(const malloc_pool& other) const noexcept;

public:
  static void* malloc_fn(void* user_ptr, size_t size, size_t align) noexcept;
  static void free_fn(void* user_ptr, void* mem, size_t size) noexcept;
};
static_assert(meta::allocator_pool_type<malloc_pool>);

// Fixed size arena
class fixed_arena : public impl::mempool_ops<fixed_arena> {
public:
  fixed_arena(void* user_ptr, free_fn_t free_fn,
              void* block, size_t block_sz) noexcept;

public:
  static expected<fixed_arena, std::bad_alloc> from_size(size_t size) noexcept;
  static expected<fixed_arena, std::bad_alloc> from_extern(malloc_funcs func, size_t sz) noexcept;

public:
  void* allocate(size_t size, size_t align);
  void deallocate(void* mem, size_t size) noexcept;
  bool is_equal(const fixed_arena& other) const noexcept;

  void clear() noexcept;

public:
  size_t size() const noexcept { return _used; }
  size_t capacity() const noexcept { return _allocated; }
  void* data() { return _block; }

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
  linked_arena(void* user_ptr, malloc_fn_t malloc_fn, free_fn_t free_fn,
               void* block, size_t block_sz) noexcept;

public:
  static expected<linked_arena, std::bad_alloc> from_size(size_t size) noexcept;
  static expected<linked_arena, std::bad_alloc> from_extern(malloc_funcs func, size_t sz) noexcept;

public:
  void* allocate(size_t size, size_t alignment) noexcept;
  void deallocate(void* mem, size_t size) noexcept;
  bool is_equal(const linked_arena& other) const noexcept;

public:
  void clear() noexcept;

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
template<size_t buffer_sz, size_t max_align = alignof(std::max_align_t)>
requires(buffer_sz > 0u && max_align > 0u)
class stack_arena : public impl::mempool_ops<stack_arena<buffer_sz, max_align>> {
public:
  static constexpr size_t BUFFER_SIZE = buffer_sz;

public:
  constexpr stack_arena() noexcept :
    _used{0u} {}

public:
  constexpr void* allocate(size_t size, size_t alignment) noexcept(NTF_NOEXCEPT) {
    const size_t available = BUFFER_SIZE-_used;
    const size_t padding = align_fw_adjust(ptr_add(data(), _used), alignment);
    const size_t required = padding+size;
    if (available < required) {
      if (std::is_constant_evaluated()) {
        return nullptr;
      } else {
        NTF_THROW(std::bad_alloc);
      }
    }
    void* ptr = ptr_add(data(), _used+padding);
    _used += required;
    return ptr;
  }
  constexpr void deallocate(void*, size_t) noexcept {}
  constexpr bool is_equal(const malloc_pool& other) const noexcept { return (&other == this); }

  constexpr void clear() noexcept { _used = 0u; }

public:
  constexpr size_t size() const noexcept { return _used; }
  constexpr size_t capacity() const noexcept { return BUFFER_SIZE; }
  constexpr void* data() noexcept { return static_cast<void*>(&_buffer[0]); }
    
private:
  alignas(max_align) uint8 _buffer[BUFFER_SIZE];
  size_t _used;
};

} // namespace ntf
