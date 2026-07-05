#ifndef NTF_FUNCTION_HPP_
#define NTF_FUNCTION_HPP_

#include <ntf/core.hpp>

#include <memory>

namespace ntf {

namespace impl {

template<typename T, bool IsConst, bool IsNoexcept, typename Ret, typename... Args>
struct ErasedInvoker {
  static constexpr Ret invoke(void* ptr, Args... args) noexcept(IsNoexcept) {
    if constexpr (std::is_void_v<Ret>) {
      (*std::launder(static_cast<T*>(ptr)))(std::forward<Args>(args)...);
    } else {
      return (*std::launder(static_cast<T*>(ptr)))(std::forward<Args>(args)...);
    }
  }
};

template<typename T, bool IsNoexcept, typename Ret, typename... Args>
struct ErasedInvoker<T, true, IsNoexcept, Ret, Args...> {
  static constexpr Ret invoke(const void* ptr, Args... args) noexcept(IsNoexcept) {
    if constexpr (std::is_void_v<Ret>) {
      (*std::launder(static_cast<const T*>(ptr)))(std::forward<Args>(args)...);
    } else {
      return (*std::launder(static_cast<const T*>(ptr)))(std::forward<Args>(args)...);
    }
  }
};

} // namespace impl

// Same as C++23's std::bind_front, but supporting compile time bound values
template<auto Func, auto... Binds, typename... Params>
constexpr auto bind_front(Params&&... params) {
  // TODO: Forward captured params instead of copying
  if constexpr (sizeof...(params) == 0) {
    return []<typename... InnerParam>(InnerParam&&... ps) {
      return Func(Binds..., std::forward<InnerParam>(ps)...);
    };
  } else {
    return
      [... params = std::forward<Params>(params)]<typename... InnerParam>(InnerParam&&... ps) {
        return Func(Binds..., params..., std::forward<InnerParam>(ps)...);
      };
  }
}

// Same as C++23's std::bind_back, but supporting compile time bound values
template<auto Func, auto... Binds, typename... Params>
constexpr auto bind_back(Params&&... params) {
  // TODO: Forward captured params instead of copying
  if constexpr (sizeof...(params) == 0) {
    return []<typename... InnerParam>(InnerParam&&... ps) {
      return Func(std::forward<InnerParam>(ps)..., Binds...);
    };

  } else {
    return
      [... params = std::forward<Params>(params)]<typename... InnerParam>(InnerParam&&... ps) {
        return Func(std::forward<InnerParam>(ps)..., params..., Binds...);
      };
  }
}

template<typename... Fs>
struct OverloadFn : Fs... {
  using Fs::operator()...;
};

template<typename... Fs>
OverloadFn(Fs...) -> OverloadFn<Fs...>;

} // namespace ntf

#endif // NTF_FUNCTION_HPP_
