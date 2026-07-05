#include <catch2/catch_test_macros.hpp>
#include <ntf/func_util.hpp>

namespace {

class Thingy {
public:
  constexpr Thingy(int val = 0) noexcept : _val{val} {}

public:
  constexpr int sum_thing(int other) const noexcept { return _val + other; }

public:
  static constexpr int sum(Thingy thing, int other) noexcept { return thing.sum_thing(other); }

private:
  int _val;
};

} // namespace

TEST_CASE("bind_front", "[utility]") {
  constexpr auto bind1 = ntf::bind_front<&Thingy::sum, 3>();
  constexpr auto ret = bind1(3);
  static_assert(ret == 6);
}

TEST_CASE("bind_back", "[utility]") {
  constexpr Thingy thing{3};
  constexpr auto bind2 = ntf::bind_back<&Thingy::sum, 3>();
  constexpr auto ret2 = bind2(thing);
  static_assert(ret2 == 6);
}
