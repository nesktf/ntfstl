#include <catch2/catch_test_macros.hpp>

#include <ntf/stringbuf.hpp>

static constexpr char char_array[] = "test_data";
static constexpr wchar_t wchar_array[] = L"test_data";

TEST_CASE("StringBuf construction", "[StringBuf]") {
  SECTION("Default construction") {
    ntf::StringBuf<32> str;
    static_assert(decltype(str)::BUFFER_SIZE == 32);
    static_assert(std::same_as<decltype(str)::char_type, char>);
    REQUIRE(str.capacity() == 31);
    REQUIRE(str.size() == 0);
    REQUIRE(str.empty());
    REQUIRE(std::strlen(str.c_str()) == 0);
    REQUIRE(*(str.data() + str.size()) == '\0');
    for (size_t i = 0; i < decltype(str)::BUFFER_SIZE; ++i) {
      REQUIRE(*(str.data() + i) == '\0');
    }
  }

  SECTION("Array of char construction") {
    ntf::StringBuf str = char_array;
    static_assert(std::same_as<decltype(str)::char_type, char>);
    static_assert(decltype(str)::BUFFER_SIZE == sizeof(char_array));
    REQUIRE(str.capacity() == sizeof(char_array) - 1);
    REQUIRE(str.size() == sizeof(char_array) - 1);
    REQUIRE(!str.empty());
    REQUIRE(*(str.data() + str.size()) == '\0');

    static constexpr ntf::StringBuf cons_str = char_array;
    static_assert(std::same_as<decltype(cons_str)::char_type, char>);
    static_assert(decltype(cons_str)::BUFFER_SIZE == sizeof(char_array));
    static_assert(cons_str.capacity() == sizeof(char_array) - 1);
    static_assert(cons_str.size() == sizeof(char_array) - 1);
    static_assert(!cons_str.empty());
    static_assert(*(cons_str.data() + cons_str.size()) == '\0'); // operator[] is not constexpr
  }

  SECTION("Array of wchar construction") {
    ntf::StringBuf str = wchar_array;
    static_assert(std::same_as<decltype(str)::char_type, wchar_t>);
    static_assert(decltype(str)::BUFFER_SIZE == sizeof(char_array));
    REQUIRE(str.capacity() == sizeof(char_array) - 1);
    REQUIRE(str.size() == sizeof(char_array) - 1);
    REQUIRE(!str.empty());
    REQUIRE(*(str.data() + str.size()) == '\0');

    static constexpr ntf::StringBuf cons_str = wchar_array;
    static_assert(std::same_as<decltype(cons_str)::char_type, wchar_t>);
    static_assert(decltype(cons_str)::BUFFER_SIZE == sizeof(char_array));
    static_assert(cons_str.capacity() == sizeof(char_array) - 1);
    static_assert(cons_str.size() == sizeof(char_array) - 1);
    static_assert(!cons_str.empty());
    static_assert(*(cons_str.data() + cons_str.size()) == '\0'); // operator[] is not constexpr
  }

  SECTION("C string construction") {
    const char* c_string = char_array;
    const size_t len = std::strlen(c_string);
    ntf::StringBuf<sizeof(char_array)> str{c_string};
    REQUIRE(str.capacity() == len);
    REQUIRE(str.size() == len);
    REQUIRE(!str.empty());
    REQUIRE(*(str.data() + str.size()) == '\0');
    for (size_t i = 0; i < len; ++i) {
      REQUIRE(c_string[i] == str[i]);
    }
  }

  SECTION("Uninitialized construction") {
    ntf::StringBuf<32> str1{ntf::uninitialized, 64};
    REQUIRE(str1.capacity() == 31);
    REQUIRE(str1.size() == 31);
    REQUIRE(!str1.empty());

    ntf::StringBuf<32> str2{ntf::uninitialized, 16};
    REQUIRE(str2.capacity() == 31);
    REQUIRE(str2.size() == 16);
    REQUIRE(!str2.empty());
  }

  SECTION("Truncated construction") {
    ntf::StringBuf<5> str{char_array, sizeof(char_array)};
    REQUIRE(str.capacity() == 4);
    REQUIRE(str.size() == 4);
    REQUIRE(!str.empty());
    REQUIRE(str[0] == 't');
    REQUIRE(str[1] == 'e');
    REQUIRE(str[2] == 's');
    REQUIRE(str[3] == 't');
    REQUIRE(*(str.data() + 4) == '\0');

    static constexpr ntf::StringBuf<5> cons_str{char_array, sizeof(char_array)};
    static_assert(cons_str.capacity() == 4);
    static_assert(cons_str.size() == 4);
    static_assert(!cons_str.empty());
    static_assert(*(cons_str.data() + 0) == 't');
    static_assert(*(cons_str.data() + 1) == 'e');
    static_assert(*(cons_str.data() + 2) == 's');
    static_assert(*(cons_str.data() + 3) == 't');
    static_assert(*(cons_str.data() + 4) == '\0');
  }

  SECTION("Construction from smaller string") {
    ntf::StringBuf<2 * sizeof(char_array)> str = char_array;
    REQUIRE(str.capacity() == 2 * sizeof(char_array) - 1);
    REQUIRE(str.size() == sizeof(char_array) - 1);
    REQUIRE(!str.empty());
    for (size_t i = 0; i < str.size(); ++i) {
      REQUIRE(str[i] == char_array[i]);
    }
    REQUIRE(*(str.data() + str.size()) == '\0');

    // Everything after the string should be zeros
    for (size_t i = str.size(); i < decltype(str)::BUFFER_SIZE; ++i) {
      REQUIRE(*(str.data() + i) == '\0');
    }

    static constexpr ntf::StringBuf<2 * sizeof(char_array)> cons_str = char_array;
    static_assert(cons_str.capacity() == 2 * sizeof(char_array) - 1);
    static_assert(cons_str.size() == sizeof(char_array) - 1);
    static_assert(!cons_str.empty());
    static_assert(*(cons_str.data() + 0) == 't');
    static_assert(*(cons_str.data() + 1) == 'e');
    static_assert(*(cons_str.data() + 2) == 's');
    static_assert(*(cons_str.data() + 3) == 't');
    static_assert(*(cons_str.data() + 4) == '_');
    static_assert(*(cons_str.data() + 5) == 'd');
    static_assert(*(cons_str.data() + 6) == 'a');
    static_assert(*(cons_str.data() + 7) == 't');
    static_assert(*(cons_str.data() + 8) == 'a');
    static_assert(*(cons_str.data() + 9) == '\0');
  }
}

TEST_CASE("StringBuf resize", "[StringBuf]") {
  ntf::StringBuf str = char_array;
  REQUIRE(str.size() == sizeof(char_array) - 1);
  REQUIRE(!str.empty());

  str.resize(4);
  REQUIRE(str.size() == 4);
  REQUIRE(!str.empty());
  REQUIRE(*(str.data() + str.size()) == '\0');

  SECTION("Resize grow zero") {
    str.resize(8);
    REQUIRE(str.size() == 8);
    REQUIRE(!str.empty());
    REQUIRE(*(str.data() + str.size()) == '\0');
    for (size_t i = 4; i < 8; ++i) {
      REQUIRE(*(str.data() + i) == '\0');
    }
  }

  SECTION("Resize grow fill") {
    str.resize(8, 'a');
    REQUIRE(str.size() == 8);
    REQUIRE(!str.empty());
    REQUIRE(*(str.data() + str.size()) == '\0');
    for (size_t i = 4; i < 8; ++i) {
      REQUIRE(*(str.data() + i) == 'a');
    }
  }
}

TEST_CASE("StringBuf checked access", "[StringBuf]") {
  ntf::StringBuf<32> str_empty;
  REQUIRE_THROWS(str_empty.at(64));
  REQUIRE_THROWS(str_empty.at(32));
  REQUIRE_THROWS(str_empty.at(31));

  ntf::StringBuf<32> str = char_array;
  REQUIRE_THROWS(str.at(64));
  REQUIRE_THROWS(str.at(32));
  REQUIRE_NOTHROW(str.at(sizeof(char_array) - 2));
}

TEST_CASE("StringBuf iterator access", "[StringBuf]") {
  ntf::StringBuf str = char_array;

  SECTION("Forward iterator") {
    size_t i = 0;
    for (auto it = str.begin(); it != str.end(); ++it) {
      REQUIRE(*it == char_array[i]);
      ++i;
    }
    i = 0;
    for (auto it = str.cbegin(); it != str.cend(); ++it) {
      REQUIRE(*it == char_array[i]);
      ++i;
    }
    i = 0;
    for (auto& ch : str) {
      REQUIRE(ch == char_array[i]);
      ++i;
    }
    i = 0;
    for (const auto& ch : str) {
      REQUIRE(ch == char_array[i]);
      ++i;
    }
  }

  SECTION("Reverse iterator") {
    size_t i = str.size() - 1;
    for (auto it = str.rbegin(); it != str.rend(); ++it) {
      REQUIRE(*it == char_array[i]);
      --i;
    }
    i = str.size() - 1;
    for (auto it = str.crbegin(); it != str.crend(); ++it) {
      REQUIRE(*it == char_array[i]);
      --i;
    }
  }
}
