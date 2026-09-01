#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_templated.hpp>

#include <ntf/vec.hpp>

#include <vector>

namespace {

template<typename Range>
struct EqualsRangeMatcher : Catch::Matchers::MatcherGenericBase {
  EqualsRangeMatcher(const Range & range) : range{range} {}

  template<typename OtherRange>
  bool match(const OtherRange & other) const {
    using std::begin; using std::end;
    return std::equal(begin(range), end(range), begin(other), end(other));
  }

  std::string describe() const override {
    return "Equals: " + Catch::rangeToString(range);
  }

private:
  const Range& range;
};

template<typename Range>
auto EqualsRange(const Range& range) -> EqualsRangeMatcher<Range> {
  return EqualsRangeMatcher<Range>{range};
}

} // namespace

TEST_CASE("InplaceVec construction", "[vec/InplaceVec]") {
  SECTION("Default constructor") {
    ntf::InplaceVec<int, 32> vec;
    REQUIRE(vec.empty());
    REQUIRE(vec.size() == 0);
    REQUIRE(vec.capacity() == 32);
  }
#ifndef NTF_NO_STD
  SECTION("initializer_list") {
    ntf::InplaceVec<int, 32> vec{1, 2, 3, 4};
    REQUIRE(!vec.empty());
    REQUIRE(vec.size() == 4);
    REQUIRE(vec.capacity() == 32);
    REQUIRE_THAT(vec, EqualsRange(std::to_array<int>({1, 2, 3, 4})));
  }
  SECTION("Iterator copy") {
    std::vector<int> stl_vec{1, 2, 3, 4};
    ntf::InplaceVec<int, 32> vec(stl_vec.begin(), stl_vec.end());
    REQUIRE(!vec.empty());
    REQUIRE(vec.size() == 4);
    REQUIRE(vec.capacity() == 32);
    REQUIRE_THAT(vec, EqualsRange(std::to_array<int>({1, 2, 3, 4})));
  }
#endif
}

TEST_CASE("InplaceVec push & emplace", "[vec/InplaceVec]") {
  ntf::InplaceVec<int, 4> vec;
  vec.push_back(1);
  vec.push_back(2);
  vec.push_back(3);
  vec.push_back(4);
  REQUIRE(vec.size() == 4);
  REQUIRE_THAT(vec, EqualsRange(std::to_array<int>({1, 2, 3, 4})));
  REQUIRE_THROWS_AS(vec.push_back(5), ntf::BadAlloc);
  REQUIRE(vec.try_push_back(6) == nullptr);
  vec.clear();
  REQUIRE(vec.empty());
  vec.emplace_back(5);
  vec.emplace_back(6);
  vec.emplace_back(7);
  REQUIRE(vec.size() == 3);
  REQUIRE_THAT(vec, EqualsRange(std::to_array<int>({5, 6, 7})));
  REQUIRE_NOTHROW(vec.emplace_back(8));
  REQUIRE_THROWS_AS(vec.emplace_back(9), ntf::BadAlloc);
  REQUIRE(vec.try_push_back(10) == nullptr);
}
