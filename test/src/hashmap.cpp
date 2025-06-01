#include <catch2/catch_test_macros.hpp>
#include <ntfstl/hashmap.hpp>

namespace {

struct some_mempool {
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

template<typename T>
using alloc_t = ntf::allocator_adaptor<T, some_mempool>;

template<typename T>
using del_t = ntf::allocator_delete<T, alloc_t<T>>;

template<typename K, typename T>
using alloc_map =
  ntf::fixed_hashmap<K, T, std::hash<K>, std::equal_to<K>, del_t<std::pair<const K, T>>>;

} // namespace

TEST_CASE("fixed_hashmap construction", "[hashmap]") {
  const size_t count = 10u;
  SECTION("From size, using std::allocator") {
    ntf::fixed_hashmap<int, int> map(count);
    REQUIRE(map.capacity() == count);
    REQUIRE(map.size() == 0u);
  }
  SECTION("From size, using custom allocator"){
    some_mempool pool;
    {
      alloc_t<std::pair<const int, int>> alloc{pool};
      alloc_map<int, int> map(count, std::move(alloc));
      REQUIRE(map.capacity() == count);
      REQUIRE(map.size() == 0u);
    }
    REQUIRE(pool.allocated == 0u);
  }
  SECTION("From initializer list, using std::allocator") {
    ntf::fixed_hashmap<int, int> map{{1, 1}, {2, 2}};
    REQUIRE(map.capacity() == 2u);
    REQUIRE(map.size() == 2u);
    for (const auto& [k, v] : map){
      REQUIRE(k == v);
    }
  }
  SECTION("From initializer list, using custom allocator"){
    some_mempool pool;
    {
      alloc_t<std::pair<const int, int>> alloc{pool};
      alloc_map<int, int> map{{{1, 1}, {2, 2}}, std::move(alloc)};
      REQUIRE(map.capacity() == 2u);
      REQUIRE(map.size() == 2u);
      for (const auto& [k, v] : map){
        REQUIRE(k == v);
      }
    }
    REQUIRE(pool.allocated == 0u);
  }
  SECTION("From factory and size, using std::allocator") {
    auto map = ntf::fixed_hashmap<int, int>::from_size(count);
    REQUIRE(map.has_value());
    REQUIRE(map->capacity() == count);
    REQUIRE(map->size() == 0u);
  }
  SECTION("From factory and size, using custom allocator") {
    some_mempool pool;
    {
      alloc_t<std::pair<const int, int>> alloc{pool};
      auto map = ntf::fixed_hashmap<int, int>::from_size(count, std::move(alloc));
      REQUIRE(map.has_value());
      REQUIRE(map->capacity() == count);
      REQUIRE(map->size() == 0u);
    }
    REQUIRE(pool.allocated == 0u);
  }
  SECTION("From factory and initializer list, using std::allocator") {
    auto map = ntf::fixed_hashmap<int, int>::from_initializer({{1, 1}, {2, 2}});
    REQUIRE(map.has_value());
    REQUIRE(map->capacity() == 2u);
    REQUIRE(map->size() == 2u);
    for (const auto& [k, v] : *map){
      REQUIRE(k == v);
    }
  }
  SECTION("From factory and initializer list, using custom allocator") {
    some_mempool pool;
    {
      alloc_t<std::pair<const int, int>> alloc{pool};
      auto map = ntf::fixed_hashmap<int, int>::from_initializer({{1, 1},{2, 2}}, std::move(alloc));
      REQUIRE(map.has_value());
      REQUIRE(map->capacity() == 2u);
      REQUIRE(map->size() == 2u);
      for (const auto& [k, v] : *map){
        REQUIRE(k == v);
      }
    }
    REQUIRE(pool.allocated == 0u);
  }
}
