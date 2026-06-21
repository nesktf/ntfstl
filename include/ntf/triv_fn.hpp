#ifndef NTF_TRIV_FN_HPP_
#define NTF_TRIV_FN_HPP_

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
    std::is_invocable_r_v<Ret, T, Args...> && !std::is_same_v<TrivFn, T> &&
    std::is_trivially_copyable_v<T> && std::is_trivially_destructible_v<T>;

public:
  TrivFn(Ret (*func_ptr)(Args...) noexcept(IsNoexcept)) :
      _func_ptr_invoke(func_ptr), _is_object(false) {
    NTF_THROW_IF(!func_ptr, std::runtime_error("Assigning null function pointer to TrivFn"));
  }

  template<typename F>
  requires(is_valid_func<std::remove_cvref_t<F>>)
  TrivFn(F&& functor) noexcept :
      _func_obj_invoke(
        &impl::ErasedInvoker<std::remove_cvref_t<F>, true, IsNoexcept, Ret, Args...>::invoke),
      _is_object(true) {
    NTF_PNEW(_buffer) std::remove_cvref_t<F>(std::forward<F>(functor));
  }

  template<typename F, typename... Args2>
  requires(is_valid_func<F> && std::constructible_from<F, Args2...>)
  TrivFn(std::in_place_type_t<F>, Args2&&... args) :
      _func_obj_invoke(&impl::ErasedInvoker<F, true, IsNoexcept, Ret, Args...>::invoke),
      _is_object(true) {
    NTF_PNEW(_buffer) F(forward<Args2>(args)...);
  }

public:
  TrivFn(const TrivFn&) noexcept = default;
  TrivFn(TrivFn&&) noexcept = default;
  ~TrivFn() noexcept = default;

public:
  Ret operator()(Args... args) const noexcept(IsNoexcept) {
    if (_is_object) {
      if constexpr (std::is_void_v<Ret>) {
        _func_obj_invoke(_buffer, std::forward<Args>(args)...);
      } else {
        return _func_obj_invoke(_buffer, std::forward<Args>(args)...);
      }
    } else {
      if constexpr (std::is_void_v<Ret>) {
        _func_ptr_invoke(std::forward<Args>(args)...);
      } else {
        return _func_ptr_invoke(std::forward<Args>(args)...);
      }
    }
  }

public:
  template<typename F>
  requires(is_valid_func<std::remove_cvref_t<F>>)
  TrivFn& emplace(F&& functor) noexcept {
    _func_obj_invoke =
      &impl::ErasedInvoker<std::remove_cvref_t<F>, true, IsNoexcept, Ret, Args...>::invoke;
    _is_object = true;
    NTF_PNEW(_buffer) std::remove_cvref_t<F>(std::forward<F>(functor));
    return *this;
  }

  template<typename F, typename... Args2>
  requires(is_valid_func<F> && std::constructible_from<F, Args2...>)
  TrivFn& emplace(std::in_place_type_t<F>, Args2&&... args) noexcept {
    _func_obj_invoke = &impl::ErasedInvoker<F, true, IsNoexcept, Ret, Args...>::invoke;
    _is_object = true;
    NTF_PNEW(_buffer) F(std::forward<Args2>(args)...);
    return *this;
  }

  TrivFn& emplace(Ret (*func_ptr)(Args...) noexcept(IsNoexcept)) {
    NTF_THROW_IF(!func_ptr, std::runtime_error("Assigning null function pointer to TrivFn"));
    _func_ptr_invoke = func_ptr;
    _is_object = false;
    return *this;
  }

public:
  TrivFn& operator=(const TrivFn&) noexcept = default;
  TrivFn& operator=(TrivFn&&) noexcept = default;

  template<typename F>
  requires(is_valid_func<F>)
  TrivFn& operator=(F&& functor) noexcept {
    return emplace(std::forward<F>(functor));
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
    std::is_invocable_r_v<Ret, T, Args...> && !std::is_same_v<TrivFn, T> &&
    std::is_trivially_copyable_v<T> && std::is_trivially_destructible_v<T>;

public:
  explicit TrivFn(Ret (*func_ptr)(Args...) noexcept(IsNoexcept)) :
      _func_ptr_invoke(func_ptr), _is_object(false) {
    NTF_THROW_IF(!func_ptr, std::runtime_error("Assigning null function pointer to TrivFn"));
  }

  template<typename F>
  requires(is_valid_func<std::remove_cvref_t<F>>)
  TrivFn(F&& functor) noexcept :
      _func_obj_invoke(
        &impl::ErasedInvoker<std::remove_cvref_t<F>, false, IsNoexcept, Ret, Args...>::invoke),
      _is_object(true) {
    NTF_PNEW(_buffer) std::remove_cvref_t<F>(forward<F>(functor));
  }

  template<typename F, typename... Args2>
  requires(is_valid_func<F> && std::constructible_from<F, Args2...>)
  TrivFn(std::in_place_type_t<F>, Args2&&... args) :
      _func_obj_invoke(&impl::ErasedInvoker<F, false, IsNoexcept, Ret, Args...>::invoke),
      _is_object(true) {
    NTF_PNEW(_buffer) F(std::forward<Args2>(args)...);
  }

public:
  TrivFn(const TrivFn&) noexcept = default;
  TrivFn(TrivFn&&) noexcept = default;
  ~TrivFn() noexcept = default;

public:
  constexpr Ret operator()(Args... args) noexcept(IsNoexcept) {
    if (_is_object) {
      if constexpr (std::is_void_v<Ret>) {
        _func_obj_invoke(_buffer, std::forward<Args>(args)...);
      } else {
        return _func_obj_invoke(_buffer, std::forward<Args>(args)...);
      }
    } else {
      if constexpr (std::is_void_v<Ret>) {
        _func_ptr_invoke(std::forward<Args>(args)...);
      } else {
        return _func_ptr_invoke(std::forward<Args>(args)...);
      }
    }
  }

public:
  template<typename F>
  requires(is_valid_func<std::remove_cvref_t<F>>)
  TrivFn& emplace(F&& functor) noexcept {
    _func_obj_invoke =
      &impl::ErasedInvoker<std::remove_cvref_t<F>, false, IsNoexcept, Ret, Args...>::invoke;
    _is_object = true;
    NTF_PNEW(_buffer) std::remove_cvref_t<F>(std::forward<F>(functor));
    return *this;
  }

  template<typename F, typename... Args2>
  requires(is_valid_func<F> && std::constructible_from<F, Args2...>)
  TrivFn& emplace(std::in_place_type_t<F>, Args2&&... args) noexcept {
    _func_obj_invoke = &impl::ErasedInvoker<F, false, IsNoexcept, Ret, Args...>::invoke;
    _is_object = true;
    NTF_PNEW(_buffer) F(forward<Args2>(args)...);
    return *this;
  }

  TrivFn& emplace(Ret (*func_ptr)(Args...) noexcept(IsNoexcept)) {
    NTF_THROW_IF(!func_ptr, std::runtime_error("Assigning null function pointer to TrivFn"));
    _func_ptr_invoke = func_ptr;
    _is_object = false;
    return *this;
  }

public:
  TrivFn& operator=(const TrivFn&) noexcept = default;
  TrivFn& operator=(TrivFn&&) noexcept = default;

  template<typename F>
  requires(is_valid_func<F>)
  TrivFn& operator=(F&& functor) noexcept {
    return emplace(std::forward<F>(functor));
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

#endif // NTF_TRIV_FN_HPP_
