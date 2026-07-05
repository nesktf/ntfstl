#ifndef NTF_FUNC_HPP_
#define NTF_FUNC_HPP_

#include <ntf/impl/concepts.hpp>
#include <ntf/impl/erasure.hpp>
#include <ntf/memory.hpp>

namespace ntf {

// Type erased function class for trivially copyable and destructible callables
// Nice for lambdas with referece captures
template<typename Signature, size_t MaxSize, size_t MaxAlign>
class TrivFn;

// const specialization
template<size_t MaxSize, size_t MaxAlign, bool IsNoexcept, typename Ret, typename... Args>
class TrivFn<Ret(Args...) const noexcept(IsNoexcept), MaxSize, MaxAlign> {
private:
  template<typename T>
  static constexpr bool is_valid_func =
    meta::invocable_with_r<T, Ret, Args...> && !meta::is_same_v<TrivFn, T> &&
    meta::trivially_copy_constructible<T> && meta::trivially_destructible<T>;

public:
  TrivFn(Ret (*func_ptr)(Args...) noexcept(IsNoexcept)) :
      _func_ptr_invoke(func_ptr), _is_object(false) {
    NTF_THROW_IF(!func_ptr, MsgException("Assigning null function pointer to TrivFn"));
  }

  template<typename Fn>
  requires(is_valid_func<meta::remove_cvref_t<Fn>>)
  TrivFn(Fn&& callable) noexcept :
      _func_obj_invoke(
        &impl::ErasedInvoker<meta::remove_cvref_t<Fn>, true, IsNoexcept, Ret, Args...>::invoke),
      _is_object(true) {
    NTF_PNEW(_buffer) meta::remove_cvref_t<Fn>(::ntf::forward<Fn>(callable));
  }

  template<typename Fn, typename... Args2>
  requires(is_valid_func<Fn> && meta::constructible_from<Fn, Args2...>)
  explicit TrivFn(in_place_type_t<Fn>, Args2&&... args) :
      _func_obj_invoke(&impl::ErasedInvoker<Fn, true, IsNoexcept, Ret, Args...>::invoke),
      _is_object(true) {
    NTF_PNEW(_buffer) Fn(::ntf::forward<Args2>(args)...);
  }

public:
  TrivFn(const TrivFn&) noexcept = default;
  TrivFn(TrivFn&&) noexcept = default;
  ~TrivFn() noexcept = default;

public:
  Ret operator()(Args... args) const noexcept(IsNoexcept) {
    if (_is_object) {
      if constexpr (meta::is_void_v<Ret>) {
        _func_obj_invoke(_buffer, ::ntf::forward<Args>(args)...);
      } else {
        return _func_obj_invoke(_buffer, ::ntf::forward<Args>(args)...);
      }
    } else {
      if constexpr (meta::is_void_v<Ret>) {
        _func_ptr_invoke(::ntf::forward<Args>(args)...);
      } else {
        return _func_ptr_invoke(::ntf::forward<Args>(args)...);
      }
    }
  }

public:
  template<typename Fn>
  requires(is_valid_func<meta::remove_cvref_t<Fn>>)
  TrivFn& emplace(Fn&& callable) noexcept {
    _func_obj_invoke =
      &impl::ErasedInvoker<meta::remove_cvref_t<Fn>, true, IsNoexcept, Ret, Args...>::invoke;
    _is_object = true;
    NTF_PNEW(_buffer) meta::remove_cvref_t<Fn>(::ntf::forward<Fn>(callable));
    return *this;
  }

  template<typename Fn, typename... Args2>
  requires(is_valid_func<Fn> && meta::constructible_from<Fn, Args2...>)
  TrivFn& emplace(in_place_type_t<Fn>, Args2&&... args) noexcept {
    _func_obj_invoke = &impl::ErasedInvoker<Fn, true, IsNoexcept, Ret, Args...>::invoke;
    _is_object = true;
    NTF_PNEW(_buffer) Fn(::ntf::forward<Args2>(args)...);
    return *this;
  }

  TrivFn& emplace(Ret (*func_ptr)(Args...) noexcept(IsNoexcept)) {
    NTF_THROW_IF(!func_ptr, MsgException("Assigning null function pointer to TrivFn"));
    _func_ptr_invoke = func_ptr;
    _is_object = false;
    return *this;
  }

public:
  TrivFn& operator=(const TrivFn&) noexcept = default;
  TrivFn& operator=(TrivFn&&) noexcept = default;

  template<typename F>
  requires(is_valid_func<F>)
  TrivFn& operator=(F&& callable) noexcept {
    return emplace(::ntf::forward<F>(callable));
  }

  TrivFn& operator==(Ret (*func_ptr)(Args...) noexcept(IsNoexcept)) { return emplace(func_ptr); }

private:
  alignas(MaxAlign) u8 _buffer[MaxSize];

  union {
    Ret (*_func_ptr_invoke)(Args...) noexcept(IsNoexcept);
    Ret (*_func_obj_invoke)(const void*, Args...) noexcept(IsNoexcept);
  };

  bool _is_object;
};

// non const specialization
template<size_t MaxSize, size_t MaxAlign, bool IsNoexcept, typename Ret, typename... Args>
class TrivFn<Ret(Args...) noexcept(IsNoexcept), MaxSize, MaxAlign> {
private:
  template<typename T>
  static constexpr bool is_valid_func =
    meta::invocable_with_r<T, Ret, Args...> && !meta::is_same_v<TrivFn, T> &&
    meta::trivially_copy_constructible<T> && meta::trivially_destructible<T>;

public:
  explicit TrivFn(Ret (*func_ptr)(Args...) noexcept(IsNoexcept)) :
      _func_ptr_invoke(func_ptr), _is_object(false) {
    NTF_THROW_IF(!func_ptr, MsgException("Assigning null function pointer to TrivFn"));
  }

  template<typename Fn>
  requires(is_valid_func<meta::remove_cvref_t<Fn>>)
  TrivFn(Fn&& callable) noexcept :
      _func_obj_invoke(
        &impl::ErasedInvoker<meta::remove_cvref_t<Fn>, false, IsNoexcept, Ret, Args...>::invoke),
      _is_object(true) {
    NTF_PNEW(_buffer) meta::remove_cvref_t<Fn>(::ntf::forward<Fn>(callable));
  }

  template<typename Fn, typename... Args2>
  requires(is_valid_func<Fn> && meta::constructible_from<Fn, Args2...>)
  explicit TrivFn(in_place_type_t<Fn>, Args2&&... args) :
      _func_obj_invoke(&impl::ErasedInvoker<Fn, false, IsNoexcept, Ret, Args...>::invoke),
      _is_object(true) {
    NTF_PNEW(_buffer) Fn(::ntf::forward<Args2>(args)...);
  }

public:
  TrivFn(const TrivFn&) noexcept = default;
  TrivFn(TrivFn&&) noexcept = default;
  ~TrivFn() noexcept = default;

public:
  constexpr Ret operator()(Args... args) noexcept(IsNoexcept) {
    if (_is_object) {
      if constexpr (meta::is_void_v<Ret>) {
        _func_obj_invoke(_buffer, ::ntf::forward<Args>(args)...);
      } else {
        return _func_obj_invoke(_buffer, ::ntf::forward<Args>(args)...);
      }
    } else {
      if constexpr (meta::is_void_v<Ret>) {
        _func_ptr_invoke(::ntf::forward<Args>(args)...);
      } else {
        return _func_ptr_invoke(::ntf::forward<Args>(args)...);
      }
    }
  }

public:
  template<typename Fn>
  requires(is_valid_func<meta::remove_cvref_t<Fn>>)
  TrivFn& emplace(Fn&& callable) noexcept {
    _func_obj_invoke =
      &impl::ErasedInvoker<meta::remove_cvref_t<Fn>, false, IsNoexcept, Ret, Args...>::invoke;
    _is_object = true;
    NTF_PNEW(_buffer) meta::remove_cvref_t<Fn>(::ntf::forward<Fn>(callable));
    return *this;
  }

  template<typename Fn, typename... Args2>
  requires(is_valid_func<Fn> && meta::constructible_from<Fn, Args2...>)
  TrivFn& emplace(in_place_type_t<Fn>, Args2&&... args) noexcept {
    _func_obj_invoke = &impl::ErasedInvoker<Fn, false, IsNoexcept, Ret, Args...>::invoke;
    _is_object = true;
    NTF_PNEW(_buffer) Fn(::ntf::forward<Args2>(args)...);
    return *this;
  }

  TrivFn& emplace(Ret (*func_ptr)(Args...) noexcept(IsNoexcept)) {
    NTF_THROW_IF(!func_ptr, MsgException("Assigning null function pointer to TrivFn"));
    _func_ptr_invoke = func_ptr;
    _is_object = false;
    return *this;
  }

public:
  TrivFn& operator=(const TrivFn&) noexcept = default;
  TrivFn& operator=(TrivFn&&) noexcept = default;

  template<typename F>
  requires(is_valid_func<F>)
  TrivFn& operator=(F&& callable) noexcept {
    return emplace(::ntf::forward<F>(callable));
  }

  TrivFn& operator==(Ret (*func_ptr)(Args...) noexcept(IsNoexcept)) { return emplace(func_ptr); }

private:
  alignas(MaxAlign) u8 _buffer[MaxSize];

  union {
    Ret (*_func_ptr_invoke)(Args...) noexcept(IsNoexcept);
    Ret (*_func_obj_invoke)(void*, Args...) noexcept(IsNoexcept);
  };

  bool _is_object;
};

} // namespace ntf

#endif // NTF_FUNC_HPP_
