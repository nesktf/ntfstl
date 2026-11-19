#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_templated.hpp>

#include <ntf/vec.hpp>

#include <vector>

using namespace ntf;

namespace {

template<typename Range>
struct EqualsRangeMatcher : Catch::Matchers::MatcherGenericBase {
  EqualsRangeMatcher(const Range& range) : range{range} {}

  template<typename OtherRange>
  bool match(const OtherRange& other) const {
    using std::begin;
    using std::end;
    return std::equal(begin(range), end(range), begin(other), end(other));
  }

  std::string describe() const override { return "Equals: " + Catch::rangeToString(range); }

private:
  const Range& range;
};

template<typename Range>
auto EqualsRange(const Range& range) -> EqualsRangeMatcher<Range> {
  return EqualsRangeMatcher<Range>{range};
}

struct LifetimeThing {
  static inline int constructions = 0;
  static inline int destructions = 0;
  static inline int copies = 0;
  static inline int moves = 0;

  static void reset() {
    constructions = 0;
    destructions = 0;
    copies = 0;
    moves = 0;
  }

  int value = 0;

  LifetimeThing() : value(0) { ++constructions; }

  explicit LifetimeThing(int v) : value(v) { ++constructions; }

  LifetimeThing(const LifetimeThing& other) : value(other.value) { ++copies; }

  LifetimeThing(LifetimeThing&& other) noexcept : value(other.value) {
    other.value = -1;
    ++moves;
  }

  ~LifetimeThing() { ++destructions; }

  LifetimeThing& operator=(const LifetimeThing& other) {
    if (this != &other) {
      value = other.value;
      ++copies;
    }
    return *this;
  }

  LifetimeThing& operator=(LifetimeThing&& other) noexcept {
    if (this != &other) {
      value = other.value;
      other.value = -1;
      ++moves;
    }
    return *this;
  }

  bool operator==(const LifetimeThing& other) const noexcept { return value == other.value; }
};

} // namespace

TEST_CASE("InplaceVec construction", "[vec/InplaceVec]") {
  SECTION("Default constructor") {
    InplaceVec<int, 32> vec;
    REQUIRE(vec.empty());
    REQUIRE(vec.size() == 0);
    REQUIRE(vec.capacity() == 32);
  }
#ifndef NTF_NO_STD
  SECTION("initializer_list") {
    InplaceVec<int, 32> vec{1, 2, 3, 4};
    REQUIRE(!vec.empty());
    REQUIRE(vec.size() == 4);
    REQUIRE(vec.capacity() == 32);
    REQUIRE_THAT(vec, EqualsRange(std::to_array<int>({1, 2, 3, 4})));
  }
  SECTION("Iterator copy") {
    std::vector<int> stl_vec{1, 2, 3, 4};
    InplaceVec<int, 32> vec(stl_vec.begin(), stl_vec.end());
    REQUIRE(!vec.empty());
    REQUIRE(vec.size() == 4);
    REQUIRE(vec.capacity() == 32);
    REQUIRE_THAT(vec, EqualsRange(std::to_array<int>({1, 2, 3, 4})));
  }
#endif
}

TEST_CASE("InplaceVec push & emplace", "[vec/InplaceVec]") {
  InplaceVec<int, 4> vec;
  vec.push_back(1);
  vec.push_back(2);
  vec.push_back(3);
  vec.push_back(4);
  REQUIRE(vec.size() == 4);
  REQUIRE_THAT(vec, EqualsRange(std::to_array<int>({1, 2, 3, 4})));
  REQUIRE_THROWS_AS(vec.push_back(5), BadAlloc);
  REQUIRE(vec.try_push_back(6) == nullptr);
  vec.clear();
  REQUIRE(vec.empty());
  vec.emplace_back(5);
  vec.emplace_back(6);
  vec.emplace_back(7);
  REQUIRE(vec.size() == 3);
  REQUIRE_THAT(vec, EqualsRange(std::to_array<int>({5, 6, 7})));
  REQUIRE_NOTHROW(vec.emplace_back(8));
  REQUIRE_THROWS_AS(vec.emplace_back(9), BadAlloc);
  REQUIRE(vec.try_push_back(10) == nullptr);
}

TEST_CASE("SmallVec inline storage and trivial memcpy", "[engine/util/vec]") {
  SmallVec<int, 4> vec;
  CHECK(vec.empty());
  CHECK(vec.size() == 0);
  CHECK(vec.capacity() == 4);
  CHECK(vec.is_small());

  vec.push_back(10);
  vec.push_back(20);
  vec.push_back(30);
  CHECK(vec.size() == 3);
  CHECK(vec.is_small());
  CHECK(vec[0] == 10);
  CHECK(vec[1] == 20);
  CHECK(vec[2] == 30);
  CHECK(vec.front() == 10);
  CHECK(vec.back() == 30);

  vec.push_back(40);
  CHECK(vec.is_small());
  vec.push_back(50);
  CHECK(!vec.is_small());
  CHECK(vec.size() == 5);
  CHECK(vec.capacity() >= 5);
  CHECK(vec[4] == 50);

  SmallVec<int, 4> copy_vec(vec);
  CHECK(!copy_vec.is_small());
  CHECK(copy_vec.size() == 5);
  CHECK(copy_vec == vec);

  SmallVec<int, 4> moved_vec(std::move(copy_vec));
  CHECK(!moved_vec.is_small());
  CHECK(moved_vec.size() == 5);
  CHECK(moved_vec[0] == 10);
  CHECK(copy_vec.empty());
  CHECK(copy_vec.is_small());

  SmallVec<int, 4> assigned;
  assigned = moved_vec;
  CHECK(assigned == moved_vec);

  moved_vec.pop_back();
  CHECK(moved_vec.size() == 4);
  CHECK(moved_vec.back() == 40);

  SmallVec<int, 4> shrunk_copy(moved_vec);
  CHECK(shrunk_copy.is_small());
  CHECK(shrunk_copy.size() == 4);

  assigned.clear();
  CHECK(assigned.empty());
}

TEST_CASE("SmallVec move optimization when inline", "[engine/util/vec]") {
  SmallVec<int, 4> a;
  a.push_back(1);
  a.push_back(2);
  CHECK(a.is_small());

  SmallVec<int, 4> b(std::move(a));
  CHECK(b.is_small());
  CHECK(b.size() == 2);
  CHECK(b[0] == 1);
  CHECK(b[1] == 2);
  CHECK(a.empty());
}

TEST_CASE("SmallVec non-trivial lifetime tracking", "[engine/util/vec]") {
  LifetimeThing::reset();

  {
    SmallVec<LifetimeThing, 2> vec;
    vec.emplace_back(1);
    vec.emplace_back(2);
    CHECK(vec.is_small());
    CHECK(LifetimeThing::constructions == 2);

    vec.emplace_back(3);
    CHECK(!vec.is_small());
    CHECK(vec.size() == 3);
    CHECK(vec[0].value == 1);
    CHECK(vec[1].value == 2);
    CHECK(vec[2].value == 3);
  }

  CHECK(LifetimeThing::constructions + LifetimeThing::moves == LifetimeThing::destructions);
}
