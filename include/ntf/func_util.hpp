#ifndef NTF_FUNC_UTIL_HPP_
#define NTF_FUNC_UTIL_HPP_

#include <ntf/impl/concepts.hpp>

namespace ntf {

template<typename... Fs>
struct OverloadFn : Fs... {
  using Fs::operator()...;
};

template<typename... Fs>
OverloadFn(Fs...) -> OverloadFn<Fs...>;

template<typename Fn>
class DeferFn {
public:
  template<typename Func>
  constexpr DeferFn(Func&& func) : _func(::ntf::forward<Func>(func)), _engaged(true) {}

  constexpr ~DeferFn() noexcept {
    if (_engaged) {
      invoke();
    }
  }

  NTF_NO_MOVE(DeferFn);
  NTF_NO_COPY(DeferFn);

public:
  constexpr void invoke() noexcept {
    // static_assert(meta::nothrow_invocable<Fn>, "Fn has to be nothrow invocable");
    _func();
  }

  constexpr void disengage() noexcept { _engaged = false; }

private:
  Fn _func;
  bool _engaged;
};

template<typename Fn>
DeferFn(Fn) -> DeferFn<Fn>;

// Same as C++23's std::bind_front, but supporting compile time bound values
template<auto Func, auto... Binds, typename... Params>
constexpr auto bind_front(Params&&... params) {
  if constexpr (sizeof...(params) == 0) {
    return []<typename... InnerParam>(InnerParam&&... ps) {
      return Func(Binds..., ::ntf::forward<InnerParam>(ps)...);
    };
  } else {
    return
      [... params = ::ntf::forward<Params>(params)]<typename... InnerParam>(InnerParam&&... ps) {
        return Func(Binds..., params..., ::ntf::forward<InnerParam>(ps)...);
      };
  }
}

// Same as C++23's std::bind_back, but supporting compile time bound values
template<auto Func, auto... Binds, typename... Params>
constexpr auto bind_back(Params&&... params) {
  if constexpr (sizeof...(params) == 0) {
    return []<typename... InnerParam>(InnerParam&&... ps) {
      return Func(::ntf::forward<InnerParam>(ps)..., Binds...);
    };

  } else {
    return
      [... params = ::ntf::forward<Params>(params)]<typename... InnerParam>(InnerParam&&... ps) {
        return Func(::ntf::forward<InnerParam>(ps)..., params..., Binds...);
      };
  }
}

} // namespace ntf

#endif // NTF_FUNC_UTIL_HPP_
