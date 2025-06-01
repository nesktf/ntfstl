#include <catch2/catch_test_macros.hpp>
#include <ntfstl/allocator.hpp>

using namespace ntf::numdefs;

namespace {

struct some_allocator {
  size_t allocated = 0u;

  void* allocate(size_t size, size_t align) {
    allocated += size;
    return std::aligned_alloc(size, align);
  }

  void deallocate(void* mem, size_t size){
    allocated -= size;
    std::free(mem);
  }
};

} // namespace

TEST_CASE("fixed arena common usage", "[memory_pool]") {
  SECTION("Using default allocator") {
    auto arena = ntf::fixed_arena::from_size(ntf::mibs(4u));
    REQUIRE(arena.has_value());
    REQUIRE(arena->capacity() >= ntf::mibs(4u));
    REQUIRE(arena->size() == 0u);

    arena->allocate_uninited<uint32>(20);
    REQUIRE(arena->size() != 0u);
    arena->clear();
    REQUIRE(arena->size() == 0u);

    void* ptr = arena->allocate(ntf::mibs(8u), alignof(uint8)); // More than arena max
    REQUIRE(ptr == nullptr);
    REQUIRE(arena->size() == 0u);
  }
  SECTION("Using custom allocator") {
    some_allocator alloc;
    {
      auto arena = ntf::fixed_arena::from_extern(ntf::make_pool_funcs(alloc), ntf::mibs(4u));
      REQUIRE(arena.has_value());
      REQUIRE(arena->capacity() >= ntf::mibs(4u));
      REQUIRE(arena->size() == 0u);

      arena->allocate_uninited<uint32>(20);
      REQUIRE(alloc.allocated >= ntf::mibs(4u));
      REQUIRE(arena->size() != 0u);
      arena->clear();
      REQUIRE(arena->size() == 0u);

      void* ptr = arena->allocate(ntf::mibs(8u), alignof(uint8)); // More than arena max
      REQUIRE(ptr == nullptr);
      REQUIRE(arena->size() == 0u);
    }
    REQUIRE(alloc.allocated == 0u);
  }
}

TEST_CASE("linked arena common usage", "[memory_pool]") {
  SECTION("Using default allocator") {
    auto arena = ntf::linked_arena::from_size(ntf::mibs(4u));
    REQUIRE(arena.has_value());
    REQUIRE(arena->capacity() >= ntf::mibs(4u));
    REQUIRE(arena->size() == 0u);

    arena->allocate_uninited<uint32>(20);
    REQUIRE(arena->size() != 0u);
    arena->clear();
    REQUIRE(arena->size() == 0u);

    void* ptr = arena->allocate(ntf::mibs(8u), alignof(uint8)); // Force arena to grow
    REQUIRE(ptr != nullptr);
    REQUIRE(arena->size() != 0u);
    REQUIRE(arena->capacity() >= ntf::mibs(8u));
    arena->clear();
    REQUIRE(arena->size() == 0u);
  }

  SECTION("Using custom allocator") {
    some_allocator alloc;
    {
      auto arena = ntf::linked_arena::from_extern(ntf::make_pool_funcs(alloc), ntf::mibs(4u));
      REQUIRE(arena.has_value());
      REQUIRE(arena->capacity() >= ntf::mibs(4u));
      REQUIRE(alloc.allocated >= ntf::mibs(4u));
      REQUIRE(arena->size() == 0u);

      arena->allocate_uninited<uint32>(20);
      REQUIRE(arena->size() != 0u);
      arena->clear();
      REQUIRE(arena->size() == 0u);

      void* ptr = arena->allocate(ntf::mibs(8u), alignof(uint8)); // Force arena to grow
      REQUIRE(ptr != nullptr);
      REQUIRE(arena->size() != 0u);
      REQUIRE(arena->capacity() >= ntf::mibs(8u));
      arena->clear();
      REQUIRE(arena->size() == 0u);
    }
    REQUIRE(alloc.allocated == 0u);
  }
}
