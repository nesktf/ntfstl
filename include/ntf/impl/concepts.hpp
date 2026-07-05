#ifndef NTF_CONCEPTS_HPP_
#define NTF_CONCEPTS_HPP_

#include <ntf/core.hpp>

namespace ntf::meta {

template<typename To, typename From>
concept convertible_to = requires(From obj) { static_cast<To>(obj); };

template<typename T, typename... Args>
concept constructible_from = requires(Args... args) { T(::ntf::forward<Args>(args)...); };

template<typename T>
concept default_constructible = requires() { T(); };

template<typename T>
concept nothrow_default_constructible = requires() { requires noexcept(T()); };

template<typename T, typename... Args>
concept nothrow_constructible =
  requires(Args... args) { requires noexcept(T(::ntf::forward<Args>(args)...)); };

template<typename T>
concept nothrow_move_constructible = requires(T&& obj) { requires noexcept(T(::ntf::move(obj))); };

template<typename T>
concept move_constructible = requires(T& obj) { T(::ntf::move(obj)); };

template<typename T>
concept nothrow_move_assignable = requires(T&& obj) { requires noexcept(obj = ::ntf::move(obj)); };

template<typename T>
concept copy_constructible = requires(const T& obj) { T(obj); };

template<typename T>
concept copy_assignable = requires(T src, const T& dst) {
  { src = dst } -> same_as<T&>;
};

template<typename T>
concept nothrow_copy_constructible = requires(const T& obj) { requires noexcept(T(obj)); };

template<typename T>
concept nothrow_copy_assignable =
  requires(T& obj, const T& other) { requires noexcept(obj = ::ntf::move(other)); };

template<typename T>
concept nothrow_destructible = requires(T obj) { requires noexcept(obj.~T()); };

template<typename T>
struct is_trivially_destructible :
    public bool_constant<
#if defined(__has_builtin)
#if __has_builtin(__is_trivially_destructible)
      __is_trivially_destructible(T)
#elif __has_builtin(__has_trivial_destructor)
      __has_trivial_destructor(T)
#endif
#endif
      > {
};

template<typename T>
constexpr inline bool is_trivially_destructible_v = is_trivially_destructible<T>::value;

template<typename T>
concept trivially_destructible = is_trivially_destructible_v<T>;

template<typename T, bool = is_referenceable<T>::value>
struct is_trivially_copy_constructible_impl :
    public bool_constant<
#if defined(__has_builtin)
#if __has_builtin(__is_trivially_constructible)
      __is_trivially_copyable(T)
#elif __has_builtin(__has_trivial_copy)
      __has_trivial_copy(T)
#endif
#endif
      > {
};

template<typename T>
struct is_trivially_copy_constructible_impl<T, false> : public false_type {};

template<typename T>
struct is_trivially_copy_constructible : public is_trivially_copy_constructible_impl<T> {};

template<typename T>
constexpr inline bool is_trivially_copy_constructible_v =
  is_trivially_copy_constructible<T>::value;

template<typename T>
concept trivially_copy_constructible = is_trivially_copy_constructible_v<T>;

template<typename T>
struct is_trivially_move_constructible :
    public bool_constant<__is_trivially_constructible(T, T&&)> {};

template<typename T>
constexpr inline bool is_trivially_move_constructible_v =
  is_trivially_move_constructible<T>::value;

template<typename T>
concept trivially_move_constructible = is_trivially_move_constructible_v<T>;

template<typename T>
struct is_trivially_copy_asignable :
    public bool_constant<__is_trivially_assignable(T&, const T&)> {};

template<typename T>
constexpr inline bool is_trivially_copy_assignable_v = is_trivially_copy_asignable<T>::value;

template<typename T>
concept trivially_copy_assignable = is_trivially_copy_assignable_v<T>;

template<typename T>
concept trivially_copyable = trivially_copy_constructible<T> && trivially_copy_assignable<T>;

template<typename T>
struct is_trivially_move_asignable : public bool_constant<__is_trivially_assignable(T&, T&&)> {};

template<typename T>
constexpr inline bool is_trivially_move_assignable_v = is_trivially_move_asignable<T>::value;

template<typename T>
concept trivially_move_assignable = is_trivially_move_assignable_v<T>;

template<typename T, typename... Args>
struct is_trivially_constructible :
    public bool_constant<__is_trivially_constructible(T, Args...)> {};

template<typename T, typename... Args>
constexpr inline bool is_trivially_constructible_v = is_trivially_constructible<T, Args...>::value;

template<typename T, typename... Args>
concept trivially_constructible_with = is_trivially_constructible_v<T, Args...>;

template<typename T>
concept trivially_constructible = is_trivially_constructible_v<T>;

template<typename Fn, typename... Args>
concept invocable_with = requires(Fn func, Args... args) { func(::ntf::forward<Args>(args)...); };

template<typename Fn, typename... Args>
concept nothrow_invocable_with =
  requires(Fn func, Args... args) { requires noexcept(func(::ntf::forward<Args>(args)...)); };

template<typename Fn, typename Ret, typename... Args>
concept invocable_with_r = requires(Fn func, Args... args) {
  { func(::ntf::forward<Args>(args)...) } -> same_as<Ret>;
};

template<typename Fn, typename Ret, typename... Args>
concept nothrow_invocable_with_r = requires(Fn func, Args... args) {
  { func(::ntf::forward<Args>(args)...) } -> same_as<Ret>;
  requires noexcept(func(::ntf::forward<Args>(args)...));
};

template<typename Fn, typename... Args>
concept invocable = invocable_with<Fn>;

template<typename Fn, typename... Args>
concept nothrow_invocable = nothrow_invocable_with<Fn>;

template<typename Fn, typename Ret>
concept invocable_r = invocable_with_r<Fn, Ret>;

template<typename Fn, typename Ret>
concept nothrow_invocable_r = nothrow_invocable_with_r<Fn, Ret>;

struct invoke_result_impl {
  template<typename Fn, typename... Args>
  static success_type<decltype(declval<Fn>()(declval<Args>()...))> test_call(int);

  template<typename...>
  static failure_type test_call(...);
};

template<typename Fn, typename... Args>
struct invoke_result {
  using type = decltype(invoke_result_impl::template test_call<Fn, Args...>(0))::type;
};

template<typename Fn, typename... Args>
using invoke_result_t = typename invoke_result<Fn, Args...>::type;

template<typename T>
concept mem_resource = requires(T res, size_t size, size_t align, void* ptr) {
  { res.allocate(size, align) } -> same_as<void*>;
  { res.deallocate(ptr, size) } -> same_as<void>;
};

template<typename Alloc, typename T>
concept allocator_of = requires(Alloc alloc, const Alloc coalloc, T* ptr, size_t n) {
  { alloc.allocate(n) } -> same_as<T*>;
  { alloc.deallocate(ptr, n) } -> same_as<void>;
  { coalloc == coalloc } -> same_as<bool>;
  requires noexcept(coalloc == coalloc);
  requires copy_constructible<Alloc>;
  requires copy_assignable<Alloc>;
};

template<typename Alloc, typename T>
concept alloc_arg = !same_as<T, remove_cvref_t<Alloc>> && allocator_of<remove_cvref_t<Alloc>, T>;

template<typename Mem, typename T>
concept mem_arg = requires(Mem m) {
  requires mem_resource<remove_cvref_t<Mem>>;
  requires allocator_of<typename remove_cvref_t<Mem>::template bind_alloc<T>, T>;
};

template<typename E>
concept exception_msg_type = requires(const E& err) {
  { err.what() } -> same_as<const char*>;
  requires(noexcept(err.what()));
};

} // namespace ntf::meta

#endif // NTF_CONCEPTS_HPP_
