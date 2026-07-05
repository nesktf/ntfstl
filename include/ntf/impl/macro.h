#ifndef NTF_MACRO_H_
#define NTF_MACRO_H_

#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#if defined(_MSC_VER)
#define NTF_INLINE   __forceinline
#define NTF_NOINLINE __declspec((noinline))
#elif defined(__GNUC__) || defined(__MINGW32__)
#define NTF_INLINE   inline __attribute__((__always_inline__))
#define NTF_NOINLINE __attribute__((__noinline__))
#elif defined(__clang__)
#define NTF_INLINE   __forceinline__
#define NTF_NOINLINE __noinline__
#else
#define NTF_INLINE inline
#define NTF_NOINLINE
#endif

#ifdef NDEBUG
#define NTF_ABORT() ::abort()
#else
#ifdef __clang__
#define NTF_ABORT() __builtin_debugtrap()
#else
#define NTF_ABORT() __builtin_trap()
#endif
#endif

#define NTF_NOOP         (void)0
#define NTF_UNUSED(expr) (void)(true ? NTF_NOOP : ((void)(expr)))

#define NTF_APPLY_VA_ARGS_(_M, _args) _M _args
#define NTF_APPLY_VA_ARGS(_M, ...)    NTF_APPLY_VA_ARGS_(_M, (__VA_ARGS__))

#define NTF_NARG_(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, \
                  _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31, _32, _33, \
                  ...)                                                                            \
  _33

#define NTF_EMPTY_MACRO
#define NTF_NARG(...)                                                                             \
  NTF_APPLY_VA_ARGS(NTF_NARG_, NTF_EMPTY_MACRO __VA_OPT__(, ) __VA_ARGS__, 32, 31, 30, 29, 28,    \
                    27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, \
                    7, 6, 5, 4, 3, 2, 1, 0, NTF_EMPTY_MACRO)

#define NTF_JOIN__(_lhs, _rhs) _lhs##_rhs
#define NTF_JOIN_(_lhs, _rhs)  NTF_JOIN__(_lhs, _rhs)
#define NTF_JOIN(_lhs, _rhs)   NTF_JOIN_(_lhs, _rhs)

#define NTF_STRINGIFY_(a) #a
#define NTF_STRINGIFY(a)  NTF_STRINGIFY_(a)

#define NTF_LIKELY(_arg)   __builtin_expect(!!(_arg), !0)
#define NTF_UNLIKELY(_arg) __builtin_expect(!!(_arg), 0)
#define NTF_UNREACHABLE()  __builtin_unreachable()

#define NTF_NORETURN  [[noreturn]]
#define NTF_NODISCARD [[nodiscard]]

#define NTF_PANIC_1(_msg) ::ntf__panic_handler(__FILE__, __PRETTY_FUNCTION__, __LINE__, _msg);
#define NTF_PANIC_0()     NTF_PANIC_1("It's over");
#define NTF_PANIC(...)    NTF_APPLY_VA_ARGS(NTF_JOIN(NTF_PANIC_, NTF_NARG(__VA_ARGS__)), __VA_ARGS__)

#define NTF_TODO_1(_msg)                                                            \
  do {                                                                              \
    if (!::ntf::is_constant_evaluated()) {                                          \
      ::ntf__panic_handler(__FILE__, __PRETTY_FUNCTION__, __LINE__, "TODO: " _msg); \
      NTF_UNREACHABLE();                                                            \
    }                                                                               \
  } while (0)
#define NTF_TODO_0()                                                         \
  do {                                                                       \
    if (!::ntf::is_constant_evaluated()) {                                   \
      ::ntf__panic_handler(__FILE__, __PRETTY_FUNCTION__, __LINE__, "TODO"); \
      NTF_UNREACHABLE();                                                     \
    }                                                                        \
  } while (0)
#define NTF_TODO(...) NTF_APPLY_VA_ARGS(NTF_JOIN(NTF_TODO_, NTF_NARG(__VA_ARGS__)), __VA_ARGS__)

#define NTF_ASSERT_2(_cond, _msg)                                                  \
  do {                                                                             \
    if (!::ntf::is_constant_evaluated() && NTF_UNLIKELY(!(_cond))) {               \
      ::ntf__panic_handler(__FILE__, __PRETTY_FUNCTION__, __LINE__,                \
                           "Assertion failure (" NTF_STRINGIFY(_cond) "): " _msg); \
      NTF_UNREACHABLE();                                                           \
    }                                                                              \
  } while (0)
#define NTF_ASSERT_1(_cond)                                                 \
  do {                                                                      \
    if (!::ntf::is_constant_evaluated() && NTF_UNLIKELY(!(_cond))) {        \
      ::ntf__panic_handler(__FILE__, __PRETTY_FUNCTION__, __LINE__,         \
                           "Assertion failure (" NTF_STRINGIFY(_cond) ")"); \
      NTF_UNREACHABLE();                                                    \
    }                                                                       \
  } while (0)
#define NTF_ASSERT(...) \
  NTF_APPLY_VA_ARGS(NTF_JOIN(NTF_ASSERT_, NTF_NARG(__VA_ARGS__)), __VA_ARGS__)

#ifdef __cpp_exceptions
#define NTF_THROW(_obj) throw _obj
#else
#define NTF_THROW(_obj) NTF_PANIC("Thrown exception " NTF_STRINGIFY(_obj))
#endif
#define NTF_THROW_IF(_cond, _obj) \
  do {                            \
    if (_cond) {                  \
      NTF_THROW(_obj);            \
    }                             \
  } while (0)

#ifdef NTF_KEYWORD_FN
#define fn auto
#endif

#ifdef NTF_KEYWORD_LET
#define let const auto
#endif

#define NTF_DEFINE_HANDLE(_typename) typedef struct _typename##_T* _typename

#define NTF_NO_MOVE(_typename)     \
  _typename(_typename&&) = delete; \
  _typename& operator=(_typename&&) = delete

#define NTF_DO_MOVE(_typename)     \
  _typename(_typename&&) noexcept; \
  _typename& operator=(_typename&&) noexcept

#define NTF_NO_COPY(_typename)          \
  _typename(const _typename&) = delete; \
  _typename& operator=(const _typename&) = delete

#ifdef __cplusplus
extern "C" {
#endif

NTF_NORETURN void ntf__panic_handler(const char* file, const char* func, int line,
                                     const char* msg);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // NTF_MACRO_H_
