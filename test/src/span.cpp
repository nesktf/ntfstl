#include <catch2/catch_test_macros.hpp>
#include <ntf/span.hpp>
#include <ntf/vec.hpp>

#ifndef NTF_NO_STD
#include <vector>
#endif

namespace {

struct CustomRange {
  int values[3] = {100, 200, 300};
  int* begin() noexcept { return values; }
  int* end() noexcept { return values + 3; }
  const int* begin() const noexcept { return values; }
  const int* end() const noexcept { return values + 3; }
};

} // namespace

TEST_CASE("Span construction", "[Span]") {
  SECTION("Default construction") {
    static constexpr ntf::Span<int> const_span;
    static_assert(const_span.size() == 0);
    static_assert(const_span.data() == nullptr);
    static_assert(const_span.empty());
  }

  SECTION("Pointer and count") {
    int arr[] = {1, 2, 3, 4};
    ntf::Span<int> s(arr, 4);
    CHECK(s.size() == 4);
    CHECK(s.data() == arr);
    CHECK(s[0] == 1);
    CHECK(s[3] == 4);

    ntf::Span<int, 4> s_fixed(arr, 4);
    CHECK(s_fixed.size() == 4);
    CHECK(s_fixed.data() == arr);
  }

  SECTION("Iterator pair") {
    int arr[] = {10, 20, 30};
    ntf::Span<int> s(arr, arr + 3);
    CHECK(s.size() == 3);
    CHECK(s[0] == 10);
    CHECK(s[2] == 30);
  }

  SECTION("C array") {
    int arr[5] = {1, 2, 3, 4, 5};
    ntf::Span<int> s_dyn(arr);
    CHECK(s_dyn.size() == 5);
    CHECK(s_dyn.data() == arr);

    ntf::Span<int, 5> s_fixed(arr);
    CHECK(s_fixed.size() == 5);
    CHECK(s_fixed.data() == arr);
  }

  SECTION("Single object") {
    int val = 42;
    ntf::Span<int, 1> s_one(val);
    CHECK(s_one.size() == 1);
    CHECK(s_one[0] == 42);

    ntf::Span<int> s_dyn(val);
    CHECK(s_dyn.size() == 1);
    CHECK(s_dyn[0] == 42);
  }
}

TEST_CASE("Span contiguous range construction", "[Span]") {
  SECTION("From InlineVec") {
    ntf::InlineVec<int, 4> vec;
    vec.push_back(10);
    vec.push_back(20);
    vec.push_back(30);

    ntf::Span<int> s = vec;
    CHECK(s.size() == 3);
    CHECK(s.data() == vec.data());
    CHECK(s[0] == 10);
    CHECK(s[1] == 20);
    CHECK(s[2] == 30);

    ntf::Span<int, 3> s_fixed(vec);
    CHECK(s_fixed.size() == 3);
    CHECK(s_fixed[1] == 20);

    const ntf::InlineVec<int, 4>& const_vec = vec;
    ntf::Span<const int> s_const = const_vec;
    CHECK(s_const.size() == 3);
    CHECK(s_const[2] == 30);
  }

  SECTION("From SmallVec inline and heap") {
    ntf::SmallVec<int, 4> sv;
    sv.push_back(1);
    sv.push_back(2);
    CHECK(sv.is_small());

    ntf::Span<int> s_small = sv;
    CHECK(s_small.size() == 2);
    CHECK(s_small[0] == 1);
    CHECK(s_small[1] == 2);

    for (int i = 3; i <= 6; ++i) {
      sv.push_back(i);
    }
    CHECK(!sv.is_small());

    ntf::Span<int> s_heap = sv;
    CHECK(s_heap.size() == 6);
    CHECK(s_heap.data() == sv.data());
    CHECK(s_heap[5] == 6);
  }

  SECTION("From custom container with only begin and end") {
    CustomRange cr;
    ntf::Span<int> s = cr;
    CHECK(s.size() == 3);
    CHECK(s[0] == 100);
    CHECK(s[1] == 200);
    CHECK(s[2] == 300);

    const CustomRange ccr;
    ntf::Span<const int> cs = ccr;
    CHECK(cs.size() == 3);
    CHECK(cs[0] == 100);
  }

#ifndef NTF_NO_STD
  SECTION("From std::vector") {
    std::vector<int> v = {7, 8, 9};
    ntf::Span<int> s = v;
    CHECK(s.size() == 3);
    CHECK(s.data() == v.data());
    CHECK(s[0] == 7);
  }
#endif
}

TEST_CASE("Span element access and observers", "[Span]") {
  int arr[] = {10, 20, 30, 40};
  ntf::Span<int> s(arr);

  CHECK(s.size() == 4);
  CHECK(s.size_bytes() == 4 * sizeof(int));
  CHECK(!s.empty());
  CHECK(static_cast<bool>(s));
  CHECK(s.front() == 10);
  CHECK(s.back() == 40);
  CHECK(s[1] == 20);

  CHECK(s.at(0) == 10);
  CHECK(s.at(3) == 40);
  REQUIRE_THROWS_AS(s.at(4), ntf::MsgException);

  CHECK(s.at_opt(2) == arr + 2);
  CHECK(s.at_opt(4) == nullptr);

  const ntf::Span<int> const_s = s;
  CHECK(const_s.at_opt(1) == arr + 1);
  CHECK(const_s.at_opt(10) == nullptr);
}

TEST_CASE("Span subviews first and last", "[Span]") {
  int arr[] = {1, 2, 3, 4, 5};
  ntf::Span<int> s(arr);

  auto f_static = s.first<3>();
  CHECK(f_static.size() == 3);
  CHECK(f_static[0] == 1);
  CHECK(f_static[2] == 3);

  auto f_dyn = s.first(2);
  CHECK(f_dyn.size() == 2);
  CHECK(f_dyn[0] == 1);
  CHECK(f_dyn[1] == 2);

  auto f_all = s.first(5);
  CHECK(f_all.size() == 5);

  auto f_zero = s.first(0);
  CHECK(f_zero.empty());

  auto l_static = s.last<2>();
  CHECK(l_static.size() == 2);
  CHECK(l_static[0] == 4);
  CHECK(l_static[1] == 5);

  auto l_dyn = s.last(3);
  CHECK(l_dyn.size() == 3);
  CHECK(l_dyn[0] == 3);
  CHECK(l_dyn[2] == 5);

  auto l_all = s.last(5);
  CHECK(l_all.size() == 5);

  auto l_zero = s.last(0);
  CHECK(l_zero.empty());
}

TEST_CASE("Span iteration and reverse iteration", "[Span]") {
  int arr[] = {10, 20, 30, 40};
  ntf::Span<int> s(arr);

  int sum = 0;
  for (int x : s) {
    sum += x;
  }
  CHECK(sum == 100);

  int rev_expected[] = {40, 30, 20, 10};
  int idx = 0;
  for (auto it = s.rbegin(); it != s.rend(); ++it) {
    CHECK(*it == rev_expected[idx++]);
  }
  CHECK(idx == 4);
}

TEST_CASE("Span conversions and assignment", "[Span]") {
  int arr[] = {1, 2, 3};
  ntf::Span<int, 3> fixed(arr);

  ntf::Span<int> dyn(fixed);
  CHECK(dyn.size() == 3);
  CHECK(dyn[0] == 1);

  ntf::Span<const int> const_dyn(dyn);
  CHECK(const_dyn.size() == 3);
  CHECK(const_dyn[0] == 1);

  ntf::Span<int> reassigned;
  CHECK(reassigned.empty());
  reassigned = dyn;
  CHECK(reassigned.size() == 3);
  CHECK(reassigned.data() == arr);
}
