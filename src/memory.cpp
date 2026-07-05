#include <ntf/memory.hpp>

#include <sys/mman.h>

using namespace ntf::numdefs;

namespace {

struct ArenaHeader {};

} // namespace

size_t ntf_system_page_size() noexcept {}

int ntf_arena_init(ntf_Arena* arena, size_t capacity) noexcept {}

void ntf_arena_destroy(ntf_Arena* arena) noexcept {}

void ntf_arena_clear(ntf_Arena* arena) noexcept {}

void* ntf_arena_alloc(ntf_Arena* arena, size_t size, size_t align) noexcept {}

void* ntf_arena_realloc(ntf_Arena* arena, void* ptr, size_t newsz, size_t oldsz,
                        size_t align) noexcept {}
