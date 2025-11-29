#include <catch2/catch_test_macros.hpp>
#include <ntfstl/sparse_set.hpp>

using namespace ntf::numdefs;

namespace {

struct my_funny_elem {
  my_funny_elem(std::string_view name_, f32 x_, f32 y_) noexcept : name{name_}, x{x_}, y{y_} {}

  f32 len() const { return std::sqrt((x * x) + (y * y)); }

  std::string_view name;
  f32 x, y;
};

} // namespace

TEST_CASE("sparse_set construct", "[sparse_set]") {
  ntf::sparse_set<my_funny_elem> elems;
  REQUIRE(elems.empty());
  REQUIRE(elems.size() == 0);
  REQUIRE(elems.capacity() == 0);
  REQUIRE(elems.page_capacity() == 0);
  REQUIRE(elems.pages() == 0);
}

TEST_CASE("sparse_set push", "[sparse_set]") {
  ntf::sparse_set<my_funny_elem> elems;
  auto& elem = elems.emplace(0, "amoma", 1.f, 1.f);
  REQUIRE(!elems.empty());
  REQUIRE(elems.size() == 1);
  REQUIRE(elems.capacity() != 0);
  REQUIRE(elems.pages() != 0);
  REQUIRE(elems.has_element(0));

  REQUIRE(elem.name == "amoma");
  REQUIRE(elem.x == 1.f);
  REQUIRE(elem.y == 1.f);
  {
    auto& ref = elems.at(0);
    REQUIRE(&elem == &ref);

    auto* ptr = elems.at_ptr(0);
    REQUIRE(ptr != nullptr);
    REQUIRE(&elem == ptr);
  }

  elems.clear();
  REQUIRE(elems.empty());
  REQUIRE(elems.size() == 0);
  REQUIRE(elems.capacity() != 0);
  REQUIRE(elems.pages() != 0);

  elems.reset();
  REQUIRE(elems.empty());
  REQUIRE(elems.size() == 0);
  REQUIRE(elems.capacity() == 0);
  REQUIRE(elems.page_capacity() == 0);
  REQUIRE(elems.pages() == 0);
}
