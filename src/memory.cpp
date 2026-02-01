#include <ntfstl/memory.hpp>

// TODO: Make this multiplatform
#define SHOGLE_OS_LOONIX

#ifdef SHOGLE_OS_LOONIX
#include <sys/mman.h>
#include <unistd.h>
#endif

#ifndef SHOGLE_OS_LOONIX
#error "memory.cpp not defined for non linux os"
#endif

#include <atomic>

namespace ntf::mem {

#ifdef SHOGLE_OS_LOONIX
size_t system_page_size() noexcept {
  static const size_t page_size = static_cast<size_t>(sysconf(_SC_PAGE_SIZE));
  return page_size;
}
#endif

namespace {

size_t next_page_size(size_t sz) noexcept {
  // Allocate one extra memory page just in case
  const size_t page_size = system_page_size();
  return page_size * static_cast<size_t>(
                       (1.f + std::ceil(static_cast<f32>(sz) / static_cast<f32>(page_size))));
}

std::atomic<size_t> default_pool_allocated = 0;
std::atomic<size_t> default_pool_bulk_allocated = 0;

} // namespace

default_pool& default_pool::instance() noexcept {
  static default_pool pool;
  return pool;
}

default_pool::size_type default_pool::total_allocated() noexcept {
  return default_pool_allocated.load();
}

default_pool::size_type default_pool::total_bulk_allocated() noexcept {
  return default_pool_bulk_allocated.load();
}

void* default_pool::allocate(size_type size, size_type alignment) {
  void* ptr = std::aligned_alloc(alignment, size);
  NTF_THROW_IF(!ptr, std::bad_alloc());
  default_pool_allocated += size;
  return ptr;
}

void default_pool::deallocate(void* ptr, size_type size) noexcept {
  std::free(ptr);
  default_pool_allocated -= size;
}

#ifdef SHOGLE_OS_LOONIX
auto default_pool::bulk_allocate(size_type size, size_type alignment)
  -> std::pair<void*, size_type> {
  NTF_UNUSED(alignment); // assume mmap is always 8 byte aligned
  const size_t mapping_sz = next_page_size(size);
  void* ptr = mmap(nullptr, mapping_sz, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, 0, 0);
  NTF_THROW_IF(!ptr, std::bad_alloc());
  default_pool_bulk_allocated += size;
  return {ptr, mapping_sz};
}

void default_pool::bulk_deallocate(void* ptr, size_type size) noexcept {
  int ret = munmap(ptr, size);
  NTF_UNUSED(ret);
  default_pool_bulk_allocated -= size;
}
#endif

namespace {

struct arena_header {
  memory_pool* pool;
  arena_header* next;
  arena_header* prev;
  size_t size;
  size_t used;
};

std::pair<void*, size_t> init_arena(memory_pool* pool, size_t initial_size) noexcept {
  size_t block_size = next_page_size(initial_size + sizeof(arena_header));
  void* ptr;
  try {
    if (pool) {
      std::tie(ptr, block_size) = pool->bulk_allocate(block_size, alignof(arena_header));
    } else {
      std::tie(ptr, block_size) =
        default_pool::instance().bulk_allocate(block_size, alignof(arena_header));
    }
  } catch (...) {
    return {nullptr, 0};
  }
  if (!ptr) {
    return {nullptr, 0};
  }
  arena_header* header = static_cast<arena_header*>(ptr);
  header->pool = pool;
  header->next = nullptr;
  header->prev = nullptr;
  header->size = block_size;
  header->used = 0u;
  return {ptr, block_size};
}

void free_arena(void* arena_block) noexcept {
  arena_header* header = static_cast<arena_header*>(arena_block);
  while (header->next) {
    header = header->next;
  }
  while (header) {
    arena_header* prev = header->prev;
    const size_t block_size = header->size;
    memory_pool* pool = header->pool;
    if (pool) {
      pool->bulk_deallocate(static_cast<void*>(header), block_size);
    } else {
      default_pool::instance().bulk_deallocate(static_cast<void*>(header), block_size);
    }
    header = prev;
  }
}

} // namespace

growing_arena::growing_arena(create_t, void* data, size_type block_size) noexcept :
    _data(data), _used(), _allocated(block_size) {}

growing_arena::growing_arena(size_type initial_size) : _used() {
  std::tie(_data, _allocated) = init_arena(nullptr, initial_size);
  NTF_THROW_IF(!_data, std::bad_alloc());
}

growing_arena::growing_arena(memory_pool& pool, size_type initial_size) : _used() {
  std::tie(_data, _allocated) = init_arena(&pool, initial_size);
  NTF_THROW_IF(!_data, std::bad_alloc());
}

ntf::expected<growing_arena, std::bad_alloc>
growing_arena::with_initial_size(size_type initial_size) noexcept {
  auto [data, block_size] = init_arena(nullptr, initial_size);
  if (!data) {
    return {ntf::unexpect};
  }
  return {ntf::in_place, create_t{}, data, block_size};
}

ntf::expected<growing_arena, std::bad_alloc>
growing_arena::using_pool(memory_pool& pool, size_type initial_size) noexcept {
  auto [data, block_size] = init_arena(&pool, initial_size);
  if (!data) {
    return {ntf::unexpect};
  }
  return {ntf::in_place, create_t{}, data, block_size};
}

void* growing_arena::allocate(size_type size, size_type alignment) {
  NTF_ASSERT(_data != nullptr, "growing_arena use after move");
  arena_header* header = static_cast<arena_header*>(_data);
  const auto acquire_new_block = [&]() {
    // First we try to fit our data in an already existing memory block
    arena_header* next = header->next;
    while (next) {
      void* data_init = ptr_add(static_cast<void*>(next), sizeof(arena_header));
      const size_t pad = align_fw_adjust(data_init, alignment);
      if (next->size >= size + pad) {
        header = next;
        _data = static_cast<void*>(next);
        return;
      }
      next = next->next;
    }

    // If we didn't find an appropiate block, we allocate a new one
    size_t block_size = next_page_size(size + sizeof(arena_header));
    void* mem;
    if (header->pool) {
      std::tie(mem, block_size) = header->pool->bulk_allocate(block_size, alignof(arena_header));
    } else {
      std::tie(mem, block_size) =
        default_pool::instance().bulk_allocate(block_size, alignof(arena_header));
    }
    NTF_THROW_IF(!mem, std::bad_alloc());

    // Initialize the new block header
    arena_header* new_header = static_cast<arena_header*>(mem);
    new_header->pool = header->pool;
    new_header->next = nullptr;
    new_header->prev = header;
    new_header->prev->next = new_header;
    _allocated += block_size;
    header = new_header;
    _data = mem;
  };
  void* data_init;
  size_t pad, required;
  const auto calc_block = [&]() {
    data_init = ptr_add(_data, sizeof(arena_header));
    pad = align_fw_adjust(ptr_add(data_init, header->used), alignment);
    required = size + pad;
  };

  const size_t available = header->size - header->used;
  calc_block();
  if (available < required) {
    acquire_new_block();
    calc_block();
  }
  void* ptr = ptr_add(data_init, header->used + pad);
  _used += required;
  header->used += required;
  return ptr;
}

void growing_arena::clear() noexcept {
  arena_header* header = static_cast<arena_header*>(_data);
  while (header->prev) {
    header->used = 0u;
    header = header->prev;
  }
  _data = static_cast<void*>(header);
  _used = 0u;
}

// TODO: this thing
bool growing_arena::is_equal(const growing_arena& other) const noexcept {
  return false;
}

growing_arena::~growing_arena() noexcept {
  if (_data) {
    free_arena(_data);
  }
}

growing_arena::growing_arena(growing_arena&& other) noexcept :
    _data(other._data), _used(other._used), _allocated(other._allocated) {
  other._data = nullptr;
}

growing_arena& growing_arena::operator=(growing_arena&& other) noexcept {
  if (this == &other) {
    return *this;
  }
  if (_data) {
    free_arena(_data);
  }

  _data = other._data;
  _used = other._used;
  _allocated = other._allocated;

  other._data = nullptr;

  return *this;
}

fixed_arena::fixed_arena(create_t, void* data, size_type block_size) noexcept :
    _data(data), _used(), _allocated(block_size) {}

fixed_arena::fixed_arena(size_type capacity) : _used() {
  std::tie(_data, _allocated) = init_arena(nullptr, capacity);
  NTF_THROW_IF(!_data, std::bad_alloc());
}

fixed_arena::fixed_arena(memory_pool& pool, size_type capacity) : _used() {
  std::tie(_data, _allocated) = init_arena(&pool, capacity);
  NTF_THROW_IF(!_data, std::bad_alloc());
}

ntf::expected<fixed_arena, std::bad_alloc>
fixed_arena::with_capacity(size_type capacity) noexcept {
  auto [ptr, block_size] = init_arena(nullptr, capacity);
  if (!ptr) {
    return {ntf::unexpect};
  }
  return {ntf::in_place, create_t{}, ptr, block_size};
}

ntf::expected<fixed_arena, std::bad_alloc> fixed_arena::using_pool(memory_pool& pool,
                                                                   size_type capacity) noexcept {
  auto [ptr, block_size] = init_arena(&pool, capacity);
  if (!ptr) {
    return {ntf::unexpect};
  }
  return {ntf::in_place, create_t{}, ptr, block_size};
}

void* fixed_arena::allocate(size_type size, size_type alignment) {
  NTF_ASSERT(_data != nullptr, "fixed_arena use after move");
  arena_header* header = static_cast<arena_header*>(_data);
  void* data_init = ptr_add(static_cast<void*>(header), sizeof(arena_header));
  const size_type available = _allocated - header->used;
  const size_type padding = align_fw_adjust(ptr_add(data_init, header->used), alignment);
  const size_type required = padding + size;
  NTF_THROW_IF(available < required, std::bad_alloc());
  data_init = ptr_add(data_init, _used + padding);
  _used += required;
  return data_init;
}

// TODO: this thing
bool fixed_arena::is_equal(const fixed_arena& other) const noexcept {
  return false;
}

void fixed_arena::clear() noexcept {
  _used = 0;
}

fixed_arena::~fixed_arena() noexcept {
  if (_data) {
    free_arena(_data);
  }
}

fixed_arena::fixed_arena(fixed_arena&& other) noexcept :
    _data(other._data), _used(other._used), _allocated(other._allocated) {
  other._data = nullptr;
}

fixed_arena& fixed_arena::operator=(fixed_arena&& other) noexcept {
  if (this == &other) {
    return *this;
  }
  if (_data) {
    free_arena(_data);
  }

  _data = other._data;
  _used = other._used;
  _allocated = other._allocated;

  other._data = nullptr;

  return *this;
}

} // namespace ntf::mem
