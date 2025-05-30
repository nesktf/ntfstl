#include <catch2/catch_test_macros.hpp>
#include <ntfstl/function.hpp>

namespace {

class funny_functor {
  int _val;

public:
  funny_functor(int val) : _val{val} {}

  int operator()(int a) {
    return _val * a;
  }
};

int fptr(int a) {
  return a*2;
}

int view_fun(ntf::function_view<int()> fun, const int& value) {
  return fun() + value;
}

} // namespace

TEST_CASE("inplace function empty construction", "[function]") {
  ntf::inplace_function<int(int)> f;
  REQUIRE(f.empty());

  ntf::inplace_function<int(int)> g = nullptr;
  REQUIRE(g.empty());
}

TEST_CASE("inplace function construction with function", "[function]") {
  SECTION("Construct with lambda") {
    ntf::inplace_function<int(int)> f = [](int a) {
      return a + 2;
    };
    REQUIRE(!f.empty());
    REQUIRE(f(6) == 8);
  }
  SECTION("Construct with function pointer") {
    ntf::inplace_function<int(int)> f = &fptr;
    REQUIRE(!f.empty());
    REQUIRE(f(8) == 16);
  }
  SECTION("Construct in place with functor"){
    ntf::inplace_function<int(int)> f{std::in_place_type_t<funny_functor>{}, 2};
    REQUIRE(!f.empty());
    REQUIRE(f(2) == 4);
  }
}

TEST_CASE("inplace function copy operations", "[function]") {
  int a = 7;
  ntf::inplace_function<int(int)> f = [a](int b) {
    return a+b;
  };
  REQUIRE(!f.empty());
  const auto fret = f(3);
  REQUIRE(fret == 10);

  ntf::inplace_function<int(int)> g = f;
  REQUIRE(!g.empty());
  REQUIRE(g(3) == fret);
}

TEST_CASE("inplace function move operations", "[function]") {
  int a = 10;
  ntf::inplace_function<int(int)> f = [a](int b) {
    return a*b;
  };
  REQUIRE(!f.empty());
  const auto fret = f(2);
  REQUIRE(fret == 20);

  ntf::inplace_function<int(int)> g = std::move(f);
  REQUIRE(!g.empty());
  REQUIRE(g(2) == fret);
  // REQUIRE(f.empty());
}

TEST_CASE("inplace function assignment", "[function]") {
  SECTION("Assign null") {
    ntf::inplace_function<void()> f = []() {};
    REQUIRE(!f.empty());
    f = nullptr;
    REQUIRE(f.empty());
  }
  SECTION("Assign lambda") {
    ntf::inplace_function<int(int)> f = [](int) { return 4; };
    REQUIRE(!f.empty());

    f = [](int) { return 2; };
    REQUIRE(!f.empty());
    REQUIRE(f(2) == 2);
  }
  SECTION("Assign function pointer") {
    ntf::inplace_function<int(int)> f = [](int) { return 4; };
    REQUIRE(!f.empty());

    f = +[](int) { return 8; };
    REQUIRE(!f.empty());
    REQUIRE(f(8) == 8);
  }
}
