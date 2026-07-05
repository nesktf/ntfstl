#ifndef NTF_CONCEPTS_HPP_
#define NTF_CONCEPTS_HPP_

#include <ntf/impl/core.hpp>

namespace ntf::meta {

template<typename T>
concept default_constructible = requires() { T(); };

template<typename T>
concept nothrow_default_constructible = requires() { requires noexcept(T()); };

template<typename T>
concept nothrow_move_constructible = requires(T&& obj) { requires noexcept(T(move(obj))); };

template<typename T>
concept nothrow_move_assignable = requires(T&& obj) { requires noexcept(obj = move(obj)); };

template<typename T>
concept copy_constructible = requires(const T& obj) { T(obj); };

template<typename T>
concept copy_assignable = requires(T src, const T& dst) {
  { src = dst } -> same_as<T&>;
};

template<typename T>
concept nothrow_copy_constructible = requires(const T& obj) { requires noexcept(T(obj)); };

template<typename T>
concept nothrow_destructible = requires(T obj) { requires noexcept(obj.~T()); };

template<typename Fn, typename... Args>
concept invocable_with = requires(Fn func, Args... args) { func(forward<Args>(args)...); };

template<typename Fn, typename... Args>
concept nothrow_invocable_with =
  requires(Fn func, Args... args) { requires noexcept(func(forward<Args>(args)...)); };

template<typename Fn, typename Ret, typename... Args>
concept invocable_with_r = requires(Fn func, Args... args) {
  { func(forward<Args>(args)...) } -> same_as<Ret>;
};

template<typename Fn, typename Ret, typename... Args>
concept nothrow_invocable_with_r = requires(Fn func, Args... args) {
  { func(forward<Args>(args)...) } -> same_as<Ret>;
  requires noexcept(func(forward<Args>(args)...));
};

template<typename Fn, typename... Args>
concept invocable = invocable_with<Fn>;

template<typename Fn, typename... Args>
concept nothrow_invocable = nothrow_invocable_with<Fn>;

template<typename Fn, typename Ret>
concept invocable_r = invocable_with_r<Fn, Ret>;

template<typename Fn, typename Ret>
concept nothrow_invocable_r = nothrow_invocable_with_r<Fn, Ret>;

template<typename T>
concept mem_resource = requires(T res, size_t size, size_t align, void* ptr) {
  { res.alloc(size, align) } -> same_as<void*>;
  { res.dealloc(ptr, size) } -> same_as<void>;
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
  requires memory_resource<remove_cvref_t<Mem>>;
  requires !same_as<T, remove_cvref_t<Mem>>;
  requires allocator_of<typename remove_cvref_t<Mem>::template bind_alloc<T>, T>;
};

} // namespace ntf::meta

#endif // NTF_CONCEPTS_HPP_
