#include <catch2/catch_test_macros.hpp>
#include <ntfstl/sparse_set.hpp>

using namespace ntf::numdefs;

// namespace {
//
// struct my_funny_elem {
//   my_funny_elem(std::string_view name_, f32 x_, f32 y_) noexcept : name{name_}, x{x_}, y{y_} {}
//
//   f32 len() const { return std::sqrt((x * x) + (y * y)); }
//
//   std::string_view name;
//   f32 x, y;
// };
//
// } // namespace
//
// TEST_CASE("sparse_set construct", "[sparse_set]") {
//   ntf::sparse_set<my_funny_elem> elems;
//   REQUIRE(elems.empty());
//   REQUIRE(elems.size() == 0);
//   REQUIRE(elems.capacity() == 0);
//   REQUIRE(elems.page_capacity() == 0);
//   REQUIRE(elems.pages() == 0);
// }
//
// TEST_CASE("sparse_set push single", "[sparse_set]") {
//   ntf::sparse_set<my_funny_elem> elems;
//   auto& elem = elems.emplace(0, "amoma", 1.f, 1.f);
//   REQUIRE(!elems.empty());
//   REQUIRE(elems.size() == 1);
//   REQUIRE(elems.capacity() == 2);
//   REQUIRE(elems.pages() == 1);
//   REQUIRE(elems.page_capacity() == 1);
//   REQUIRE(elems.has_element(0));
//
//   REQUIRE(elem.name == "amoma");
//   REQUIRE(elem.x == 1.f);
//   REQUIRE(elem.y == 1.f);
//   {
//     auto& ref = elems.at(0);
//     REQUIRE(&elem == &ref);
//
//     auto* ptr = elems.at_ptr(0);
//     REQUIRE(ptr != nullptr);
//     REQUIRE(&elem == ptr);
//   }
//
//   elems.clear();
//   REQUIRE(elems.empty());
//   REQUIRE(elems.size() == 0);
//   REQUIRE(elems.capacity() == 2);
//   REQUIRE(elems.pages() == 0);
//   REQUIRE(elems.page_capacity() == 1);
//
//   elems.reset();
//   REQUIRE(elems.empty());
//   REQUIRE(elems.size() == 0);
//   REQUIRE(elems.capacity() == 0);
//   REQUIRE(elems.page_capacity() == 0);
//   REQUIRE(elems.pages() == 0);
// }
//
// TEST_CASE("sparse_set push several", "[sparse_set]") {
//   {
//     ntf::sparse_set<my_funny_elem> elems;
//     elems.reserve(16);
//     REQUIRE(elems.capacity() == 16);
//     auto& a = elems.emplace(0, "amoma", 1.f, 1.f);
//     auto& b = elems.emplace(256, "omomo", 2.f, 2.f);
//
//     REQUIRE(elems.has_element(0));
//     REQUIRE(elems.has_element(256));
//   }
//
//   {
//     ntf::sparse_set<my_funny_elem> elems2{
//       {3, {"amoma", 1.f, 1.f}},
//       {2, {"omomo", 2.f, 2.f}},
//     };
//     REQUIRE(elems2.size() == 2);
//     REQUIRE(elems2.capacity() == 2);
//     REQUIRE(elems2.has_element(3));
//     REQUIRE(elems2.has_element(2));
//
//     auto* a = elems2.at_ptr(3);
//     REQUIRE(a != nullptr);
//     auto* b = elems2.at_ptr(2);
//     REQUIRE(b != nullptr);
//   }
// }
