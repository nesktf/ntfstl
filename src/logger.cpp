#include <ntfstl/logger.hpp>

#if defined(NTF_BUILTIN_ASSERT) && NTF_BUILTIN_ASSERT
namespace ntf::impl {

[[noreturn]] void assert_failure(const char* cond, const char* func, const char* file,
                                        int line, const char* msg) {
  if (msg) {
    ::ntf::logger::fatal("{}:{}: {} assertion '{}' failed: {}", file, line, func, cond, msg);
  } else {
    ::ntf::logger::fatal("{}:{}: {} assertion '{}' failed", file, line, func, cond, msg);
  }
  NTF_ABORT();
  __builtin_unreachable();
}

} // namespace ntf::impl
#endif
