#include <catch2/catch_test_macros.hpp>
#include <ntf/func.hpp>

namespace {

class FunnyFn {
  int _val;

public:
  FunnyFn(int val) : _val{val} {}

  int operator()(int a) { return _val * a; }
};

int fptr(int a) {
  return a * 2;
}

} // namespace

TEST_CASE("TrivFn construction", "[TrivFn]") {
  SECTION("Construct with lambda") {
    ntf::TrivFn<int(int), 2 * sizeof(void*), 8> f = [](int a) {
      return a + 2;
    };
    REQUIRE(f(6) == 8);
  }
  SECTION("Construct with function pointer") {
    ntf::TrivFn<int(int), 2 * sizeof(void*), 8> f = &fptr;
    REQUIRE(f(8) == 16);
  }
  SECTION("Construct in place") {
    ntf::TrivFn<int(int), 2 * sizeof(void*), 8> f{ntf::in_place_type<FunnyFn>, 2};
    REQUIRE(f(2) == 4);
  }
}

TEST_CASE("TrivFn copy operations", "[TrivFn]") {
  int a = 7;
  ntf::TrivFn<int(int), 2 * sizeof(void*), 8> f = [a](int b) {
    return a + b;
  };
  const auto fret = f(3);
  REQUIRE(fret == 10);

  auto g = f;
  REQUIRE(g(3) == fret);
}

TEST_CASE("TrivFn assignment", "[TrivFn]") {
  SECTION("Assign lambda") {
    ntf::TrivFn<int(int), 2 * sizeof(void*), 8> f = [](int) {
      return 4;
    };

    f = [](int) {
      return 2;
    };
    REQUIRE(f(2) == 2);
  }
  SECTION("Assign function pointer") {
    ntf::TrivFn<int(int), 2 * sizeof(void*), 8> f = [](int) {
      return 4;
    };

    f = +[](int) {
      return 8;
    };
    REQUIRE(f(8) == 8);
  }
}
