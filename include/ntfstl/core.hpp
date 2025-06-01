#pragma once

#define NTF_ENABLE 1
#define NTF_DISABLE 0

#define NTF_NOOP (void)0
#define NTF_EMPTY_MACRO

#define NTF_FILE __FILE__
#define NTF_LINE __LINE__
#define NTF_FUNC __PRETTY_FUNCTION__

#define NTF_UNUSED(expr) (void)(true ? NTF_NOOP : ((void)(expr)))

#define NTF_DO_PRAGMA(X) _Pragma(#X)
#define NTF_DISABLE_WARN_PUSH \
  NTF_DO_PRAGMA(GCC diagnostic push)
#define NTF_DISABLE_WARN(_warn) \
  NTF_DO_PRAGMA(GCC diagnostic ignored #_warn)
#define NTF_DISABLE_WARN_POP \
  NTF_DO_PRAGMA(GCC diagnostic pop)

#define NTF_APPLY_VA_ARGS(M, ...) NTF_APPLY_VA_ARGS_(M, (__VA_ARGS__))
#define NTF_APPLY_VA_ARGS_(M, args) M args

#define NTF_JOIN(lhs, rhs) NTF_JOIN_(lhs, rhs)
#define NTF_JOIN_(lhs, rhs) NTF_JOIN__(lhs, rhs)
#define NTF_JOIN__(lhs, rhs) lhs##rhs

#define NTF_NARG_(_0, _1, _2, _3, _4, _5, _6, _7, _8, \
                  _9, _10, _11, _12, _13, _14, _15, _16, \
                  _17, _18, _19, _20, _21, _22, _23, _24, \
                  _25, _26, _27, _28, _29, _30, _31, _32, _33, ...) _33

#define NTF_NARG(...) NTF_APPLY_VA_ARGS(NTF_NARG_, NTF_EMPTY_MACRO, ##__VA_ARGS__, \
                                        32, 31, 30, 29, 28, 27, 26, 25, \
                                        24, 23, 22, 21, 20, 19, 18, 17, \
                                        16, 15, 14, 13, 12, 11, 10, 9, \
                                        8, 7, 6, 5, 4, 3, 2, 1, 0, NTF_EMPTY_MACRO)

#define NTF_FORCE_INLINE inline __attribute__((always_inline))
#define NTF_CONSTEXPR constexpr

#define NTF_LIKELY(arg) __builtin_expect(!!(arg), !0)
#define NTF_UNLIKELY(arg) __builtin_expect(!!(arg), 0)

#define NTF_STRINGIFY_(a) #a
#define NTF_STRINGIFY(a) NTF_STRINGIFY_(a)

#if defined(NDEBUG)
#define NTF_ABORT() ::std::abort()
#elif defined(__clang__)
#define NTF_ABORT() __builtin_debugtrap()
#else
#define NTF_ABORT() __builtin_trap()
#endif

#ifndef NTF_ASSERT
#define NTF_ASSERT_NOEXCEPT true
#define NTF_BUILTIN_ASSERT NTF_ENABLE
#ifdef NDEBUG
#define NTF_ASSERT(...) NTF_NOOP
#else

#include <type_traits>
#include <fmt/format.h>

namespace ntf::impl {

[[noreturn]] void assert_failure(const char* cond, const char* func, const char* file,
                                 int line, const char* msg);

template<typename... Args>
void assert_failure_fmt(const char* cond, const char* func, const char* file, int line,
                        fmt::format_string<Args...> fmt, Args&&... args) {
  std::string fmt_str = fmt::format(fmt, std::forward<Args>(args)...);
  ::ntf::impl::assert_failure(cond, func, file, line, fmt_str.c_str());
}

} // namespace ntf::impl

#define NTF_ASSERT_1(cond, ...) \
  NTF_ASSERT_2(cond, nullptr)
#define NTF_ASSERT_2(cond, msg, ...) \
  if (!std::is_constant_evaluated() && !NTF_UNLIKELY(cond)) { \
    ::ntf::impl::assert_failure(#cond, NTF_FUNC, NTF_FILE, NTF_LINE, msg); \
  }
#define NTF_ASSERT_3(cond, msg, ...) \
  if (!std::is_constant_evaluated() && !NTF_UNLIKELY(cond)) { \
    ::ntf::impl::assert_failure_fmt(#cond, NTF_FUNC, NTF_FILE, NTF_LINE, \
                                    msg __VA_OPT__(,) __VA_ARGS__); \
  }

#define NTF_ASSERT_(...) \
  NTF_APPLY_VA_ARGS(NTF_JOIN(NTF_ASSERT_, NTF_NARG(__VA_ARGS__)), __VA_ARGS__)

#define NTF_ASSERT(...) NTF_ASSERT_(__VA_ARGS__)
#endif
#else
#define NTF_BUILTIN_ASSERT NTF_DISABLE
#endif

#if !NTF_BUILTIN_ASSERT && !defined(NTF_ASSERT_NOEXCEPT)
#warning Custom assertion noexcept not defined
#define NTF_ASSERT_NOEXCEPT true
#endif

#define NTF_UNREACHABLE() \
do { \
  NTF_ASSERT(false, "Triggered unreachable code!!!!!");\
  __builtin_unreachable();\
} while(0)


#if __has_builtin(__builtin_assume)
#define NTF_ASSUME(x) __builtin_assume(x)
#endif

#ifndef NTF_ASSUME
#define NTF_ASSUME(x) do { if(!NTF_UNLIKELY(x)) { NTF_UNREACHABLE(); } } while(0)
#endif

#if defined(NTF_ENABLE_EXCEPTIONS) && NTF_ENABLE_EXCEPTIONS
#define NTF_THROW(_type, ...) \
  throw _type{__VA_ARGS__}
#define NTF_NOEXCEPT false
#define NTF_RETHROW() throw
#else
#define NTF_THROW(_type, ...) \
  NTF_ASSERT(false, "thrown " NTF_STRINGIFY(_type) \
              __VA_OPT__("with args" NTF_STRINGIFY(__VA_ARGS__)))
#define NTF_NOEXCEPT NTF_ASSERT_NOEXCEPT
#define NTF_RETHROW() \
  NTF_ASSERT(false, "caught unknown exception")
#endif
#define NTF_THROW_IF(_cond, _type, ...) \
  if (_cond) { NTF_THROW(_type, __VA_ARGS__); }

// RAII declarations/definitions
#define NTF_DECLARE_MOVE_COPY(__type) \
  ~__type() noexcept; \
  __type(__type&&) noexcept; \
  __type(const __type&) noexcept; \
  __type& operator=(__type&&) noexcept; \
  __type& operator=(const __type&) noexcept

#define NTF_DECLARE_MOVE_ONLY(__type) \
  ~__type() noexcept; \
  __type(__type&&) noexcept; \
  __type(const __type&) = delete; \
  __type& operator=(__type&&) noexcept; \
  __type& operator=(const __type&) = delete

#define NTF_DECLARE_COPY_ONLY(__type) \
  ~__type() noexcept; \
  __type(__type&&) = delete; \
  __type(const __type&) noexcept; \
  __type& operator=(__type&&) = delete; \
  __type& operator=(const __type&) noexcept

#define NTF_DECLARE_NO_MOVE_NO_COPY(__type) \
  ~__type() noexcept; \
  __type(__type&&) = delete; \
  __type(const __type&) = delete; \
  __type& operator=(__type&&) = delete; \
  __type& operator=(const __type&) = delete

#define NTF_DISABLE_MOVE(__type) \
  __type(__type&&) = delete; \
  __type(const __type&) = default; \
  __type& operator=(__type&&) = delete; \
  __type& operator=(const __type&) = default

#define NTF_DISABLE_COPY(__type) \
  __type(__type&&) = default; \
  __type(const __type&) = delete; \
  __type& operator=(__type&&) = default; \
  __type& operator=(const __type&) = delete

#define NTF_DISABLE_MOVE_COPY(__type) \
  __type(__type&&) = delete; \
  __type(const __type&) = delete; \
  __type& operator=(__type&&) = delete; \
  __type& operator=(const __type&) = delete

#define NTF_DISABLE_NOTHING(__type) \
  __type(__type&&) = default; \
  __type(const __type&) = default; \
  __type& operator=(__type&&) = default; \
  __type& operator=(const __type&) = default

#define NTF_DEFINE_ENUM_CLASS_FLAG_OPS(__type) \
  constexpr inline __type operator|(__type a, __type b) { \
    return static_cast<__type>( \
      static_cast<std::underlying_type_t<__type>>(a) | \
      static_cast<std::underlying_type_t<__type>>(b)); \
  } \
  constexpr inline __type operator&(__type a, __type b) { \
    return static_cast<__type>( \
      static_cast<std::underlying_type_t<__type>>(a) & \
      static_cast<std::underlying_type_t<__type>>(b)); \
  } \
  constexpr inline __type operator^(__type a, __type b) { \
    return static_cast<__type>( \
      static_cast<std::underlying_type_t<__type>>(a) ^ \
      static_cast<std::underlying_type_t<__type>>(b)); \
  } \
  constexpr inline __type operator~(__type a) { \
    return static_cast<__type>( \
      ~static_cast<std::underlying_type_t<__type>>(a)); \
  } \
  constexpr inline __type& operator|=(__type& a, __type b) { \
    return a = static_cast<__type>( \
      static_cast<std::underlying_type_t<__type>>(a) | \
      static_cast<std::underlying_type_t<__type>>(b)); \
  } \
  constexpr inline __type& operator&=(__type& a, __type b) { \
    return a = static_cast<__type>( \
      static_cast<std::underlying_type_t<__type>>(a) & \
      static_cast<std::underlying_type_t<__type>>(b)); \
  } \
  constexpr inline __type& operator^=(__type& a, __type b) { \
    return a = static_cast<__type>( \
      static_cast<std::underlying_type_t<__type>>(a) ^ \
      static_cast<std::underlying_type_t<__type>>(b)); \
  } \
  constexpr inline bool operator+(__type a) { \
    return static_cast<std::underlying_type_t<__type>>(a); \
  }
