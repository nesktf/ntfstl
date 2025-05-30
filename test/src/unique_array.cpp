#include <catch2/catch_test_macros.hpp>
#include <ntfstl/unique_array.hpp>

namespace {

struct int_arena {
  size_t pos{0};
  int mem[20];
};

struct int_alloc {
public:
  using value_type = int;

public:
  int_alloc(int_arena& arena) :
    _arena{&arena} {}

public:
  int* allocate(size_t count) {
    int* val = _arena->mem+_arena->pos;
    _arena->pos += count;
    return val;
  }

  void deallocate(int*, size_t) {}

private:
  int_arena* _arena;
};

using int_alloc_del = ntf::allocator_delete<int, int_alloc>;

} // namespace

TEST_CASE("unique array empty construction", "[unique_array]") {
  int_arena arena;
  int_alloc alloc{arena};

  ntf::unique_array<int> arr;
  REQUIRE(arr.empty());

  ntf::unique_array<int> arr_null = nullptr;
  REQUIRE(arr_null.empty());

  ntf::unique_array<int, int_alloc_del> arr_alloc{alloc};
  REQUIRE(arr_alloc.empty());
}

TEST_CASE("unique array construction and allocation", "[unique_array]") {
  const size_t count = 4u;
  const int copy = 10;

  SECTION("Use std::allocator, construct from pointer") {
    auto* ptr = std::allocator<int>{}.allocate(count);
    for (size_t i = 0; i < count; ++i) {
      new (ptr+i) int{copy};
    }

    ntf::unique_array<int> arr{count, ptr};
    REQUIRE(!arr.empty());
    REQUIRE(arr.size() == count);

    for (const int i : arr){
      REQUIRE(i == copy);
    }

    arr.reset();
    REQUIRE(arr.empty());
  }

  SECTION("Use std::allocator, construct from factory using copy") {
    auto arr = ntf::unique_array<int>::from_allocator(count, copy);
    REQUIRE(!arr.empty());
    REQUIRE(arr.size() == count);

    for (const int n : arr) {
      REQUIRE(n == copy);
    }

    arr.reset();
    REQUIRE(arr.empty());
  }

  SECTION("Use std::allocator, construct from factory, uninitialized"){
    auto arr = ntf::unique_array<int>::from_allocator(ntf::uninitialized, count);
    REQUIRE(!arr.empty());
    REQUIRE(arr.size() == count);

    arr.reset();
    REQUIRE(arr.empty());
  }

  SECTION("Use std::alllocator, use constructor and copy") {
    ntf::unique_array<int> arr{count, copy};
    REQUIRE(!arr.empty());
    REQUIRE(arr.size() == count);

    for (const int n : arr) {
      REQUIRE(n == copy);
    }

    arr.reset();
    REQUIRE(arr.empty());
  }

  SECTION("Use std::allocator, use constructor, uninitialized") {
    ntf::unique_array<int> arr{ntf::uninitialized, count};
    REQUIRE(!arr.empty());
    REQUIRE(arr.size() == count);

    arr.reset();
    REQUIRE(arr.empty());
  }

  SECTION("Use operator new[] and copy") {
    auto* ptr = new int[8u];
    for (size_t i = 0; i < count; ++i){
      ptr[i] = copy;
    }

    ntf::unique_array<int, std::default_delete<int[]>> arr{count, ptr};
    REQUIRE(!arr.empty());
    REQUIRE(arr.size() == count);

    for (const int i : arr){
      REQUIRE(i == copy);
    }

    arr.reset();
    REQUIRE(arr.empty());
  }

  SECTION("Use custom allocator, construct from pointer") {
    int_arena arena;
    int_alloc alloc{arena};

    auto* ptr = alloc.allocate(count);
    for (size_t i = 0; i < count; ++i) {
      new (ptr+i) int{copy};
    }

    ntf::unique_array<int, int_alloc_del> arr{count, ptr, alloc};
    REQUIRE(!arr.empty());
    REQUIRE(arr.size() == count);

    for (const int i : arr){
      REQUIRE(i == copy);
    }

    arr.reset();
    REQUIRE(arr.empty());
  }

  SECTION("Use custom allocator, construct from factory using copy"){
    int_arena arena;
    int_alloc alloc{arena};

    auto arr = ntf::unique_array<int>::from_allocator(count, copy, alloc);
    REQUIRE(!arr.empty());
    REQUIRE(arr.size() == count);

    for (const int n : arr) {
      REQUIRE(n == copy);
    }

    arr.reset();
    REQUIRE(arr.empty());
  }

  SECTION("Use custom allocator, construct from factory, uninitialized") {
    int_arena arena;
    int_alloc alloc{arena};

    auto arr = ntf::unique_array<int>::from_allocator(ntf::uninitialized, count, alloc);
    REQUIRE(!arr.empty());
    REQUIRE(arr.size() == count);

    arr.reset();
    REQUIRE(arr.empty());
  }

  SECTION("Use custom allocator, use constructor and copy") {
    int_arena arena;
    int_alloc alloc{arena};

    ntf::unique_array<int, int_alloc_del> arr{count, copy, alloc};
    REQUIRE(!arr.empty());
    REQUIRE(arr.size() == count);

    for (const int n : arr) {
      REQUIRE(n == copy);
    }

    arr.reset();
    REQUIRE(arr.empty());
  }

  SECTION("Use custom allocator, use constructor, uninitialized") {
    int_arena arena;
    int_alloc alloc{arena};

    ntf::unique_array<int, int_alloc_del> arr{ntf::uninitialized, count, alloc};
    REQUIRE(!arr.empty());
    REQUIRE(arr.size() == count);

    arr.reset();
    REQUIRE(arr.empty());
  }
}

TEST_CASE("unique array move operations" "[unique_array]") {
  const size_t count = 8u;
  SECTION("Use std::allocator") {
    auto arr = ntf::unique_array<int>::from_allocator(ntf::uninitialized, count);
    REQUIRE(!arr.empty());
    REQUIRE(arr.size() == count);

    auto arr2 = std::move(arr);
    REQUIRE(arr.empty());
    REQUIRE(!arr2.empty());
    REQUIRE(arr2.size() == count);

    arr = std::move(arr2);
    REQUIRE(arr2.empty());
    REQUIRE(!arr.empty());
    REQUIRE(arr.size() == count);
  }

  SECTION("Use custmo allocator"){
    int_arena arena;
    int_alloc alloc{arena};

    auto arr = ntf::unique_array<int>::from_allocator(ntf::uninitialized, count, alloc);
    REQUIRE(!arr.empty());
    REQUIRE(arr.size() == count);

    auto arr2 = std::move(arr);
    REQUIRE(arr.empty());
    REQUIRE(!arr2.empty());
    REQUIRE(arr2.size() == count);

    arr = std::move(arr2);
    REQUIRE(arr2.empty());
    REQUIRE(!arr.empty());
    REQUIRE(arr.size() == count);
  }
}

TEST_CASE("unique array reset", "[unique_array]") {
  const size_t count1 = 8u;
  const size_t count2 = 4u;;
  {
    auto arr1 = ntf::unique_array<int>::from_allocator(ntf::uninitialized, count1);
    REQUIRE(!arr1.empty());
    REQUIRE(arr1.size() == count1);

    auto arr2 = ntf::unique_array<int>::from_allocator(ntf::uninitialized, count2);
    REQUIRE(!arr2.empty());
    REQUIRE(arr2.size() == count2);

    auto [ptr, sz] = arr2.release();
    REQUIRE(arr2.empty());
    REQUIRE(sz == count2);

    arr1.reset(sz, ptr);
    REQUIRE(arr1.size() == count2);
  }
}
