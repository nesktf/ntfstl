#include <catch2/catch_test_macros.hpp>
#include <ntf/span.hpp>

TEST_CASE("Span construction", "[Span]") {
  SECTION("Default construction") {
    static constexpr ntf::Span<int> const_span;
    static_assert(const_span.size() == 0);
    static_assert(const_span.data() == nullptr);
    static_assert(const_span.empty());
  }
}
