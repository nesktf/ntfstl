#ifndef NTF_ERASURE_HPP_
#define NTF_ERASURE_HPP_

#include <ntf/core.hpp>

namespace ntf::impl {

template<typename T, bool IsConst, bool IsNoexcept, typename Ret, typename... Args>
struct ErasedInvoker {
  static constexpr Ret invoke(void* ptr, Args... args) noexcept(IsNoexcept) {
    if constexpr (meta::is_void_v<Ret>) {
      (*launder(static_cast<T*>(ptr)))(::ntf::forward<Args>(args)...);
    } else {
      return (*launder(static_cast<T*>(ptr)))(::ntf::forward<Args>(args)...);
    }
  }
};

template<typename T, bool IsNoexcept, typename Ret, typename... Args>
struct ErasedInvoker<T, true, IsNoexcept, Ret, Args...> {
  static constexpr Ret invoke(const void* ptr, Args... args) noexcept(IsNoexcept) {
    if constexpr (meta::is_void_v<Ret>) {
      (*launder(static_cast<const T*>(ptr)))(::ntf::forward<Args>(args)...);
    } else {
      return (*launder(static_cast<const T*>(ptr)))(::ntf::forward<Args>(args)...);
    }
  }
};

} // namespace ntf::impl

#endif // NTF_ERASURE_HPP_
