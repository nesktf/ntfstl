#include <ntf/memory.hpp>

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <sys/mman.h>
#include <unistd.h>

size_t ntf_system_page_size() noexcept {
  static const size_t page_size = (size_t)sysconf(_SC_PAGE_SIZE);
  return page_size;
}

namespace {

const size_t MinArenaSize = 4 * 1024 * ntf_system_page_size(); // 16MiB when page_size == 4KiB

size_t next_page_size(size_t sz) noexcept {
  // Allocate one extra memory page just in case
  const size_t page_size = ntf_system_page_size();
  return page_size * (size_t)ceilf((float)sz / (float)page_size);
}

size_t align_fw_adjust(void* ptr, size_t align) noexcept {
  uintptr_t p;
  memcpy(&p, &ptr, sizeof(p));
  return align - (p & (align - 1));
}

void* ptr_add(void* ptr, size_t sz) noexcept {
  uintptr_t p;
  memcpy(&p, &ptr, sizeof(p));
  p += sz;
  memcpy(&ptr, &p, sizeof(p));
  return ptr;
}

} // namespace

struct ntf_Arena_T {
  size_t mapping_size;
  size_t used;

  void* head(size_t pad = 0) { return ptr_add(this + sizeof(*this), used + pad); }
};

int ntf_arena_init(ntf_Arena* arena, size_t capacity) noexcept {
  if (!arena) {
    return 2;
  }
  const size_t mapping_size = ntf::max(next_page_size(capacity), MinArenaSize);
  void* ptr = mmap(nullptr, mapping_size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, 0, 0);
  if (!ptr) {
    return 1;
  }

  *arena = NTF_PNEW(ptr) ntf_Arena_T(mapping_size, 0);
  return 0;
}

void ntf_arena_destroy(ntf_Arena arena) noexcept {
  if (!arena) {
    return;
  }
  const size_t size = arena->mapping_size;
  int ret = munmap(arena, size);
  NTF_UNUSED(ret);
}

void ntf_arena_clear(ntf_Arena arena) noexcept {
  if (!arena) {
    return;
  }
  arena->used = 0;
}

void* ntf_arena_alloc(ntf_Arena arena, size_t size, size_t align) noexcept {
  if (!arena) {
    return nullptr;
  }
  const auto avail = arena->mapping_size - arena->used;
  const auto pad = align_fw_adjust(arena->head(), align);
  const auto required = size + pad;
  if (avail < required) {
    return nullptr;
  }
  void* ptr = arena->head(pad);
  arena->used += required;
  return ptr;
}

// Put this here just because i don't want to make an extra file
NTF_NORETURN void ntf__panic_handler(const char* file, const char* func, int line,
                                     const char* msg) {
  fprintf(stderr, "PANIC %s@%s:%d: %s", func, file, line, msg);
  NTF_ABORT();
  NTF_UNREACHABLE();
}
