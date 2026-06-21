#ifndef NTF_CORE_HPP_
#define NTF_CORE_HPP_

#include <ntf/macro.hpp>

#include <utility>

enum NTF_PNEW_T {
  NTF_PNEW_TAG,
};

constexpr void* operator new(size_t, void* ptr, NTF_PNEW_T) {
  return ptr;
}

#define NTF_PNEW(_ptr) ::new (_ptr, NTF_PNEW_TAG)

namespace ntf {

namespace numdefs {

using size_t = ::size_t;
using ptrdiff_t = ::ptrdiff_t;
using uintptr_t = ::uintptr_t;

using u8 = ::uint8_t;
using u16 = ::uint16_t;
using u32 = ::uint32_t;
using u64 = ::uint64_t;
using i8 = ::int8_t;
using i16 = ::int16_t;
using i32 = ::int32_t;
using i64 = ::int64_t;

using f32 = float;
static_assert(sizeof(f32) == 4);

using f64 = double;
static_assert(sizeof(f64) == 8);

} // namespace numdefs

using namespace numdefs;

struct uninitialized_t {};

constexpr inline uninitialized_t uninitialized;

template<typename... Fs>
struct OverloadFn : Fs... {
  using Fs::operator()...;
};

template<typename... Fs>
OverloadFn(Fs...) -> OverloadFn<Fs...>;

template<typename F>
class DeferFn {
public:
  template<typename Func>
  constexpr DeferFn(Func&& func) : _func(std::forward<Func>(func)), _engaged(true) {}

  constexpr ~DeferFn() noexcept {
    if (_engaged) {
      invoke();
    }
  }

  NTF_NO_MOVE(DeferFn);
  NTF_NO_COPY(DeferFn);

public:
  void invoke() noexcept { _func(); }

  void disengage() noexcept { _engaged = false; }

private:
  F _func;
  bool _engaged;
};

} // namespace ntf

#endif // NTF_CORE_HPP_
