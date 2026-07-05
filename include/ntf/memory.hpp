#ifndef NTF_MEMORY_HPP_
#define NTF_MEMORY_HPP_

#include <ntf/impl/concepts.hpp>
#include <ntf/impl/core.hpp>

extern "C" {

NTF_DEFINE_HANDLE(ntf_Arena);

size_t ntf_system_page_size() noexcept;
int ntf_arena_init(ntf_Arena* arena, size_t capacity) noexcept;
void ntf_arena_destroy(ntf_Arena arena) noexcept;
void ntf_arena_clear(ntf_Arena arena) noexcept;
void* ntf_arena_alloc(ntf_Arena arena, size_t size, size_t align) noexcept;

} // extern "C"

enum NTF_PNEW_T {
  NTF_PNEW_TAG,
};

constexpr inline void* operator new(size_t, void* ptr, NTF_PNEW_T) {
  return ptr;
}

constexpr inline void operator delete(void*, void*, NTF_PNEW_T) {}

#define NTF_PNEW(_ptr) ::new (_ptr, ::NTF_PNEW_TAG)

namespace ntf {

struct alloc_arg_t {};

constexpr inline alloc_arg_t alloc_arg;

template<typename T, typename... Args>
T* construct_at(T* ptr, Args&&... args) {
  return NTF_PNEW(ptr) T(forward<Args>(args)...);
}

template<typename T, typename... Args>
T* construct_offset(T* ptr, size_t i, Args&&... args) {
  return NTF_PNEW(ptr + i) T(forward<Args>(args)...);
}

template<typename T>
void destroy_at(T* ptr) noexcept(meta::nothrow_destructible<T>) {
  ptr->~T();
}

template<typename T>
void destroy_offset(T* ptr, size_t i) noexcept(meta::nothrow_destructible<T>) {
  (ptr + i)->~T();
}

namespace impl {

template<meta::nothrow_default_constructible T>
constexpr T* construct_array(T* ptr, size_t n) noexcept(meta::nothrow_default_constructible<T>) {
  if constexpr (meta::nothrow_default_constructible<T>) {
    for (size_t i = 0; i < n; ++i) {
      construct_offset(ptr, i);
    }
  } else {
    i64 i = 0;
#if __cpp_exceptions
    try {
#endif
      for (; i < (i64)n; ++i) {
        construct_offset(ptr, i);
      }
#if __cpp_exceptions
    } catch (...) {
      for (; i >= 0; --i) {
        destroy_offset(ptr, i);
      }
      throw;
    }
#endif
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
#if __cpp_exceptions
    try {
#endif
      for (; i < (i64)n; ++i) {
        construct_offset(ptr, i, obj);
      }
#if __cpp_exceptions
    } catch (...) {
      for (; i >= 0; --i) {
        destroy_offset(ptr, i);
      }
      throw;
    }
#endif
  }
  return launder(ptr);
}

} // namespace impl

template<typename T>
class DefaultAlloc {
public:
  using value_type = T;
  using pointer = T*;
  using size_type = size_t;
  using difference_type = ptrdiff_t;

public:
  template<typename U>
  using rebind = DefaultAlloc<U>;

public:
  constexpr DefaultAlloc() noexcept = default;
  constexpr DefaultAlloc(const DefaultAlloc&) noexcept = default;

  template<typename U>
  requires(!meta::is_same_v<U, T>)
  constexpr DefaultAlloc(const rebind<U>&) noexcept {}

public:
  constexpr T* allocate(size_t n) {
    T* ptr = static_cast<T*>(::malloc(n * sizeof(T)));
    NTF_THROW_IF(!ptr, BadAlloc());
    return ptr;
  }

  constexpr void deallocate(T* ptr, size_t n) noexcept {
    NTF_UNUSED(n);
    ::free(ptr);
  }

public:
  constexpr bool operator==(const DefaultAlloc&) const noexcept { return true; }

  template<typename U>
  requires(!meta::is_same_v<U, T>)
  constexpr bool operator==(const rebind<U>&) const noexcept {
    return true;
  }
};

static_assert(meta::allocator_of<DefaultAlloc<int>, int>);

struct ArenaDelete {
  void operator()(ntf_Arena arena) noexcept { ::ntf_arena_destroy(arena); }
};

template<typename T>
class ArenaAlloc;

class Arena {
public:
  template<typename T>
  using bind_alloc = ArenaAlloc<T>;

public:
  constexpr Arena(ntf_Arena arena) noexcept : _arena(arena) {}

  constexpr Arena(Arena&& other) noexcept : _arena(other._arena) { other._arena = nullptr; }

  ~Arena() noexcept { ::ntf_arena_destroy(_arena); }

  Arena& operator=(Arena&& other) noexcept {
    ::ntf_arena_destroy(_arena);

    _arena = other._arena;
    other._arena = nullptr;

    return *this;
  }

  NTF_NO_COPY(Arena);

public:
  void* allocate(size_t size, size_t align) const {
    void* ptr = ::ntf_arena_alloc(_arena, size, align);
    NTF_THROW_IF(!ptr, BadAlloc());
    return ptr;
  }

  void deallocate(void* ptr, size_t size) const noexcept {
    NTF_UNUSED(ptr);
    NTF_UNUSED(size);
  }

public:
  constexpr ntf_Arena arena() const noexcept { return _arena; }

  constexpr operator ntf_Arena() const noexcept { return _arena; }

private:
  ntf_Arena _arena;
};

static_assert(meta::mem_resource<Arena>);

template<typename T>
class ArenaAlloc {
public:
  using value_type = T;
  using pointer = T*;
  using size_type = size_t;
  using difference_type = ptrdiff_t;

public:
  template<typename U>
  using rebind = ArenaAlloc<U>;

public:
  constexpr ArenaAlloc(const Arena& arena) noexcept : _arena(arena.arena()) {}

  constexpr ArenaAlloc(ntf_Arena arena) noexcept : _arena(arena) {}

  constexpr ArenaAlloc(const ArenaAlloc&) noexcept = default;

  template<typename U>
  requires(!meta::is_same_v<U, T>)
  constexpr ArenaAlloc(const rebind<U>& other) noexcept : _arena(other.arena()) {}

public:
  T* allocate(size_t n) {
    T* ptr = static_cast<T*>(::ntf_arena_alloc(_arena, n * sizeof(T), alignof(T)));
    NTF_THROW_IF(!ptr, BadAlloc());
    return ptr;
  }

  void deallocate(T* ptr, size_t n) noexcept {
    NTF_UNUSED(ptr);
    NTF_UNUSED(n);
  }

public:
  constexpr ntf_Arena arena() const { return _arena; }

public:
  constexpr bool operator==(const ArenaAlloc& other) const noexcept {
    return _arena == other._arena;
  }

  template<typename U>
  requires(!meta::is_same_v<U, T>)
  constexpr bool operator==(const rebind<U>& other) const noexcept {
    return _arena == &other.arena();
  }

private:
  ntf_Arena _arena;
};

static_assert(meta::allocator_of<ArenaAlloc<int>, int>);

} // namespace ntf

#endif // NTF_MEMORY_HPP_
