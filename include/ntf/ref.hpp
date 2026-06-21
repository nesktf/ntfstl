#ifndef NTF_REF_HPP_
#define NTF_REF_HPP_

#include <ntf/memory.hpp>

#include <stdexcept>

namespace ntf {

template<typename T>
requires(!std::is_void_v<T> && !std::is_reference_v<T>)
class Ref {
public:
  using element_type = T;
  using value_type = std::remove_cvref_t<T>;

  using pointer = T*;
  using const_pointer = const T*;
  using reference = T&;
  using const_reference = const T&;

public:
  constexpr Ref(reference obj) noexcept : _ptr(std::addressof(obj)) {}

  template<typename U>
  requires(std::is_convertible_v<U*, T*>)
  constexpr Ref(U& obj) noexcept : _ptr(std::addressof(obj)) {}

  constexpr explicit Ref(pointer ptr) : _ptr(ptr) {
    NTF_THROW_IF(!_ptr, std::runtime_error("Assigning nullptr to Ref"));
  }

  template<typename U>
  requires(std::is_convertible_v<U*, T*> && !std::same_as<U, T>)
  constexpr Ref(U* ptr) : _ptr(ptr) {
    NTF_THROW_IF(!_ptr, std::runtime_error("Assigning nullptr to Ref"));
  }

  template<typename U>
  requires(std::is_convertible_v<U*, T*>)
  constexpr Ref(const Ref<U>& other) noexcept : _ptr{other.data()} {}

  constexpr Ref(const Ref&) noexcept = default;
  constexpr Ref(Ref&&) noexcept = default;

  constexpr ~Ref() noexcept = default;

public:
  constexpr reference get() const noexcept { return *_ptr; }

  constexpr pointer ptr() const noexcept { return _ptr; }

  constexpr pointer data() const noexcept { return _ptr; }

public:
  constexpr pointer operator->() const noexcept { return _ptr; }

  constexpr reference operator*() const noexcept { return *_ptr; }

  constexpr Ref& operator=(pointer ptr) {
    NTF_THROW_IF(!ptr, std::runtime_error("Assigning nullptr to Ref"));
    _ptr = ptr;
    return *this;
  }

  template<typename U>
  requires(std::is_convertible_v<U*, T*> && !std::same_as<U, T>)
  constexpr Ref& operator=(U* ptr) {
    NTF_THROW_IF(!ptr, std::runtime_error("Assigning nullptr to Ref"));
    _ptr = ptr;
    return *this;
  }

  template<typename U>
  requires(std::is_convertible_v<U*, T*>)
  constexpr Ref& operator=(U& obj) noexcept {
    _ptr = std::addressof(obj);
    return *this;
  }

  template<typename U>
  requires(std::is_convertible_v<U*, T*>)
  constexpr Ref& operator=(const Ref<U>& other) noexcept {
    _ptr = other.ptr();
    return *this;
  }

  constexpr Ref& operator=(const Ref&) noexcept = default;
  constexpr Ref& operator=(Ref&&) noexcept = default;

public:
  operator reference() const noexcept { return get(); }

private:
  T* _ptr;
};

template<typename Signature>
class FnRef;

template<bool IsNoexcept, typename Ret, typename... Args>
class FnRef<Ret(Args...) const noexcept(IsNoexcept)> {
public:
  using signature = Ret(Args...) const noexcept(IsNoexcept);
  using return_type = Ret;

public:
  explicit constexpr FnRef(Ret (*func_ptr)(Args...) noexcept(IsNoexcept)) :
      _func_obj_invoke(nullptr), _func_ptr_invoke(func_ptr) {
    NTF_THROW_IF(!func_ptr, std::runtime_error("Assigning null function pointer to FnRef"));
  }

  template<typename T>
  requires(std::is_invocable_r_v<Ret, T, Args...> && !std::is_same_v<FnRef, T>)
  constexpr FnRef(const T& callable) noexcept :
      _func_obj_invoke(&impl::ErasedInvoker<T, true, IsNoexcept, Ret, Args...>::invoke),
      _obj_ptr(std::addressof(callable)) {}

public:
  constexpr ~FnRef() noexcept = default;
  constexpr FnRef(const FnRef&) noexcept = default;
  constexpr FnRef(FnRef&&) noexcept = default;

public:
  constexpr Ret operator()(Args... args) const noexcept(IsNoexcept) {
    if (_func_obj_invoke) {
      if constexpr (std::is_void_v<Ret>) {
        _func_obj_invoke(_obj_ptr, std::forward<Args>(args)...);
      } else {
        return _func_obj_invoke(_obj_ptr, std::forward<Args>(args)...);
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
  constexpr FnRef& operator=(Ret (*func_ptr)(Args...) noexcept(IsNoexcept)) noexcept {
    _func_obj_invoke = nullptr;
    _func_ptr_invoke = func_ptr;
    return *this;
  }

  template<typename T>
  requires(std::is_invocable_r_v<Ret, T, Args...> && !std::is_same_v<FnRef, T>)
  constexpr FnRef& operator=(const T& callable) noexcept {
    _func_obj_invoke = &impl::ErasedInvoker<T, true, IsNoexcept, Ret, Args...>::invoke;
    _obj_ptr = std::addressof(callable);
    return *this;
  }

  constexpr FnRef& operator=(const FnRef&) noexcept = default;
  constexpr FnRef& operator=(FnRef&&) noexcept = default;

private:
  Ret (*_func_obj_invoke)(const void*, Args...) noexcept(IsNoexcept);

  union {
    Ret (*_func_ptr_invoke)(Args...) noexcept(IsNoexcept);
    const void* _obj_ptr;
  };
};

template<bool IsNoexcept, typename Ret, typename... Args>
class FnRef<Ret(Args...) noexcept(IsNoexcept)> {
public:
  using signature = Ret(Args...) noexcept(IsNoexcept);
  using return_type = Ret;

public:
  explicit constexpr FnRef(Ret (*func_ptr)(Args...) noexcept(IsNoexcept)) :
      _func_obj_invoke(nullptr), _func_ptr_invoke(func_ptr) {
    NTF_THROW_IF(!func_ptr, std::runtime_error("Assigning null function pointer to FnRef"));
  }

  template<typename T>
  requires(std::is_invocable_r_v<Ret, T, Args...> && !std::is_same_v<FnRef, T>)
  constexpr FnRef(T& callable) noexcept :
      _func_obj_invoke(&impl::ErasedInvoker<T, false, IsNoexcept, Ret, Args...>::invoke),
      _obj_ptr(std::addressof(callable)) {}

public:
  constexpr ~FnRef() noexcept = default;
  constexpr FnRef(const FnRef&) noexcept = default;
  constexpr FnRef(FnRef&&) noexcept = default;

public:
  constexpr Ret operator()(Args... args) const noexcept(IsNoexcept) {
    if (_func_obj_invoke) {
      if constexpr (std::is_void_v<Ret>) {
        _func_obj_invoke(_obj_ptr, std::forward<Args>(args)...);
      } else {
        return _func_obj_invoke(_obj_ptr, std::forward<Args>(args)...);
      }
    } else {
      if constexpr (std::is_void_v<Ret>) {
        _func_ptr_invoke(std::forward<Args>(args)...);
      } else {
        return _func_ptr_invoke(std::forward<Args>(args)...);
      }
    }
  }

  constexpr FnRef& operator=(Ret (*func_ptr)(Args...) noexcept(IsNoexcept)) noexcept {
    _func_obj_invoke = nullptr;
    _func_ptr_invoke = func_ptr;
    return *this;
  }

  template<typename T>
  requires(std::is_invocable_r_v<Ret, T, Args...> && !std::is_same_v<FnRef, T>)
  constexpr FnRef& operator=(T& callable) noexcept {
    _func_obj_invoke = &impl::ErasedInvoker<T, false, IsNoexcept, Ret, Args...>::invoke;
    _obj_ptr = std::addressof(callable);
    return *this;
  }

  constexpr FnRef& operator=(const FnRef&) noexcept = default;
  constexpr FnRef& operator=(FnRef&&) noexcept = default;

private:
  Ret (*_func_obj_invoke)(void*, Args...) noexcept(IsNoexcept);

  union {
    Ret (*_func_ptr_invoke)(Args...) noexcept(IsNoexcept);
    void* _obj_ptr;
  };
};

} // namespace ntf

#endif // NTF_REF_HPP_
