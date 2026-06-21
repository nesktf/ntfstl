#include <catch2/catch_test_macros.hpp>
#include <ntf/ref.hpp>

TEST_CASE("Ref construction", "[Ref]") {
  int value = 0;
  ntf::Ref<int> v = value;
  REQUIRE(v.get() == value);
  REQUIRE(*v == value);
  REQUIRE(v.ptr() == &value);

  ntf::Ref<const int> cv = value;
  REQUIRE(cv.get() == value);
  REQUIRE(*cv == value);
  REQUIRE(cv.ptr() == &value);
}

class FunnyFn {
  int _val;

public:
  FunnyFn(int val) : _val{val} {}

  int operator()(int a) { return _val * a; }
};

int fptr(int a) {
  return a * 2;
}

TEST_CASE("FnRef construction", "[FnRef]") {
  SECTION("Construct with lambda") {
    const auto l = [](int a) {
      return a + 2;
    };
    ntf::FnRef<int(int) const> f = l;
    REQUIRE(f(6) == 8);
  }
}
