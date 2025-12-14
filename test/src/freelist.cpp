#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>
#include <ntfstl/freelist.hpp>

using namespace ntf::numdefs;

// TODO: test freelist::sort()
// TODO: test move semantics

namespace {

struct lifetime_tracker {
  static inline u32 alive_count = 0u;
  u32 value;

  lifetime_tracker(int v) : value(v) { alive_count++; }

  lifetime_tracker(const lifetime_tracker& other) : value(other.value) { alive_count++; }

  lifetime_tracker(lifetime_tracker&& other) noexcept : value(other.value) { alive_count++; }

  ~lifetime_tracker() {
    fmt::print("destroyed {}\n", alive_count);
    alive_count--;
  }

  lifetime_tracker& operator=(const lifetime_tracker&) = default;
  lifetime_tracker& operator=(lifetime_tracker&&) = default;

  bool operator==(const lifetime_tracker& other) const { return value == other.value; }

  bool operator<(const lifetime_tracker& other) const { return value < other.value; }
};

} // namespace

TEST_CASE("freelist_handle basic operations", "[freelist_handle]") {
  SECTION("default constructor builds empty handle") {
    ntf::freelist_handle handle;
    REQUIRE(handle.empty());
    REQUIRE(handle.index() == ntf::freelist_handle::NULL_INDEX);
    REQUIRE(handle.version() == 0u);
    REQUIRE_FALSE(static_cast<bool>(handle));
  }
  SECTION("constructor with values") {
    ntf::freelist_handle handle{10u, 5u};
    REQUIRE_FALSE(handle.empty());
    REQUIRE(handle.index() == 10u);
    REQUIRE(handle.version() == 5u);
    REQUIRE(static_cast<bool>(handle));
  }
  SECTION("operator== & operator!=") {
    ntf::freelist_handle h1{1u, 1u};
    ntf::freelist_handle h2{1u, 1u};
    ntf::freelist_handle h3{1u, 2u};
    ntf::freelist_handle h4{2u, 1u};
    REQUIRE(h1 == h2);
    REQUIRE(h1 != h3);
    REQUIRE(h1 != h4);
  }
  SECTION("as_u64 & from_u64") {
    ntf::freelist_handle handle;
    const auto null_u64 = static_cast<u64>(ntf::freelist_handle::NULL_INDEX);
    REQUIRE(handle.as_u64() == null_u64);

    auto null_u64_handle = ntf::freelist_handle::from_u64(null_u64);
    REQUIRE(null_u64_handle.index() == ntf::freelist_handle::NULL_INDEX);
    REQUIRE(null_u64_handle.version() == 0u);
    REQUIRE(null_u64_handle.as_u64() == null_u64);

    const auto other_u64 = (static_cast<u64>(16u) << 32) | null_u64;
    auto other_u64_handle = ntf::freelist_handle::from_u64(other_u64);
    REQUIRE(other_u64_handle.index() == ntf::freelist_handle::NULL_INDEX);
    REQUIRE(other_u64_handle.version() == 16u);
    REQUIRE(other_u64_handle.as_u64() == other_u64);
  }
}

TEST_CASE("dynamic freelist construction", "[freelist]") {
  SECTION("empty freelist") {
    ntf::freelist<int> list;
    REQUIRE(list.empty());
    REQUIRE(list.size() == 0u);
    REQUIRE(list.begin() == list.end());
    REQUIRE(list.cbegin() == list.cend());
  }
  SECTION("initializer_list freelist and range for loop") {
    ntf::freelist<int> list{1, 2, 3, 4};
    REQUIRE_FALSE(list.empty());
    REQUIRE(list.size() == 4u);
    REQUIRE_FALSE(list.begin() == list.end());
    REQUIRE_FALSE(list.cbegin() == list.cend());

    std::vector<int> vals;
    for (const auto& [elem, _] : list) {
      vals.emplace_back(elem);
    }

    REQUIRE_THAT(vals, Catch::Matchers::VectorContains(1));
    REQUIRE_THAT(vals, Catch::Matchers::VectorContains(2));
    REQUIRE_THAT(vals, Catch::Matchers::VectorContains(3));
    REQUIRE_THAT(vals, Catch::Matchers::VectorContains(4));
    REQUIRE(vals.size() == 4u);
  }
}

TEST_CASE("dynamic freelist emplace and access", "[freelist]") {
  SECTION("emplace returns valid handles") {
    ntf::freelist<int> list;
    auto h1 = list.emplace(100);
    REQUIRE(h1.version() == 0u);
    REQUIRE(h1.index() == 0u);

    auto h2 = list.emplace(200);
    REQUIRE(h2.version() == 0u);
    REQUIRE(h2.index() == 1u);

    REQUIRE(list.size() == 2u);
    REQUIRE(list.is_valid(h1));
    REQUIRE(list.is_valid(h2));

    REQUIRE(list.at(h1) == 100);
    REQUIRE_NOTHROW(list.at(h2) == 200);
  }

  SECTION("freelist reuses slots") {
    ntf::freelist<int> list;
    auto h1 = list.emplace(10);

    list.remove(h1);
    REQUIRE(list.empty());
    REQUIRE_FALSE(list.is_valid(h1));

    auto h2 = list.emplace(20);
    REQUIRE(h2.index() == h1.index());
    REQUIRE_FALSE(h2.version() == h1.version());
  }
}

TEST_CASE("dynamic freelist manages lifetime properly", "[freelist]") {
  lifetime_tracker::alive_count = 0u;
  {
    ntf::freelist<lifetime_tracker> list;
    auto h1 = list.emplace(0);
    REQUIRE(h1.version() == 0u);
    REQUIRE(h1.index() == 0u);

    auto h2 = list.emplace(10);
    REQUIRE(h2.version() == 0u);
    REQUIRE(h2.index() == 1u);

    auto h3 = list.emplace(20);
    REQUIRE(h3.version() == 0u);
    REQUIRE(h3.index() == 2u);

    auto h4 = list.emplace(30);
    REQUIRE(h4.version() == 0u);
    REQUIRE(h4.index() == 3u);

    REQUIRE(list.size() == 4u);
    REQUIRE(lifetime_tracker::alive_count == 4u);

    SECTION("remove destroys the object") {
      list.remove(h1);
      REQUIRE(list.size() == 3u);
      REQUIRE(lifetime_tracker::alive_count == 3u);
      REQUIRE_FALSE(list.is_valid(h1));
    }

    SECTION("remove_where properly removes things") {
      list.remove_where([](const lifetime_tracker& item) { return item.value > 15; });
      REQUIRE(list.size() == 1u);
      REQUIRE_NOTHROW(list.at(h2).value == 10);
      REQUIRE(lifetime_tracker::alive_count == 1u);
    }

    SECTION("clear() destroys all items") {
      list.clear();
      REQUIRE(list.empty());
      REQUIRE(lifetime_tracker::alive_count == 0u);
    }

    auto h5 = list.emplace(0);
    REQUIRE(list.size() == 1u);
    REQUIRE(lifetime_tracker::alive_count == 1u);
  }
  REQUIRE(lifetime_tracker::alive_count == 0u);
}

TEST_CASE("static freelist capacity limit", "[freelist]") {
  ntf::freelist<int, 3u> list;
  REQUIRE(list.capacity() == 3u);

  auto h1 = list.emplace(1);
  auto h2 = list.emplace(2);
  auto h3 = list.emplace(3);

  REQUIRE(h1.has_value());
  REQUIRE(h2.has_value());
  REQUIRE(h3.has_value());
  REQUIRE(list.size() == 3u);

  SECTION("list can't emplace over the limit") {
    auto h4 = list.emplace(4);
    REQUIRE_FALSE(h4.has_value());
    REQUIRE(list.size() == 3u);
  }

  SECTION("removing allows inserting more elements") {
    list.remove(*h2);
    REQUIRE(list.size() == 2u);

    auto h4 = list.emplace(4);
    REQUIRE(h4.has_value());
    REQUIRE(h4->index() == h2->index());
    REQUIRE(list.size() == 3u);
    REQUIRE_NOTHROW(list.at(*h4) == 42);
  }
}

TEST_CASE("freelist edge cases", "[freelist]") {
  ntf::freelist<int> empty_list;
  ntf::freelist_handle invalid;
  auto removed = empty_list.emplace(100);
  empty_list.remove(removed);

  SECTION("access invalid handle") {
    REQUIRE_THROWS(empty_list.at(removed));
    REQUIRE(empty_list.at_opt(removed) == nullptr);
    REQUIRE_THROWS(empty_list.at(invalid));
    REQUIRE(empty_list.at_opt(invalid) == nullptr);
  }

  SECTION("deleting an invalid handle is a noop") {
    REQUIRE_NOTHROW(empty_list.remove(removed));
    REQUIRE_NOTHROW(empty_list.remove(invalid));
  }

  SECTION("static list truncates initializer list") {
    ntf::freelist<int, 2u> list{1, 2, 3, 4};
    REQUIRE(list.size() == 2u);
    REQUIRE(list.capacity() == 2u);

    std::vector<int> vals;
    for (const auto& [elem, _] : list) {
      vals.emplace_back(elem);
    }
    REQUIRE_THAT(vals, Catch::Matchers::VectorContains(1));
    REQUIRE_THAT(vals, Catch::Matchers::VectorContains(2));
    REQUIRE(vals.size() == 2u);
  }

  SECTION("use of freelist::find()") {
    ntf::freelist<int> list{1, 2, 3, 4};

    auto h1 = list.find([](int item) { return item == 1; });
    REQUIRE(h1.has_value());
    REQUIRE_FALSE(h1->empty());
    REQUIRE(h1->index() == 0u);
    REQUIRE_NOTHROW(list.at(*h1) == 1);

    auto h2 = list.find([](int item) { return item == 5; });
    REQUIRE_FALSE(h2.has_value());
  }
}
