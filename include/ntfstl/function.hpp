#pragma once

#include <ntfstl/any.hpp>

namespace ntf {

template<typename Signature>
class function_view;

template<typename Ret, typename... Args>
class function_view<Ret(Args...)> {
private:
  template<typename T>
  static constexpr Ret _invoke_for(void* obj, Args... args) {
    if constexpr (std::is_void_v<Ret>) {
      std::invoke(*static_cast<T*>(obj), std::forward<Args>(args)...);
    } else {
      return std::invoke(*static_cast<T*>(obj), std::forward<Args>(args)...);
    }
  }

public:
  using signature = Ret(Args...);
  using return_type = Ret;

public:
  constexpr function_view() noexcept : _data{nullptr}, _invoke_ptr{nullptr} {}

  explicit constexpr function_view(std::nullptr_t) noexcept :
      _data{nullptr}, _invoke_ptr{nullptr} {}

  explicit constexpr function_view(Ret (*fun)(Args...)) noexcept :
      _data{nullptr}, _invoke_ptr{fun} {}

  template<typename T>
  requires(std::is_invocable_r_v<Ret, T, Args...> && !std::is_same_v<function_view, T>)
  constexpr function_view(T& functor) noexcept :
      _data{static_cast<void*>(std::addressof(functor))}, _invoke_functor{&_invoke_for<T>} {}

  template<typename T>
  requires(std::is_invocable_r_v<Ret, T, Args...> && !std::is_same_v<function_view, T>)
  constexpr function_view(T* functor) noexcept :
      _data{static_cast<void*>(functor)}, _invoke_functor{&_invoke_for<T>} {}

public:
  constexpr ~function_view() noexcept = default;
  constexpr function_view(const function_view&) noexcept = default;
  constexpr function_view(function_view&&) noexcept = default;

public:
  constexpr Ret operator()(Args... args) const {
    if (_data) {
      NTF_ASSERT(_invoke_functor);
      if constexpr (std::is_void_v<Ret>) {
        std::invoke(_invoke_functor, _data, std::forward<Args>(args)...);
      } else {
        return std::invoke(_invoke_functor, _data, std::forward<Args>(args)...);
      }
    } else {
      NTF_ASSERT(_invoke_ptr);
      if constexpr (std::is_void_v<Ret>) {
        std::invoke(_invoke_ptr, std::forward<Args>(args)...);
      } else {
        return std::invoke(_invoke_ptr, std::forward<Args>(args)...);
      }
    }
  }

  constexpr function_view& operator=(std::nullptr_t) noexcept {
    _data = nullptr;
    _invoke_ptr = nullptr;
    return *this;
  }

  constexpr function_view& operator=(Ret (*fun)(Args...)) noexcept {
    _data = nullptr;
    _invoke_ptr = fun;
    return *this;
  }

  template<typename T>
  requires(std::is_invocable_r_v<Ret, T, Args...> && !std::is_same_v<function_view, T>)
  constexpr function_view& operator=(T& functor) noexcept {
    _data = std::addressof(functor);
    _invoke_functor = &_invoke_for<T>;
    return *this;
  }

  constexpr function_view& operator=(const function_view&) noexcept = default;
  constexpr function_view& operator=(function_view&&) noexcept = default;

public:
  constexpr bool is_empty() const { return !(_data && _invoke_functor) || _invoke_ptr == nullptr; }

  constexpr explicit operator bool() const { return !is_empty(); }

private:
  void* _data;

  union {
    Ret (*_invoke_functor)(void*, Args...);
    Ret (*_invoke_ptr)(Args...);
  };
};

namespace impl {

template<typename Derived, typename Ret, size_t buff_sz, move_policy policy, size_t max_align,
         bool is_noexcept, bool is_const, typename... Args>
class inplace_function_base : public inplace_nontrivial<Derived, buff_sz, policy, max_align> {
private:
  using base_t = inplace_nontrivial<Derived,
                                    // inplace_nontrivial<Derived, buff_sz, policy, max_align>,
                                    buff_sz, policy, max_align>;
  friend base_t;

protected:
  // clang whines if we set noexcept(is_noexcept) for some reason, gcc says its ok
  using object_invoke_t =
    std::conditional_t<is_const, Ret (*)(const uint8*, Args...) /* noexcept(is_noexcept) */,
                       Ret (*)(uint8*, Args...) /* noexcept(is_noexcept) */
                       >;
  using function_invoke_t = Ret (*)(Args...) noexcept(is_noexcept);

private:
  template<typename T>
  static constexpr bool _is_functor =
    !std::same_as<std::decay_t<T>, function_invoke_t> &&
    ((is_noexcept && is_const &&
      meta::is_nothrow_const_invocable<Ret, std::decay_t<T>, Args...>) ||
     (!is_noexcept && is_const && meta::is_const_invocable<Ret, std::decay_t<T>, Args...>) ||
     (is_noexcept && !is_const && std::is_nothrow_invocable_r_v<Ret, std::decay_t<T>, Args...>) ||
     (!is_noexcept && !is_const && std::is_invocable_r_v<Ret, std::decay_t<T>, Args...>));

public:
  inplace_function_base() noexcept : base_t{}, _fun_invoker{nullptr} {}

  inplace_function_base(std::nullptr_t) noexcept :
      base_t{}, _dispatcher{nullptr}, _fun_invoker{nullptr} {}

  inplace_function_base(function_invoke_t func) noexcept :
      base_t{}, _dispatcher{nullptr}, _fun_invoker{func} {}

  template<typename T, typename... CArgs>
  inplace_function_base(std::in_place_type_t<T> tag, CArgs&&... args) noexcept(
    std::is_nothrow_constructible_v<std::decay_t<T>, CArgs...>)
  requires(_is_functor<T> && base_t::template _can_store_type<T>)
      :
      base_t{tag}, _dispatcher{&base_t::template _dispatcher_for<T>},
      _object_invoker{&Derived::template _invoker_for<T>} {
    base_t::template _construct<T>(std::forward<Args>(args)...);
  }

  template<typename T, typename U, typename... CArgs>
  inplace_function_base(
    std::in_place_type_t<T> tag, std::initializer_list<U> il,
    CArgs&&... args) noexcept(std::is_nothrow_constructible_v<std::decay_t<T>,
                                                              std::initializer_list<U>, CArgs...>)
  requires(_is_functor<T> && base_t::template _can_store_type<T>)
      :
      base_t{tag}, _dispatcher{&base_t::template _dispatcher_for<T>},
      _object_invoker{&Derived::template _invoker_for<T>} {
    base_t::template _construct<T>(il, std::forward<Args>(args)...);
  }

  template<typename T>
  inplace_function_base(T&& obj) noexcept(meta::is_nothrow_forward_constructible<T>)
  requires(_is_functor<T> && base_t::template _can_store_type<T>)
      :
      base_t{std::in_place_type_t<std::decay_t<T>>{}},
      _dispatcher{&base_t::template _dispatcher_for<T>},
      _object_invoker{&Derived::template _invoker_for<T>} {
    base_t::template _construct<T>(std::forward<T>(obj));
  }

  inplace_function_base(const inplace_function_base& other)
  requires(base_t::_can_copy)
      : base_t{other._type_id}, _dispatcher{other._dispatcher} {
    if (other._has_object()) {
      _object_invoker = other._object_invoker;
      base_t::_copy_from(other._storage);
    } else {
      _fun_invoker = other._fun_invoker;
    }
  }

  inplace_function_base(inplace_function_base&& other)
  requires(base_t::_can_move)
      : base_t{other._type_id}, _dispatcher{other._dispatcher} {
    if (other._has_object()) {
      _object_invoker = other._object_invoker;
      base_t::_move_from(other._storage);
    } else {
      _fun_invoker = other._fun_invoker;
    }
  }

  ~inplace_function_base() noexcept { base_t::_destroy(); }

public:
  template<typename T, typename... CArgs>
  std::decay_t<T>&
  emplace(CArgs&&... args) noexcept(std::is_nothrow_constructible_v<std::decay_t<T>, CArgs...>)
  requires(_is_functor<T> && base_t::template _can_store_type<T>)
  {
    base_t::_destroy();
    _set_meta<T>();
    try {
      return base_t::template _construct<T>(std::forward<CArgs>(args)...);
    } catch (...) {
      _nullify();
      NTF_RETHROW();
    }
  }

  template<typename T, typename U, typename... CArgs>
  std::decay_t<T>& emplace(std::initializer_list<U> il, CArgs&&... args) noexcept(
    std::is_nothrow_constructible_v<std::decay_t<T>, std::initializer_list<U>, CArgs...>)
  requires(_is_functor<T> && base_t::template _can_store_type<T>)
  {
    base_t::_destroy();
    _set_meta<T>();
    try {
      return base_t::template _construct<T>(il, std::forward<CArgs>(args)...);
    } catch (...) {
      _nullify();
      NTF_RETHROW();
    }
  }

  template<typename T>
  inplace_function_base& operator=(T&& obj)
  requires(_is_functor<T> && base_t::template _can_store_type<T>)
  {
    base_t::_destroy();
    _set_meta<T>();
    try {
      base_t::template _construct<T>(std::forward<T>(obj));
    } catch (...) {
      _nullify();
      NTF_RETHROW();
    }
    return *this;
  }

  template<typename T>
  inplace_function_base& operator=(std::nullptr_t) noexcept {
    base_t::_destroy();
    _nullify();
    return *this;
  }

  template<typename T>
  inplace_function_base& operator=(function_invoke_t fun) noexcept {
    base_t::_destroy();
    _nullify(fun);
    return *this;
  }

  inplace_function_base& operator=(const inplace_function_base& other)
  requires(base_t::_can_copy)
  {
    if (std::addressof(other) == this) {
      return *this;
    }

    base_t::_destroy();
    if (other._has_object()) {
      _set_meta(other);
      try {
        _copy_from(other._storage);
      } catch (...) {
        _nullify();
        NTF_RETHROW();
      }
    } else {
      _nullify(other._fun_invoker);
    }

    return *this;
  }

  inplace_function_base& operator=(inplace_function_base&& other)
  requires(base_t::_can_move)
  {
    if (std::addressof(other) == this) {
      return *this;
    }

    base_t::_destroy();
    if (other._has_object()) {
      _set_meta(other);
      try {
        base_t::_move_from(other._storage);
      } catch (...) {
        _nullify();
        NTF_RETHROW();
      }
    } else {
      _nullify(other._fun_invoker);
    }

    return *this;
  }

public:
  bool empty() const noexcept { return !base_t::_has_object() && _fun_invoker == nullptr; }

private:
  void _nullify(function_invoke_t fun = nullptr) noexcept {
    _fun_invoker = fun;
    _dispatcher = nullptr;
    this->_type_id = meta::NULL_TYPE_ID;
  }

  template<typename T>
  void _set_meta() noexcept {
    _object_invoker = &Derived::template _invoker_for<T>;
    _dispatcher = &base_t::template _dispatcher_for<T>;
    this->_type_id = base_t::template _id_from<T>();
  }

  void _set_meta(const inplace_function_base& other) noexcept {
    _object_invoker = other._object_invoker;
    _dispatcher = other._dispatcher;
    this->_type_id = other._type_id;
  }

protected:
  base_t::call_dispatcher_t _dispatcher;

  union {
    object_invoke_t _object_invoker;
    function_invoke_t _fun_invoker;
  };
};

} // namespace impl

template<typename Signature,
         size_t buff_sz = sizeof(void*), // one pointer
         move_policy policy = move_policy::copyable, size_t max_align = alignof(std::max_align_t)>
class inplace_function;

template<typename Ret, size_t buff_sz, move_policy policy, size_t max_align, bool is_noexcept,
         typename... Args>
class inplace_function<Ret(Args...) noexcept(is_noexcept), buff_sz, policy, max_align> :
    public impl::inplace_function_base<
      inplace_function<Ret(Args...) noexcept(is_noexcept), buff_sz, policy, max_align>, Ret,
      buff_sz, policy, max_align, is_noexcept, false, Args...> {
private:
  using base_t = impl::inplace_function_base<
    inplace_function<Ret(Args...) noexcept(is_noexcept), buff_sz, policy, max_align>, Ret, buff_sz,
    policy, max_align, is_noexcept, false, Args...>;
  friend base_t;

  template<typename T>
  static Ret _invoker_for(uint8* buff, Args... args) noexcept(is_noexcept) {
    using decayed_t = std::decay_t<T>;
    auto& obj = *std::launder(reinterpret_cast<decayed_t*>(buff));
    if constexpr (std::is_void_v<Ret>) {
      std::invoke(obj, std::forward<Args>(args)...);
    } else {
      return std::invoke(obj, std::forward<Args>(args)...);
    }
  }

public:
  using base_t::base_t;

public:
  Ret operator()(Args... args) noexcept(is_noexcept && NTF_ASSERT_NOEXCEPT) {
    if constexpr (std::is_void_v<Ret>) {
      if (!base_t::_has_object()) {
        NTF_ASSERT(this->_fun_invoker != nullptr, "Empty function");
        std::invoke(this->_fun_invoker, std::forward<Args>(args)...);
      } else {
        std::invoke(this->_object_invoker, this->_storage, std::forward<Args>(args)...);
      }
    } else {
      if (!base_t::_has_object()) {
        NTF_ASSERT(this->_fun_invoker != nullptr, "Empty function");
        return std::invoke(this->_fun_invoker, std::forward<Args>(args)...);
      } else {
        return std::invoke(this->_object_invoker, this->_storage, std::forward<Args>(args)...);
      }
    }
  }
};

template<typename Ret, size_t buff_sz, move_policy policy, size_t max_align, bool is_noexcept,
         typename... Args>
class inplace_function<Ret(Args...) const noexcept(is_noexcept), buff_sz, policy, max_align> :
    public impl::inplace_function_base<
      inplace_function<Ret(Args...) const noexcept(is_noexcept), buff_sz, policy, max_align>, Ret,
      buff_sz, policy, max_align, is_noexcept, true, Args...> {
private:
  using base_t = impl::inplace_function_base<
    inplace_function<Ret(Args...) const noexcept(is_noexcept), buff_sz, policy, max_align>, Ret,
    buff_sz, policy, max_align, is_noexcept, true, Args...>;
  friend base_t;

  template<typename T>
  static Ret _invoker_for(const uint8* buff, Args... args) noexcept(is_noexcept) {
    using decayed_t = std::decay_t<T>;
    const auto& obj = *std::launder(reinterpret_cast<const decayed_t*>(buff));
    if constexpr (std::is_void_v<Ret>) {
      std::invoke(obj, std::forward<Args>(args)...);
    } else {
      return std::invoke(obj, std::forward<Args>(args)...);
    }
  }

public:
  using base_t::base_t;

public:
  Ret operator()(Args... args) const noexcept(is_noexcept && NTF_ASSERT_NOEXCEPT) {
    if constexpr (std::is_void_v<Ret>) {
      if (!base_t::_has_object()) {
        NTF_ASSERT(this->_fun_invoker != nullptr, "Empty function");
        std::invoke(this->_fun_invoker, std::forward<Args>(args)...);
      } else {
        std::invoke(this->_object_invoker, this->_storage, std::forward<Args>(args)...);
      }
    } else {
      if (!base_t::_has_object()) {
        NTF_ASSERT(this->_fun_invoker != nullptr, "Empty function");
        return std::invoke(this->_fun_invoker, std::forward<Args>(args)...);
      } else {
        return std::invoke(this->_object_invoker, this->_storage, std::forward<Args>(args)...);
      }
    }
  }
};

} // namespace ntf
