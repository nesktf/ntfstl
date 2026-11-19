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

template<typename T>
struct TrackingAlloc {
  using value_type = T;
  using pointer = T*;
  using size_type = size_t;
  using difference_type = ptrdiff_t;

  template<typename U>
  using rebind = TrackingAlloc<U>;

  struct Stats {
    size_t allocations = 0;
    size_t deallocations = 0;
    size_t bytes_allocated = 0;
  };

  Stats* stats = nullptr;
  int id = 0;

  constexpr TrackingAlloc() noexcept = default;

  constexpr TrackingAlloc(Stats* s, int i = 0) noexcept : stats(s), id(i) {}

  constexpr TrackingAlloc(const TrackingAlloc&) noexcept = default;
  constexpr TrackingAlloc& operator=(const TrackingAlloc&) noexcept = default;

  template<typename U>
  constexpr TrackingAlloc(const TrackingAlloc<U>& other) noexcept :
      stats(other.stats), id(other.id) {}

  T* allocate(size_t n) {
    if (stats) {
      stats->allocations++;
      stats->bytes_allocated += n * sizeof(T);
    }
    T* ptr = static_cast<T*>(::malloc(n * sizeof(T)));
    NTF_THROW_IF(!ptr, BadAlloc());
    return ptr;
  }

  void deallocate(T* ptr, size_t n) noexcept {
    if (stats) {
      stats->deallocations++;
    }
    NTF_UNUSED(n);
    ::free(ptr);
  }

  constexpr bool operator==(const TrackingAlloc& other) const noexcept { return id == other.id; }
};

} // namespace

TEST_CASE("InlineVec construction", "[vec/InlineVec]") {
  SECTION("Default constructor") {
    InlineVec<int, 32> vec;
    REQUIRE(vec.empty());
    REQUIRE(vec.size() == 0);
    REQUIRE(vec.capacity() == 32);
  }
#ifndef NTF_NO_STD
  SECTION("initializer_list") {
    InlineVec<int, 32> vec{1, 2, 3, 4};
    REQUIRE(!vec.empty());
    REQUIRE(vec.size() == 4);
    REQUIRE(vec.capacity() == 32);
    REQUIRE_THAT(vec, EqualsRange(std::to_array<int>({1, 2, 3, 4})));
  }
  SECTION("Iterator copy") {
    std::vector<int> stl_vec{1, 2, 3, 4};
    InlineVec<int, 32> vec(stl_vec.begin(), stl_vec.end());
    REQUIRE(!vec.empty());
    REQUIRE(vec.size() == 4);
    REQUIRE(vec.capacity() == 32);
    REQUIRE_THAT(vec, EqualsRange(std::to_array<int>({1, 2, 3, 4})));
  }
#endif
  SECTION("Pointer iterator copy") {
    int arr[4] = {10, 20, 30, 40};
    InlineVec<int, 32> vec(arr, arr + 4);
    REQUIRE(!vec.empty());
    REQUIRE(vec.size() == 4);
    REQUIRE(vec.capacity() == 32);
    REQUIRE(vec[0] == 10);
    REQUIRE(vec[1] == 20);
    REQUIRE(vec[2] == 30);
    REQUIRE(vec[3] == 40);
  }
}

TEST_CASE("InlineVec push & emplace", "[vec/InlineVec]") {
  InlineVec<int, 4> vec;
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

TEST_CASE("InlineVec trivial copy and move operations", "[vec/InlineVec]") {
  InlineVec<int, 4> a;
  a.push_back(10);
  a.push_back(20);

  InlineVec<int, 4> copy_constructed(a);
  CHECK(copy_constructed.size() == 2);
  CHECK(copy_constructed[0] == 10);
  CHECK(copy_constructed[1] == 20);
  CHECK(a.size() == 2);

  InlineVec<int, 4> copy_assigned;
  copy_assigned.push_back(99);
  copy_assigned = a;
  CHECK(copy_assigned.size() == 2);
  CHECK(copy_assigned[0] == 10);
  CHECK(copy_assigned[1] == 20);
  CHECK(a.size() == 2);

  copy_assigned = copy_assigned;
  CHECK(copy_assigned.size() == 2);
  CHECK(copy_assigned[0] == 10);

  InlineVec<int, 4> move_constructed(std::move(a));
  CHECK(move_constructed.size() == 2);
  CHECK(move_constructed[0] == 10);
  CHECK(move_constructed[1] == 20);

  InlineVec<int, 4> move_assigned;
  move_assigned.push_back(50);
  move_assigned = std::move(move_constructed);
  CHECK(move_assigned.size() == 2);
  CHECK(move_assigned[0] == 10);
  CHECK(move_assigned[1] == 20);
  CHECK(move_constructed.empty());
}

TEST_CASE("InlineVec non-trivial copy and move operations", "[vec/InlineVec]") {
  LifetimeThing::reset();

  {
    InlineVec<LifetimeThing, 4> a;
    a.emplace_back(1);
    a.emplace_back(2);
    CHECK(a.size() == 2);
    CHECK(LifetimeThing::constructions == 2);

    InlineVec<LifetimeThing, 4> copy_constructed(a);
    CHECK(copy_constructed.size() == 2);
    CHECK(copy_constructed[0].value == 1);
    CHECK(copy_constructed[1].value == 2);
    CHECK(a.size() == 2);
    CHECK(LifetimeThing::copies == 2);

    InlineVec<LifetimeThing, 4> copy_assigned;
    copy_assigned.emplace_back(100);
    copy_assigned.emplace_back(200);
    copy_assigned.emplace_back(300);
    copy_assigned = a;
    CHECK(copy_assigned.size() == 2);
    CHECK(copy_assigned[0].value == 1);
    CHECK(copy_assigned[1].value == 2);
    CHECK(a.size() == 2);

    copy_assigned = copy_assigned;
    CHECK(copy_assigned.size() == 2);
    CHECK(copy_assigned[0].value == 1);

    InlineVec<LifetimeThing, 4> move_constructed(std::move(a));
    CHECK(move_constructed.size() == 2);
    CHECK(move_constructed[0].value == 1);
    CHECK(move_constructed[1].value == 2);

    InlineVec<LifetimeThing, 4> move_assigned;
    move_assigned.emplace_back(500);
    move_assigned = std::move(move_constructed);
    CHECK(move_assigned.size() == 2);
    CHECK(move_assigned[0].value == 1);
    CHECK(move_assigned[1].value == 2);
    CHECK(move_constructed.empty());
  }

  CHECK(LifetimeThing::constructions + LifetimeThing::copies + LifetimeThing::moves ==
        LifetimeThing::destructions);
}

TEST_CASE("SmallVec inline storage and trivial memcpy", "[vec/SmallVec]") {
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

  int arr[] = {1, 2, 3, 4, 5};
  SmallVec<int, 4> from_iter(arr, arr + 5);
  CHECK(from_iter.size() == 5);
  CHECK(!from_iter.is_small());
  CHECK(from_iter[0] == 1);
  CHECK(from_iter[4] == 5);
}

TEST_CASE("SmallVec move optimization when inline", "[vec/SmallVec]") {
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

TEST_CASE("SmallVec non-trivial lifetime tracking", "[vec/SmallVec]") {
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

TEST_CASE("SmallVec custom allocator and propagation", "[vec/SmallVec]") {
  TrackingAlloc<int>::Stats stats;
  TrackingAlloc<int> alloc{&stats, 42};

  {
    SmallVec<int, 4, TrackingAlloc<int>> vec(alloc);
    CHECK(vec.allocator().id == 42);
    CHECK(vec.empty());
    CHECK(vec.is_small());
    CHECK(stats.allocations == 0);
    CHECK(stats.deallocations == 0);

    for (int i = 0; i < 4; ++i) {
      vec.push_back(i);
    }
    CHECK(vec.size() == 4);
    CHECK(vec.is_small());
    CHECK(stats.allocations == 0);

    vec.push_back(4);
    CHECK(!vec.is_small());
    CHECK(stats.allocations == 1);
    CHECK(stats.deallocations == 0);

    for (int i = 5; i < 8; ++i) {
      vec.push_back(i);
    }
    CHECK(stats.allocations == 1);

    vec.push_back(8);
    CHECK(stats.allocations == 2);
    CHECK(stats.deallocations == 1);
    CHECK(vec.size() == 9);

    SmallVec<int, 4, TrackingAlloc<int>> copy_vec(vec);
    CHECK(copy_vec.allocator().id == 42);
    CHECK(!copy_vec.is_small());
    CHECK(copy_vec.size() == 9);
    CHECK(stats.allocations == 3);

    SmallVec<int, 4, TrackingAlloc<int>> moved_vec(std::move(copy_vec));
    CHECK(moved_vec.allocator().id == 42);
    CHECK(!moved_vec.is_small());
    CHECK(moved_vec.size() == 9);
    CHECK(stats.allocations == 3);
  }

  CHECK(stats.allocations == stats.deallocations);

  {
    TrackingAlloc<int>::Stats count_stats;
    TrackingAlloc<int> count_alloc{&count_stats, 7};

    SmallVec<int, 4, TrackingAlloc<int>> small_sized(3, count_alloc);
    CHECK(small_sized.is_small());
    CHECK(small_sized.size() == 3);
    CHECK(count_stats.allocations == 0);

    SmallVec<int, 4, TrackingAlloc<int>> large_sized(10, count_alloc);
    CHECK(!large_sized.is_small());
    CHECK(large_sized.size() == 10);
    CHECK(count_stats.allocations == 1);
  }
}
