#include <catch2/catch_test_macros.hpp>
#include <charconv>
#include <ntf/optional.hpp>

namespace {

struct NullableThing {
  constexpr NullableThing(int val = 0) noexcept : value{val} {}

  constexpr bool operator==(const NullableThing& other) const { return value == other.value; }

  int value;
};

struct LifetimeThing {
  static inline int constructions = 0;
  static inline int destructions = 0;
  static inline int copy_constructions = 0;
  static inline int copy_assignments = 0;
  static inline int move_constructions = 0;
  static inline int move_assignments = 0;

  static void reset() {
    constructions = 0;
    destructions = 0;
    copy_constructions = 0;
    copy_assignments = 0;
    move_constructions = 0;
    move_assignments = 0;
  }

  int value = 0;

  LifetimeThing() : value(0) { ++constructions; }
  explicit LifetimeThing(int v) : value(v) { ++constructions; }
  LifetimeThing(const LifetimeThing& other) : value(other.value) { ++copy_constructions; }
  LifetimeThing(LifetimeThing&& other) noexcept : value(other.value) {
    other.value = -1;
    ++move_constructions;
  }
  ~LifetimeThing() { ++destructions; }

  LifetimeThing& operator=(const LifetimeThing& other) {
    if (this != &other) {
      value = other.value;
      ++copy_assignments;
    }
    return *this;
  }

  LifetimeThing& operator=(LifetimeThing&& other) noexcept {
    if (this != &other) {
      value = other.value;
      other.value = -1;
      ++move_assignments;
    }
    return *this;
  }

  bool operator==(const LifetimeThing& other) const noexcept { return value == other.value; }
};

} // namespace

static void* const some_address = reinterpret_cast<void*>(0xCAFEBABE);

TEST_CASE("optional construction", "[optional]") {
  SECTION("default construction") {
    ntf::Optional<int> opt_basic;
    REQUIRE(!opt_basic.has_value());

    ntf::Optional<void*> opt_ptr;
    REQUIRE(!opt_ptr.has_value());

    ntf::Optional<NullableThing> opt_custom;
    REQUIRE(!opt_custom.has_value());
  }

  SECTION("tag null construction") {
    ntf::Optional<int> opt_basic{ntf::nullopt};
    REQUIRE(!opt_basic.has_value());

    ntf::Optional<void*> opt_ptr{ntf::nullopt};
    REQUIRE(!opt_ptr.has_value());

    ntf::Optional<NullableThing> opt_custom{ntf::nullopt};
    REQUIRE(!opt_custom.has_value());
  }

  SECTION("forward construction") {
    ntf::Optional<int> opt_basic{1};
    REQUIRE(opt_basic.has_value());
    REQUIRE(*opt_basic == 1);

    ntf::Optional<void*> opt_ptr{some_address};
    REQUIRE(opt_ptr.has_value());
    REQUIRE(*opt_ptr == some_address);

    ntf::Optional<void*> opt_ptr_null{nullptr};
    REQUIRE(opt_ptr_null.has_value());

    ntf::Optional<NullableThing> opt_custom{NullableThing{1}};
    REQUIRE(opt_custom.has_value());
    REQUIRE((*opt_custom).value == 1);

    ntf::Optional<NullableThing> opt_custom_null{NullableThing{}};
    REQUIRE(opt_custom_null.has_value());
  }

  SECTION("inplace construction") {
    ntf::Optional<int> opt_basic{ntf::in_place, 1};
    REQUIRE(opt_basic.has_value());
    REQUIRE(*opt_basic == 1);

    ntf::Optional<void*> opt_ptr{ntf::in_place, some_address};
    REQUIRE(opt_ptr.has_value());
    REQUIRE(*opt_ptr == some_address);

    ntf::Optional<void*> opt_ptr_null{ntf::in_place, nullptr};
    REQUIRE(opt_ptr_null.has_value());

    ntf::Optional<NullableThing> opt_custom{ntf::in_place, 1};
    REQUIRE(opt_custom.has_value());
    REQUIRE((*opt_custom).value == 1);

    ntf::Optional<NullableThing> opt_custom_null{ntf::in_place, 0};
    REQUIRE(opt_custom_null.has_value());
  }
}

TEST_CASE("optional copy operations", "[optional]") {
  SECTION("copy construction null") {
    ntf::Optional<int> opt_basic;
    auto opt_basic_copy = opt_basic;
    REQUIRE(!opt_basic_copy.has_value());

    ntf::Optional<void*> opt_ptr;
    auto opt_ptr_copy = opt_ptr;
    REQUIRE(!opt_ptr_copy.has_value());

    ntf::Optional<NullableThing> opt_custom;
    auto opt_custom_copy = opt_custom;
    REQUIRE(!opt_custom_copy.has_value());
  }
  SECTION("copy construction normal") {
    ntf::Optional<int> opt_basic{1};
    auto opt_basic_copy = opt_basic;
    REQUIRE(opt_basic_copy.has_value());

    ntf::Optional<void*> opt_ptr{some_address};
    auto opt_ptr_copy = opt_ptr;
    REQUIRE(opt_ptr_copy.has_value());

    ntf::Optional<NullableThing> opt_custom{NullableThing{1}};
    auto opt_custom_copy = opt_custom;
    REQUIRE(opt_custom_copy.has_value());
  }
  SECTION("copy assignment null") {
    ntf::Optional<int> opt_basic_src_null;
    ntf::Optional<int> opt_basic_dst{1};
    opt_basic_dst = opt_basic_src_null;
    REQUIRE(!opt_basic_dst.has_value());
  }
  SECTION("copt assignemnt normal") {
    ntf::Optional<int> opt_basic_src_val{2};
    ntf::Optional<int> opt_basic_dst{1};
    opt_basic_dst = opt_basic_src_val;
    REQUIRE(opt_basic_dst.has_value());
    REQUIRE(*opt_basic_dst == 2);
  }
}

TEST_CASE("optional move operations", "[optional]") {
  SECTION("move construction null") {
    ntf::Optional<int> opt_basic;
    auto opt_basic_copy = ntf::move(opt_basic);
    REQUIRE(!opt_basic_copy.has_value());
    REQUIRE(!opt_basic.has_value());

    ntf::Optional<void*> opt_ptr;
    auto opt_ptr_copy = ntf::move(opt_ptr);
    REQUIRE(!opt_ptr_copy.has_value());
    REQUIRE(!opt_ptr.has_value());

    ntf::Optional<NullableThing> opt_custom;
    auto opt_custom_copy = ntf::move(opt_custom);
    REQUIRE(!opt_custom_copy.has_value());
    REQUIRE(!opt_custom.has_value());
  }
  SECTION("move construction normal") {
    ntf::Optional<int> opt_basic{1};
    auto opt_basic_copy = ntf::move(opt_basic);
    REQUIRE(opt_basic_copy.has_value());

    ntf::Optional<void*> opt_ptr{some_address};
    auto opt_ptr_copy = ntf::move(opt_ptr);
    REQUIRE(opt_ptr_copy.has_value());

    ntf::Optional<NullableThing> opt_custom{NullableThing{1}};
    auto opt_custom_copy = ntf::move(opt_custom);
    REQUIRE(opt_custom_copy.has_value());
  }
}

static ntf::Optional<int> parse_string(std::string_view str) {
  int out;
  auto ret = std::from_chars(str.begin(), str.end(), out);
  if (ret.ptr != str.end()) {
    return {ntf::nullopt};
  }
  return {ntf::in_place, out};
}

TEST_CASE("optional monadic operations", "[optional]") {
  SECTION("and_then") {
    auto lvalue = parse_string("4");
    auto lvalue_ret =
      lvalue.and_then([](int value) -> ntf::Optional<int> { return {ntf::in_place, value * 3}; });
    REQUIRE(lvalue_ret.has_value());
    REQUIRE(*lvalue_ret == 12);

    const auto const_lvalue = parse_string("8");
    auto const_lvalue_ret = const_lvalue.and_then(
      [](int value) -> ntf::Optional<int> { return {ntf::in_place, value * 4}; });
    REQUIRE(const_lvalue.has_value());
    REQUIRE(*const_lvalue_ret == 32);

    auto rvalue_ret = parse_string("2").and_then(
      [](int value) -> ntf::Optional<int> { return {ntf::in_place, value * 2}; });
    REQUIRE(rvalue_ret.has_value());
    REQUIRE(*rvalue_ret == 4);
  }

  SECTION("transform") {
    auto lvalue = parse_string("4");
    auto lvalue_ret = lvalue.transform([](int value) -> int { return value * 3; });
    REQUIRE(lvalue_ret.has_value());
    REQUIRE(*lvalue_ret == 12);

    const auto const_lvalue = parse_string("8");
    auto const_lvalue_ret = const_lvalue.transform([](int value) -> int { return value * 4; });
    REQUIRE(const_lvalue.has_value());
    REQUIRE(*const_lvalue_ret == 32);

    auto rvalue_ret = parse_string("2").transform([](int value) -> int { return value * 2; });
    REQUIRE(rvalue_ret.has_value());
    REQUIRE(*rvalue_ret == 4);
  }

  SECTION("or_else") {
    auto lvalue = parse_string("u");
    auto lvalue_ret = lvalue.or_else([]() -> ntf::Optional<int> { return {ntf::in_place, 2}; });
    REQUIRE(lvalue_ret.has_value());
    REQUIRE(*lvalue_ret == 2);

    auto rvalue_ret =
      parse_string("i").or_else([]() -> ntf::Optional<int> { return {ntf::in_place, 4}; });
    REQUIRE(rvalue_ret.has_value());
    REQUIRE(*rvalue_ret == 4);
  }
}

TEST_CASE("optional emplacing & reset", "[optional]") {
  ntf::Optional<int> opt_basic;
  REQUIRE(!opt_basic.has_value());
  opt_basic.emplace(1);
  REQUIRE(opt_basic.has_value());
  REQUIRE(opt_basic.value() == 1);
  opt_basic.reset();
  REQUIRE(!opt_basic.has_value());

  ntf::Optional<void*> opt_ptr;
  REQUIRE(!opt_ptr.has_value());
  opt_ptr.emplace(some_address);
  REQUIRE(opt_ptr.has_value());
  REQUIRE(opt_ptr.value() == some_address);
  opt_ptr.reset();
  REQUIRE(!opt_ptr.has_value());

  ntf::Optional<NullableThing> opt_custom;
  REQUIRE(!opt_custom.has_value());
  opt_custom.emplace(3);
  REQUIRE(opt_custom.has_value());
  REQUIRE(opt_custom.value() == NullableThing{3});
  opt_custom.reset();
  REQUIRE(!opt_custom.has_value());
}

TEST_CASE("optional reference type optimization", "[optional/ref]") {
  static_assert(sizeof(ntf::Optional<int&>) == sizeof(int*));
  static_assert(sizeof(ntf::Optional<const int&>) == sizeof(const int*));

  SECTION("default and nullopt construction") {
    ntf::Optional<int&> opt;
    CHECK(!opt.has_value());
    CHECK(opt.empty());
    CHECK(!opt);

    ntf::Optional<int&> opt_null{ntf::nullopt};
    CHECK(!opt_null.has_value());
  }

  SECTION("binding to lvalue") {
    int x = 10;
    ntf::Optional<int&> opt{x};
    REQUIRE(opt.has_value());
    CHECK(opt.value() == 10);
    CHECK(*opt == 10);

    *opt = 25;
    CHECK(x == 25);
  }

  SECTION("rebinding on assignment") {
    int a = 1;
    int b = 2;
    ntf::Optional<int&> opt{a};
    CHECK(*opt == 1);

    opt = b;
    CHECK(*opt == 2);
    *opt = 42;
    CHECK(b == 42);
    CHECK(a == 1);

    opt = ntf::nullopt;
    CHECK(!opt.has_value());
  }

  SECTION("const reference binding") {
    const int cx = 100;
    ntf::Optional<const int&> copt{cx};
    REQUIRE(copt.has_value());
    CHECK(*copt == 100);

    int mutable_x = 200;
    ntf::Optional<const int&> copt2{mutable_x};
    REQUIRE(copt2.has_value());
    CHECK(*copt2 == 200);
  }

  SECTION("value access and exceptions") {
    ntf::Optional<int&> opt;
    REQUIRE_THROWS_AS(opt.value(), ntf::BadOptionalAccess);

    int fallback = 50;
    CHECK(opt.value_or(fallback) == 50);

    int x = 70;
    opt = x;
    CHECK(opt.value() == 70);
    CHECK(opt.value_or(fallback) == 70);
  }

  SECTION("emplace and reset") {
    int x = 12;
    ntf::Optional<int&> opt;
    opt.emplace(x);
    REQUIRE(opt.has_value());
    CHECK(*opt == 12);

    opt.reset();
    CHECK(!opt.has_value());
  }

  SECTION("arrow operator") {
    struct Target {
      int val = 0;
    };
    Target target{42};
    ntf::Optional<Target&> opt{target};
    REQUIRE(opt.has_value());
    CHECK(opt->val == 42);
    opt->val = 99;
    CHECK(target.val == 99);
  }

  SECTION("monadic operations") {
    int x = 5;
    ntf::Optional<int&> opt{x};

    auto transformed = opt.transform([](int& val) -> int { return val * 3; });
    REQUIRE(transformed.has_value());
    CHECK(*transformed == 15);

    auto and_then_result = opt.and_then([](int& val) -> ntf::Optional<int&> { return {val}; });
    REQUIRE(and_then_result.has_value());
    CHECK(*and_then_result == 5);

    ntf::Optional<int&> empty_opt;
    auto or_else_result = empty_opt.or_else([&]() -> ntf::Optional<int&> { return {x}; });
    REQUIRE(or_else_result.has_value());
    CHECK(*or_else_result == 5);
  }

  SECTION("forbid rvalues") {
    static_assert(!ntf::meta::nullable_type<int&&>);
    static_assert(!std::is_constructible_v<ntf::Optional<int&>, int&&>);
    static_assert(!std::is_constructible_v<ntf::Optional<const int&>, int&&>);
  }
}

TEST_CASE("optional non-trivial lifetime tracking", "[optional/lifetime]") {
  SECTION("construction and destruction") {
    LifetimeThing::reset();
    {
      ntf::Optional<LifetimeThing> opt;
      CHECK(!opt.has_value());
    }
    CHECK(LifetimeThing::constructions == 0);
    CHECK(LifetimeThing::destructions == 0);

    {
      ntf::Optional<LifetimeThing> opt{ntf::in_place, 10};
      CHECK(opt.has_value());
      CHECK(opt->value == 10);
      CHECK(LifetimeThing::constructions == 1);
    }
    CHECK(LifetimeThing::destructions == 1);
    CHECK(LifetimeThing::constructions == LifetimeThing::destructions);
  }

  SECTION("copy and move construction") {
    LifetimeThing::reset();
    {
      ntf::Optional<LifetimeThing> src{ntf::in_place, 42};
      ntf::Optional<LifetimeThing> copy{src};
      CHECK(copy.has_value());
      CHECK(copy->value == 42);
      CHECK(LifetimeThing::copy_constructions == 1);

      ntf::Optional<LifetimeThing> moved{std::move(src)};
      CHECK(moved.has_value());
      CHECK(moved->value == 42);
      CHECK(LifetimeThing::move_constructions == 1);
    }
    int total_created = LifetimeThing::constructions + LifetimeThing::copy_constructions +
                        LifetimeThing::move_constructions;
    CHECK(total_created == LifetimeThing::destructions);
  }

  SECTION("copy assignment operations") {
    LifetimeThing::reset();
    {
      ntf::Optional<LifetimeThing> a{ntf::in_place, 1};
      ntf::Optional<LifetimeThing> b{ntf::in_place, 2};

      a = b;
      CHECK(a->value == 2);
      CHECK(LifetimeThing::copy_assignments == 1);

      ntf::Optional<LifetimeThing> empty;
      a = empty;
      CHECK(!a.has_value());

      empty = b;
      CHECK(empty.has_value());
      CHECK(empty->value == 2);
      CHECK(LifetimeThing::copy_constructions == 1);
    }
    int total_created = LifetimeThing::constructions + LifetimeThing::copy_constructions +
                        LifetimeThing::move_constructions;
    CHECK(total_created == LifetimeThing::destructions);
  }

  SECTION("move assignment operations") {
    LifetimeThing::reset();
    {
      ntf::Optional<LifetimeThing> a{ntf::in_place, 10};
      ntf::Optional<LifetimeThing> b{ntf::in_place, 20};

      a = std::move(b);
      CHECK(a->value == 20);
      CHECK(LifetimeThing::move_assignments == 1);

      ntf::Optional<LifetimeThing> empty;
      a = std::move(empty);
      CHECK(!a.has_value());

      ntf::Optional<LifetimeThing> c{ntf::in_place, 30};
      empty = std::move(c);
      CHECK(empty.has_value());
      CHECK(empty->value == 30);
      CHECK(LifetimeThing::move_constructions == 1);
    }
    int total_created = LifetimeThing::constructions + LifetimeThing::copy_constructions +
                        LifetimeThing::move_constructions;
    CHECK(total_created == LifetimeThing::destructions);
  }

  SECTION("emplace and reset") {
    LifetimeThing::reset();
    {
      ntf::Optional<LifetimeThing> opt;
      opt.emplace(5);
      CHECK(opt.has_value());
      CHECK(opt->value == 5);
      CHECK(LifetimeThing::constructions == 1);

      opt.emplace(15);
      CHECK(opt->value == 15);
      CHECK(LifetimeThing::constructions == 2);
      CHECK(LifetimeThing::destructions == 1);

      opt.reset();
      CHECK(!opt.has_value());
      CHECK(LifetimeThing::destructions == 2);
    }
    CHECK(LifetimeThing::constructions == LifetimeThing::destructions);
  }

  SECTION("reference to non-trivial type") {
    LifetimeThing::reset();
    {
      LifetimeThing original{99};
      CHECK(LifetimeThing::constructions == 1);

      {
        ntf::Optional<LifetimeThing&> opt_ref{original};
        CHECK(opt_ref.has_value());
        CHECK(opt_ref->value == 99);
        CHECK(LifetimeThing::constructions == 1);
        CHECK(LifetimeThing::copy_constructions == 0);
        CHECK(LifetimeThing::move_constructions == 0);
        CHECK(LifetimeThing::destructions == 0);

        opt_ref->value = 100;
        CHECK(original.value == 100);

        opt_ref.reset();
        CHECK(!opt_ref.has_value());
        CHECK(LifetimeThing::destructions == 0);
      }
      CHECK(LifetimeThing::destructions == 0);
      CHECK(original.value == 100);
    }
    CHECK(LifetimeThing::destructions == 1);
    CHECK(LifetimeThing::constructions == LifetimeThing::destructions);
  }
}
