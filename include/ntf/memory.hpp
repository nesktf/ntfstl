#ifndef NTF_MEMORY_HPP_
#define NTF_MEMORY_HPP_

#include <ntf/core.hpp>

enum NTF_PNEW_T {
  NTF_PNEW_TAG,
};

constexpr inline void* operator new(size_t, void* ptr, NTF_PNEW_T) {
  return ptr;
}

constexpr inline void operator delete(void*, void*, NTF_PNEW_T) {}

#define NTF_PNEW(_ptr) ::new (_ptr, NTF_PNEW_TAG)

namespace ntf {

constexpr inline u64 ArenaMinSize = 4 * 1024 * 1024; // 4MiB

NTF_DEFINE_HANDLE(Arena);

void* mem_alloc(size_t size) noexcept;
void* mem_realloc(void* ptr, size_t newsz, size_t oldsz) noexcept;
void mem_free(void* ptr) noexcept;

class bad_alloc : public exception {
public:
  virtual const char* what() const noexcept { return "bad_alloc"; }
};

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
  T* allocate(size_t n) {
    T* ptr = static_cast<T*>(::ntf::mem_alloc(n * sizeof(T)));
    NTF_THROW_IF(!ptr, bad_alloc());
    return ptr;
  }

  void deallocate(T* ptr, size_t n) noexcept {
    NTF_UNUSED(n);
    ::ntf::mem_free(ptr);
  }

public:
  constexpr bool operator==(const DefaultAlloc&) const noexcept { return true; }

  template<typename U>
  requires(!meta::is_same_v<U, T>)
  constexpr bool operator==(const rebind<U>&) const noexcept {
    return true;
  }
};

int arena_init(Arena* arena, size_t capacity) noexcept;
void arena_destroy(Arena* arena) noexcept;
void arena_clear(Arena* arena) noexcept;
void* arena_alloc(Arena* arena, size_t size, size_t align) noexcept;
void* arena_realloc(Arena* arena, void* ptr, size_t newsz, size_t oldsz, size_t align) noexcept;

struct ArenaDelete {
  void operator()(Arena* arena) noexcept { arena_destroy(arena); }
};

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
  constexpr ArenaAlloc(Arena* arena) noexcept : _arena(arena) {}

  constexpr ArenaAlloc(const ArenaAlloc&) noexcept = default;

  template<typename U>
  requires(!meta::is_same_v<U, T>)
  constexpr ArenaAlloc(const rebind<U>& other) noexcept : _arena(other.arena()) {}

public:
  T* allocate(size_t n) {
    T* ptr = static_cast<T*>(::ntf::arena_alloc(_arena, n * sizeof(T), alignof(T)));
    NTF_THROW_IF(!ptr, bad_alloc());
    return ptr;
  }

  void deallocate(T* ptr, size_t n) noexcept {
    NTF_UNUSED(ptr);
    NTF_UNUSED(n);
  }

public:
  constexpr Arena* arena() const { return _arena; }

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
  Arena* _arena;
};

} // namespace ntf

#endif // NTF_MEMORY_HPP_
