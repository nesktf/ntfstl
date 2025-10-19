#include <catch2/catch_test_macros.hpp>
#include <charconv>
#include <ntfstl/optional.hpp>

namespace {

// TODO: Check non trivial types
struct nullable_thing {
  constexpr nullable_thing(int val = 0) noexcept : value{val} {}

  constexpr bool operator==(const nullable_thing& other) const { return value == other.value; }

  int value;
};

} // namespace

template<>
struct ntf::optional_null<nullable_thing> :
    public std::integral_constant<nullable_thing, nullable_thing{}> {};

// Check optimizations
static_assert(ntf::meta::optimized_optional_type<void*>);
static_assert(sizeof(ntf::optional<void*>) == sizeof(void*));

static_assert(ntf::meta::optimized_optional_type<nullable_thing>);
static_assert(sizeof(ntf::optional<nullable_thing>) == sizeof(nullable_thing));

static void* const some_address = reinterpret_cast<void*>(0xCAFEBABE);

TEST_CASE("optional construction", "[optional]") {
  SECTION("default construction") {
    ntf::optional<int> opt_basic;
    REQUIRE(!opt_basic.has_value());

    ntf::optional<void*> opt_ptr;
    REQUIRE(!opt_ptr.has_value());

    ntf::optional<nullable_thing> opt_custom;
    REQUIRE(!opt_custom.has_value());
  }

  SECTION("tag null construction") {
    ntf::optional<int> opt_basic{ntf::nullopt};
    REQUIRE(!opt_basic.has_value());

    ntf::optional<void*> opt_ptr{ntf::nullopt};
    REQUIRE(!opt_ptr.has_value());

    ntf::optional<nullable_thing> opt_custom{ntf::nullopt};
    REQUIRE(!opt_custom.has_value());
  }

  SECTION("forward construction") {
    ntf::optional<int> opt_basic{1};
    REQUIRE(opt_basic.has_value());
    REQUIRE(*opt_basic == 1);

    ntf::optional<void*> opt_ptr{some_address};
    REQUIRE(opt_ptr.has_value());
    REQUIRE(*opt_ptr == some_address);

    ntf::optional<void*> opt_ptr_null{nullptr};
    REQUIRE(!opt_ptr_null.has_value());

    ntf::optional<nullable_thing> opt_custom{nullable_thing{1}};
    REQUIRE(opt_custom.has_value());
    REQUIRE((*opt_custom).value == 1);

    ntf::optional<nullable_thing> opt_custom_null{nullable_thing{}};
    REQUIRE(!opt_custom_null.has_value());
  }

  SECTION("inplace construction") {
    ntf::optional<int> opt_basic{std::in_place, 1};
    REQUIRE(opt_basic.has_value());
    REQUIRE(*opt_basic == 1);

    ntf::optional<void*> opt_ptr{std::in_place, some_address};
    REQUIRE(opt_ptr.has_value());
    REQUIRE(*opt_ptr == some_address);

    ntf::optional<void*> opt_ptr_null{std::in_place, nullptr};
    REQUIRE(!opt_ptr_null.has_value());

    ntf::optional<nullable_thing> opt_custom{std::in_place, 1};
    REQUIRE(opt_custom.has_value());
    REQUIRE((*opt_custom).value == 1);

    ntf::optional<nullable_thing> opt_custom_null{std::in_place, 0};
    REQUIRE(!opt_custom_null.has_value());
  }
}

TEST_CASE("optional copy operations", "[optional]") {
  SECTION("copy construction null") {
    ntf::optional<int> opt_basic;
    auto opt_basic_copy = opt_basic;
    REQUIRE(!opt_basic_copy.has_value());

    ntf::optional<void*> opt_ptr;
    auto opt_ptr_copy = opt_ptr;
    REQUIRE(!opt_ptr_copy.has_value());

    ntf::optional<nullable_thing> opt_custom;
    auto opt_custom_copy = opt_custom;
    REQUIRE(!opt_custom_copy.has_value());
  }
  SECTION("copy construction normal") {
    ntf::optional<int> opt_basic{1};
    auto opt_basic_copy = opt_basic;
    REQUIRE(opt_basic_copy.has_value());

    ntf::optional<void*> opt_ptr{some_address};
    auto opt_ptr_copy = opt_ptr;
    REQUIRE(opt_ptr_copy.has_value());

    ntf::optional<nullable_thing> opt_custom{nullable_thing{1}};
    auto opt_custom_copy = opt_custom;
    REQUIRE(opt_custom_copy.has_value());
  }
  SECTION("copy assignment null") {
    ntf::optional<int> opt_basic_src_null;
    ntf::optional<int> opt_basic_dst{1};
    opt_basic_dst = opt_basic_src_null;
    REQUIRE(!opt_basic_dst.has_value());
  }
  SECTION("copt assignemnt normal") {
    ntf::optional<int> opt_basic_src_val{2};
    ntf::optional<int> opt_basic_dst{1};
    opt_basic_dst = opt_basic_src_val;
    REQUIRE(opt_basic_dst.has_value());
    REQUIRE(*opt_basic_dst == 2);
  }
}

TEST_CASE("optional move operations", "[optional]") {
  SECTION("move construction null") {
    ntf::optional<int> opt_basic;
    auto opt_basic_copy = std::move(opt_basic);
    REQUIRE(!opt_basic_copy.has_value());
    REQUIRE(!opt_basic.has_value());

    ntf::optional<void*> opt_ptr;
    auto opt_ptr_copy = std::move(opt_ptr);
    REQUIRE(!opt_ptr_copy.has_value());
    REQUIRE(!opt_ptr.has_value());

    ntf::optional<nullable_thing> opt_custom;
    auto opt_custom_copy = std::move(opt_custom);
    REQUIRE(!opt_custom_copy.has_value());
    REQUIRE(!opt_custom.has_value());
  }
  SECTION("move construction normal") {
    ntf::optional<int> opt_basic{1};
    auto opt_basic_copy = std::move(opt_basic);
    REQUIRE(opt_basic_copy.has_value());

    ntf::optional<void*> opt_ptr{some_address};
    auto opt_ptr_copy = std::move(opt_ptr);
    REQUIRE(opt_ptr_copy.has_value());

    ntf::optional<nullable_thing> opt_custom{nullable_thing{1}};
    auto opt_custom_copy = std::move(opt_custom);
    REQUIRE(opt_custom_copy.has_value());
  }
}

static ntf::optional<int> parse_string(std::string_view str) {
  int out;
  auto ret = std::from_chars(str.begin(), str.end(), out);
  if (ret.ptr != str.end()) {
    return {ntf::nullopt};
  }
  return {std::in_place, out};
}

TEST_CASE("optional monadic operations", "[optional]") {
  SECTION("and_then") {
    auto lvalue = parse_string("4");
    auto lvalue_ret = lvalue.and_then([](int value) -> ntf::optional<int> {
      return {std::in_place, value * 3};
    });
    REQUIRE(lvalue_ret.has_value());
    REQUIRE(*lvalue_ret == 12);

    const auto const_lvalue = parse_string("8");
    auto const_lvalue_ret = const_lvalue.and_then([](int value) -> ntf::optional<int> {
      return {std::in_place, value * 4};
    });
    REQUIRE(const_lvalue.has_value());
    REQUIRE(*const_lvalue_ret == 32);

    auto rvalue_ret = parse_string("2").and_then([](int value) -> ntf::optional<int> {
      return {std::in_place, value * 2};
    });
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
    auto lvalue_ret = lvalue.or_else([]() -> ntf::optional<int> { return {std::in_place, 2}; });
    REQUIRE(lvalue_ret.has_value());
    REQUIRE(*lvalue_ret == 2);

    auto rvalue_ret = parse_string("i").or_else([]() -> ntf::optional<int> {
      return {std::in_place, 4};
    });
    REQUIRE(rvalue_ret.has_value());
    REQUIRE(*rvalue_ret == 4);
  }
}

TEST_CASE("optional emplacing & reset", "[optional]") {
  ntf::optional<int> opt_basic;
  REQUIRE(!opt_basic.has_value());
  opt_basic.emplace(1);
  REQUIRE(opt_basic.has_value());
  REQUIRE(opt_basic.value() == 1);
  opt_basic.reset();
  REQUIRE(!opt_basic.has_value());

  ntf::optional<void*> opt_ptr;
  REQUIRE(!opt_ptr.has_value());
  opt_ptr.emplace(some_address);
  REQUIRE(opt_ptr.has_value());
  REQUIRE(opt_ptr.value() == some_address);
  opt_ptr.reset();
  REQUIRE(!opt_ptr.has_value());

  ntf::optional<nullable_thing> opt_custom;
  REQUIRE(!opt_custom.has_value());
  opt_custom.emplace(3);
  REQUIRE(opt_custom.has_value());
  REQUIRE(opt_custom.value() == nullable_thing{3});
  opt_custom.reset();
  REQUIRE(!opt_custom.has_value());
}
