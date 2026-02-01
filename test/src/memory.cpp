#include <catch2/catch_test_macros.hpp>
#include <ntfstl/memory.hpp>

using namespace ntf::numdefs;

namespace {

struct some_mempool : public ::ntf::mem::memory_pool {
  using size_type = size_t;
  using difference_type = ptrdiff_t;

  size_t allocated = 0;
  size_t bulk_allocated = 0;

  void* allocate(size_t size, size_t align) override {
    allocated += size;
    return std::aligned_alloc(align, size);
  }

  void deallocate(void* ptr, size_t size) noexcept override {
    std::free(ptr);
    allocated -= size;
  }

  std::pair<void*, size_t> bulk_allocate(size_t size, size_t align) override {
    void* ptr = std::aligned_alloc(align, size);
    bulk_allocated += size;
    return {ptr, size};
  }

  void bulk_deallocate(void* ptr, size_t size) noexcept override {
    std::free(ptr);
    bulk_allocated -= size;
  }
};

static_assert(ntf::meta::memory_pool_type<some_mempool>);

} // namespace

TEST_CASE("fixed_arena common usage", "[mem::fixed_arena]") {
  SECTION("Using default memory pool") {
    auto arena = ntf::mem::fixed_arena::with_capacity(ntf::mem::mibs(4u));
    REQUIRE(arena.has_value());
    REQUIRE(arena->allocated() >= ntf::mem::mibs(4u));
    REQUIRE(arena->used() == 0);

    void* data;
    REQUIRE_NOTHROW(data = arena->allocate(20 * sizeof(u32), alignof(u32)));
    REQUIRE(data != nullptr);
    REQUIRE(arena->used() > 0);

    arena->clear();
    REQUIRE(arena->used() == 0);

    REQUIRE_THROWS(arena->allocate(ntf::mem::mibs(16u), alignof(u32))); // More than arena max
  }
  SECTION("Using custom memory pool") {
    some_mempool pool;
    {
      auto arena = ntf::mem::fixed_arena::using_pool(pool, ntf::mem::mibs(4u));
      REQUIRE(arena.has_value());
      REQUIRE(arena->allocated() >= ntf::mem::mibs(4u));
      REQUIRE(arena->used() == 0);
      REQUIRE(pool.bulk_allocated >= ntf::mem::mibs(4u));

      void* data;
      REQUIRE_NOTHROW(data = arena->allocate(20 * sizeof(u32), alignof(u32)));
      REQUIRE(data != nullptr);
      REQUIRE(arena->used() > 0);

      arena->clear();
      REQUIRE(arena->used() == 0);

      REQUIRE_THROWS(arena->allocate(ntf::mem::mibs(16u), alignof(u32))); // More than arena max
    }
    REQUIRE(pool.bulk_allocated == 0u);
  }
}

TEST_CASE("growing_arena common usage", "[mem::growing_arena]") {
  SECTION("Using default memory pool") {
    auto arena = ntf::mem::growing_arena::with_initial_size(ntf::mem::mibs(4u));
    REQUIRE(arena.has_value());
    REQUIRE(arena->allocated() >= ntf::mem::mibs(4u));
    REQUIRE(arena->used() == 0);

    void* data;
    REQUIRE_NOTHROW(data = arena->allocate(20 * sizeof(u32), alignof(u32)));
    REQUIRE(data != nullptr);
    REQUIRE(arena->used() > 0);

    arena->clear();
    REQUIRE(arena->used() == 0);

    // Force arena to grow
    REQUIRE_NOTHROW(data = arena->allocate(ntf::mem::mibs(16u), alignof(u8)));
    REQUIRE(data != nullptr);

    REQUIRE(arena->used() != 0);
    REQUIRE(arena->allocated() >= ntf::mem::mibs(16u));

    arena->clear();
    REQUIRE(arena->used() == 0);
  }

  SECTION("Using custom memory pool") {
    some_mempool pool;
    {
      auto arena = ntf::mem::growing_arena::using_pool(pool, ntf::mem::mibs(4u));
      REQUIRE(arena.has_value());
      REQUIRE(arena->allocated() >= ntf::mem::mibs(4u));
      REQUIRE(arena->used() == 0u);
      REQUIRE(pool.bulk_allocated >= ntf::mem::mibs(4u));

      void* data;
      REQUIRE_NOTHROW(data = arena->allocate(20 * sizeof(u32), alignof(u32)));
      REQUIRE(data != nullptr);
      REQUIRE(arena->used() > 0);

      arena->clear();
      REQUIRE(arena->used() == 0);

      // Force arena to grow
      REQUIRE_NOTHROW(data = arena->allocate(ntf::mem::mibs(16u), alignof(u8)));
      REQUIRE(data != nullptr);

      REQUIRE(arena->used() != 0);
      REQUIRE(arena->allocated() >= ntf::mem::mibs(16u));

      arena->clear();
      REQUIRE(arena->used() == 0);
    }
    REQUIRE(pool.allocated == 0);
  }
}
