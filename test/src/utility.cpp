#include <catch2/catch_test_macros.hpp>
#include <ntfstl/utility.hpp>

namespace {

class thingy {
public:
  constexpr thingy(int val = 0) noexcept : _val{val} {}

public:
  constexpr int sum_thing(int other) const noexcept { return _val + other; }

public:
  static constexpr int sum(thingy thing, int other) noexcept { return thing.sum_thing(other); }

private:
  int _val;
};

} // namespace

TEST_CASE("lambda_wrap", "[utility]") {
  thingy thing{3};
  auto wrap1 = ntf::lambda_wrap<&thingy::sum_thing>(thing);
  REQUIRE(wrap1(3) == 6);
  auto wrap2 = ntf::lambda_wrap<&thingy::sum_thing>(&thing);
  REQUIRE(wrap2(4) == 7);
}

TEST_CASE("bind_front", "[utility]") {
  constexpr auto bind1 = ntf::bind_front<&thingy::sum, 3>();
  constexpr auto ret = bind1(3);
  static_assert(ret == 6);
}

TEST_CASE("bind_back", "[utility]") {
  constexpr thingy thing{3};
  constexpr auto bind2 = ntf::bind_back<&thingy::sum, 3>();
  constexpr auto ret2 = bind2(thing);
  static_assert(ret2 == 6);
}
