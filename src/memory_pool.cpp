#include <ntfstl/memory_pool.hpp>

#include <sys/mman.h>
#include <unistd.h>

namespace ntf {

namespace {

static const size_t page_size = static_cast<size_t>(sysconf(_SC_PAGE_SIZE));
// static constexpr int mem_pflag = PROT_READ | PROT_WRITE;
// static constexpr int mem_type = MAP_PRIVATE | MAP_ANONYMOUS;

size_t next_page_mult(size_t sz) noexcept {
  return page_size*std::ceil(static_cast<float>(sz)/static_cast<float>(page_size));
}

struct arena_header {
  arena_header* next;
  arena_header* prev;
  size_t size;
};
static constexpr size_t MIN_BLOCK_SIZE = kibs(4);
static constexpr size_t BLOCK_ALIGN = alignof(arena_header);

} // namespace

void* malloc_pool::allocate(size_t size, size_t align) const noexcept {
  return std::aligned_alloc(align, size);
}

void malloc_pool::deallocate(void* mem, size_t size) const noexcept {
  NTF_UNUSED(size);
  std::free(mem);
}

bool malloc_pool::is_equal(const malloc_pool&) const noexcept { return true; }

void* malloc_pool::malloc_fn(void* user_ptr, size_t size, size_t align) noexcept {
  NTF_UNUSED(user_ptr);
  return std::aligned_alloc(align, size);
}

void malloc_pool::free_fn(void* user_ptr, void* mem, size_t size) noexcept {
  NTF_UNUSED(user_ptr);
  NTF_UNUSED(size);
  std::free(mem);
}

fixed_arena::fixed_arena(void* user_ptr, free_fn_t free_fn,
                         void* block, size_t block_sz) noexcept :
  _user_ptr{user_ptr}, _free{free_fn},
  _block{block}, _used{0u}, _allocated{block_sz} {}

fixed_arena::fixed_arena(fixed_arena&& other) noexcept :
  _user_ptr{std::move(other._user_ptr)}, _free{std::move(other._free)},
  _block{std::move(other._block)}, _used{std::move(other._used)},
  _allocated{std::move(other._allocated)} { other._block = nullptr; }

fixed_arena::~fixed_arena() noexcept { _free_block(); }

fixed_arena& fixed_arena::operator=(fixed_arena&& other) noexcept { 
  _free_block();

  _user_ptr = std::move(other._user_ptr);
  _free = std::move(other._free);
  _block = std::move(other._block);
  _used = std::move(other._used);
  _allocated = std::move(other._allocated);

  other._block = nullptr;

  return *this;
}

expected<fixed_arena, std::bad_alloc> fixed_arena::from_size(size_t size) noexcept {
  const size_t block_sz = std::max(next_page_mult(size), MIN_BLOCK_SIZE);
  void* block = malloc_pool::malloc_fn(nullptr, block_sz, BLOCK_ALIGN);
  if (!block) {
    return unexpected{std::bad_alloc{}};
  }

  return expected<fixed_arena, std::bad_alloc>{
    in_place,
    nullptr, malloc_pool::free_fn, block, block_sz
  };
}

expected<fixed_arena, std::bad_alloc> fixed_arena::from_extern(malloc_funcs func,
                                                               size_t size) noexcept {
  if (!func.mem_alloc || !func.mem_free) {
    return unexpected{std::bad_alloc{}};
  }
  const size_t block_sz = std::max(next_page_mult(size), MIN_BLOCK_SIZE);
  void* block;
  try {
    block = std::invoke(func.mem_alloc, func.user_ptr, block_sz, BLOCK_ALIGN);
    if (!block) {
      return unexpected{std::bad_alloc{}};
    }
  } catch(...) {
    return unexpected{std::bad_alloc{}};
  }

  return expected<fixed_arena, std::bad_alloc>{
    in_place, func.user_ptr, func.mem_free, block, block_sz
  };
}

void* fixed_arena::allocate(size_t size, size_t align) {
  const size_t available = _allocated-_used;
  const size_t padding = align_fw_adjust(ptr_add(_block, _used), align);
  const size_t required = padding+size;

  if (available < required) {
    return nullptr;
  }

  void* ptr = ptr_add(_block, _used+padding);
  _used += required;
  return ptr;
}

void fixed_arena::deallocate(void* mem, size_t size) noexcept {
  NTF_UNUSED(mem);
  NTF_UNUSED(size);
}

bool fixed_arena::is_equal(const fixed_arena& other) const noexcept {
  return (this == &other);
}

void fixed_arena::clear() noexcept {
  _used = 0u;
}

void fixed_arena::_free_block() noexcept {
 if (_block) {
    std::invoke(_free, _user_ptr, _block, _allocated);
  }
}

linked_arena::linked_arena(void* user_ptr, malloc_fn_t malloc_fn, free_fn_t free_fn,
                           void* block, size_t block_sz) noexcept :
  _user_ptr{user_ptr}, _malloc{malloc_fn}, _free{free_fn},
  _block{block}, _block_used{0u},
  _total_used{0u}, _allocated{block_sz} {}

linked_arena::~linked_arena() noexcept { _free_blocks(); }

linked_arena::linked_arena(linked_arena&& other) noexcept :
  _user_ptr{std::move(other._user_ptr)},
  _malloc{std::move(other._malloc)},
  _free{std::move(other._free)},
  _block{std::move(other._block)},
  _block_used{std::move(other._block_used)},
  _total_used{std::move(other._total_used)},
  _allocated{std::move(other._allocated)} { other._block = nullptr; }

linked_arena& linked_arena::operator=(linked_arena&& other) noexcept {
  _free_blocks();

  _user_ptr = std::move(other._user_ptr);
  _malloc = std::move(other._malloc);
  _free = std::move(other._free);
  _block = std::move(other._block);
  _block_used = std::move(other._block_used);
  _total_used = std::move(other._total_used);
  _allocated = std::move(other._allocated);

  other._block = nullptr;

  return *this;
}

expected<linked_arena, std::bad_alloc> linked_arena::from_size(size_t size) noexcept {
  const size_t block_sz = std::max(next_page_mult(size+sizeof(arena_header)), MIN_BLOCK_SIZE);
  void* mem = malloc_pool::malloc_fn(nullptr, block_sz, BLOCK_ALIGN);
  if (!mem) {
    return unexpected{std::bad_alloc{}};
  }

  arena_header* block = static_cast<arena_header*>(mem);
  block->next = nullptr;
  block->prev = nullptr;
  block->size = block_sz;

  return expected<linked_arena, std::bad_alloc>{
    in_place,
    nullptr, malloc_pool::malloc_fn, malloc_pool::free_fn, static_cast<void*>(block), block_sz
  };
}
expected<linked_arena, std::bad_alloc> linked_arena::from_extern(malloc_funcs funcs,
                                                                 size_t size) noexcept {
  if (!funcs.mem_alloc || !funcs.mem_free) {
    return unexpected{std::bad_alloc{}};
  }
  const size_t block_sz = std::max(next_page_mult(size+sizeof(arena_header)), MIN_BLOCK_SIZE);
  void* mem;
  try {
    mem = std::invoke(funcs.mem_alloc, funcs.user_ptr, block_sz, BLOCK_ALIGN);
    if (!mem) {
      return unexpected{std::bad_alloc{}};
    }
  } catch(...) {
    return unexpected{std::bad_alloc{}};
  }

  arena_header* block = static_cast<arena_header*>(mem);
  block->next = nullptr;
  block->prev = nullptr;
  block->size = block_sz;

  return expected<linked_arena, std::bad_alloc>{
    in_place, funcs.user_ptr, funcs.mem_alloc, funcs.mem_free, static_cast<void*>(block), block_sz
  };
}

bool linked_arena::_try_acquire_block(size_t size, size_t align) noexcept {
  arena_header* next_block = static_cast<arena_header*>(_block)->next;
  while (next_block) {
    void* data_init = ptr_add(next_block, sizeof(arena_header));
    const size_t padding = align_fw_adjust(ptr_add(data_init, _block_used), align);
    if (next_block->size >= size+padding) {
      _block_used = 0u;
      _block = static_cast<void*>(next_block);
      return true;
    }
    next_block = next_block->next;
  }

  const size_t block_sz = std::max(next_page_mult(size+sizeof(arena_header)), MIN_BLOCK_SIZE);
  void* mem;
  try {
    mem = std::invoke(_malloc, _user_ptr, block_sz, BLOCK_ALIGN);
    if (!mem) {
      return false;
    }
  } catch(...) {
    return false;
  }
  arena_header* block = static_cast<arena_header*>(mem);
  block->next = nullptr;
  block->prev = static_cast<arena_header*>(_block);
  block->size = block_sz;
  block->prev->next = block;
  _block = static_cast<void*>(block); // Will waste the last few free bytes in the block
  _block_used = 0u;
  _allocated += block_sz;
  return true;
}

void* linked_arena::allocate(size_t size, size_t alignment) noexcept {
  arena_header* block = static_cast<arena_header*>(_block);
  void* data_init = ptr_add(block, sizeof(arena_header));

  const size_t available = block->size-_block_used;
  size_t padding = align_fw_adjust(ptr_add(data_init, _block_used), alignment);
  size_t required = size+padding;

  if (available < required) {
    if (!_try_acquire_block(size, alignment)) {
      return nullptr;
    }

    data_init = ptr_add(_block, sizeof(arena_header));
    padding = align_fw_adjust(data_init, alignment);
    required = size+padding;
  }

  void* ptr = ptr_add(data_init, _block_used+padding);
  _total_used += required;
  _block_used += required;
  return ptr;
}

void linked_arena::deallocate(void* mem, size_t size) noexcept {
  NTF_UNUSED(mem);
  NTF_UNUSED(size);
}

bool linked_arena::is_equal(const linked_arena& other) const noexcept {
  return (this == &other);
}

void linked_arena::clear() noexcept {
  arena_header* block = static_cast<arena_header*>(_block);
  while (block->prev) {
    block = block->prev;
  }
  _block = block;
  _total_used = 0u;
  _block_used = 0u;
}

void linked_arena::_free_blocks() noexcept {
  if (!_block) {
    return;
  }
  arena_header* block = static_cast<arena_header*>(_block);
  while (block->next) {
    block = block->next;
  }
  while (block) {
    arena_header* prev = block->prev;
    const size_t size = block->size;
    std::invoke(_free, _user_ptr, static_cast<void*>(block), size);
    block = prev;
  }
}

} // namespace ntf
