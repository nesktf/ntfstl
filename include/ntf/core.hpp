#ifndef NTF_CORE_HPP_
#define NTF_CORE_HPP_

#include <ntf/impl/macro.h>

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

namespace meta {

template<typename T>
struct remove_reference {
  using type = T;
};

template<typename T>
struct remove_reference<T&> {
  using type = T;
};

template<typename T>
struct remove_reference<T&&> {
  using type = T;
};

template<typename T>
using remove_reference_t = typename remove_reference<T>::type;

template<typename T>
struct remove_const {
  using type = T;
};

template<typename T>
struct remove_const<const T> {
  using type = T;
};

template<typename T>
using remove_const_t = typename remove_const<T>::type;

template<typename T>
struct remove_cv {
  using type = T;
};

template<typename T>
struct remove_cv<const T> {
  using type = T;
};

template<typename T>
struct remove_cv<volatile T> {
  using type = T;
};

template<typename T>
struct remove_cv<const volatile T> {
  using type = T;
};

template<typename T>
using remove_cv_t = typename remove_cv<T>::type;

template<typename T>
struct remove_cvref {
  using type = remove_cv_t<remove_reference_t<T>>;
};

template<typename T>
using remove_cvref_t = typename remove_cvref<T>::type;

template<typename T, T Val>
struct integral_constant {
  static constexpr T value = Val;
};

struct true_type : public integral_constant<bool, true> {};

struct false_type : public integral_constant<bool, false> {};

template<typename T>
struct success_type {
  using type = T;
};

struct failure_type {};

template<bool Bool>
struct bool_constant : public integral_constant<bool, Bool> {};

template<typename T>
struct type_identity {
  using type = T;
};

template<typename T>
using type_identity_t = typename type_identity<T>::type;

template<typename T>
struct is_reference : public false_type {};

template<typename T>
struct is_reference<T&> : public true_type {};

template<typename T>
struct is_reference<T&&> : public true_type {};

template<typename T>
constexpr inline bool is_reference_v = is_reference<T>::value;

template<typename T>
struct is_lvalue_reference : public false_type {};

template<typename T>
struct is_lvalue_reference<T&> : public true_type {};

template<typename T>
constexpr inline bool is_lvalue_reference_v = is_lvalue_reference<T>::value;

template<typename T>
struct is_rvalue_reference : public false_type {};

template<typename T>
struct is_rvalue_reference<T&&> : public true_type {};

template<typename T>
constexpr inline bool is_rvalue_reference_v = is_rvalue_reference<T>::value;

template<typename T>
constexpr bool false_value = false;

template<typename T, typename U>
struct is_same : public false_type {};

template<typename T>
struct is_same<T, T> : public true_type {};

template<typename T, typename U>
constexpr inline bool is_same_v = is_same<T, U>::value;

template<typename T, typename U>
concept same_as = is_same_v<T, U>;

template<typename T>
struct is_void : public false_type {};

template<>
struct is_void<void> : public true_type {};

template<typename T>
constexpr inline bool is_void_v = is_void<T>::value;

template<bool Cond, typename IfTrue, typename IfFalse>
struct conditional {
  using type = IfTrue;
};

template<typename IfTrue, typename IfFalse>
struct conditional<false, IfTrue, IfFalse> {
  using type = IfFalse;
};

template<bool Cond, typename IfTrue, typename IfFalse>
using conditional_t = typename conditional<Cond, IfTrue, IfFalse>::type;

template<typename T>
struct is_const : public false_type {};

template<typename T>
struct is_const<T const> : public true_type {};

template<typename T>
constexpr inline bool is_const_v = is_const<T>::value;

template<typename T>
struct is_function : public bool_constant<!is_const_v<const T>> {};

template<typename T>
struct is_function<T&> : public false_type {};

template<typename T>
struct is_function<T&&> : public false_type {};

template<typename T>
constexpr inline bool is_function_v = is_function<T>::value;

template<typename T>
struct is_array : public false_type {};

template<typename T, size_t Size>
struct is_array<T[Size]> : public true_type {};

template<typename T>
struct is_array<T[]> : public true_type {};

template<typename T>
constexpr inline bool is_array_v = is_array<T>::value;

template<typename T>
struct is_pointer_impl : public false_type {};

template<typename T>
struct is_pointer_impl<T*> : public true_type {};

template<typename T>
struct is_pointer : public is_pointer_impl<remove_cv_t<T>>::type {};

template<typename T>
constexpr inline bool is_pointer_v = is_pointer<T>::value;

template<typename T>
struct remove_extent {
  using type = T;
};

template<typename T, size_t Size>
struct remove_extent<T[Size]> {
  using type = T;
};

template<typename T>
struct remove_extent<T[]> {
  using type = T;
};

template<typename...>
using void_t = void;

template<typename...>
struct sfinae_or;

template<>
struct sfinae_or<> : public false_type {};

template<typename T1>
struct sfinae_or<T1> : public T1 {};

template<typename T1, typename T2>
struct sfinae_or<T1, T2> : public conditional_t<T1::value, T1, T2> {};

template<typename T1, typename T2, typename T3, typename... Ts>
struct sfinae_or<T1, T2, T3, Ts...> :
    public conditional_t<T1::value, T1, sfinae_or<T2, T3, Ts...>> {};

template<typename... Ts>
constexpr inline bool sfinae_or_v = sfinae_or<Ts...>::value;

template<typename T, typename = void>
struct is_referenceable : public false_type {};

template<typename T>
struct is_referenceable<T, void_t<T&>> : public true_type {};

template<typename T, bool = sfinae_or_v<is_referenceable<T>, is_void<T>>>
struct add_pointer_impl {
  using type = T;
};

template<typename T>
struct add_pointer_impl<T, true> {
  using type = remove_reference<T>::type*;
};

template<typename T>
struct add_pointer : public add_pointer_impl<T> {};

template<typename T, bool IsArray = is_array_v<T>, bool IsFunction = is_function_v<T>>
struct decay_impl;

template<typename T>
struct decay_impl<T, false, false> {
  using type = remove_cv_t<T>;
};

template<typename T>
struct decay_impl<T, true, false> {
  using type = remove_extent<T>::type*;
};

template<typename T>
struct decay_impl<T, false, true> {
  using type = typename add_pointer<T>::type;
};

template<typename T>
class decay {
  using type = typename decay_impl<remove_reference_t<T>>::type;
};

template<typename T>
using decay_t = typename decay<T>::type;

template<typename T>
auto try_add_lvalue_ref(int) -> type_identity_t<T&>;

template<typename T> // T = void
auto try_add_lvalue_ref(...) -> type_identity_t<T>;

template<typename T>
struct add_lvalue_reference : public decltype(try_add_rvalue_ref<T>(0)) {};

template<typename T>
using add_lvalue_reference_t = typename add_lvalue_reference<T>::type;

template<typename T, bool = is_referenceable<T>::value>
struct add_rvalue_reference_impl {
  using type = T;
};

template<typename T>
struct add_rvalue_reference_impl<T, true> {
  using type = T&&;
};

/// add_rvalue_reference
template<typename T>
struct add_rvalue_reference : public add_rvalue_reference_impl<T> {};

template<typename T>
using add_rvalue_reference_t = typename add_rvalue_reference<T>::type;

} // namespace meta

template<typename T>
meta::add_rvalue_reference_t<T> declval() noexcept {
  static_assert(meta::false_value<T>, "declval no tallowed in evaluated context");
}

template<typename T>
constexpr meta::remove_reference_t<T>&& move(T&& thing) noexcept {
  return static_cast<meta::remove_reference_t<T>&&>(thing);
}

template<typename T>
NTF_NODISCARD constexpr T&& forward(meta::remove_reference_t<T>& thing) noexcept {
  return static_cast<T&&>(thing);
}

template<typename T>
NTF_NODISCARD constexpr T&& forward(meta::remove_reference_t<T>&& thing) noexcept {
  static_assert(!meta::is_lvalue_reference_v<T>, "Don't cast things to lvalue");
  return static_cast<T&&>(thing);
}

template<typename T>
constexpr const T& max(const T& a, const T& b) {
  return a < b ? b : a;
}

template<typename T>
constexpr const T& min(const T& a, const T& b) {
  return a < b ? a : b;
}

template<typename T>
NTF_NODISCARD constexpr T* launder(T* ptr) noexcept {
#if defined(__has_builtin) && __has_builtin(__builtin_launder)
  return __builtin_launder(ptr);
#else
  return ptr;
#endif
}

template<typename T>
NTF_NODISCARD constexpr T* launder_as(void* ptr) noexcept {
  return launder(static_cast<T*>(ptr));
}

template<typename T>
NTF_NODISCARD constexpr T* addressof(T& thing) noexcept {
#if defined(__has_builtin) && __has_builtin(__builtin_addressof)
  return __builtin_addressof(thing);
#else
  return &thing;
#endif
}

template<typename T>
const T* addressof(const T&&) = delete;

template<typename T>
NTF_NODISCARD constexpr const T* as_const(T* p) noexcept {
  return p;
}

template<typename T>
NTF_NODISCARD constexpr const T* as_const(const T* p) noexcept {
  return p;
}

template<typename T>
NTF_NODISCARD constexpr const T& as_const(T& p) noexcept {
  return p;
}

template<typename T>
NTF_NODISCARD constexpr const T& as_const(const T& p) noexcept {
  return p;
}

constexpr inline bool is_constant_evaluated() noexcept {
#if defined(__has_builtin) && __has_builtin(__builtin_is_constant_evaluated)
  return __builtin_is_constant_evaluated();
#else
  // if consteval { return true; } else { return false; }
  return false;
#endif
}

struct uninitialized_t {};

constexpr inline uninitialized_t uninitialized;

struct in_place_t {};

constexpr inline in_place_t in_place;

template<typename T>
struct in_place_type_t {};

template<typename T>
constexpr inline in_place_type_t<T> in_place_type;

using nullptr_t = decltype(nullptr);

class Exception {
public:
  Exception() noexcept = default;
  virtual ~Exception() noexcept = default;
  Exception(const Exception&) noexcept = default;
  Exception& operator=(const Exception&) noexcept = default;
  Exception(Exception&&) noexcept = default;
  Exception& operator=(Exception&&) noexcept = default;

public:
  virtual const char* what() const noexcept = 0;
};

class BadAlloc final : public Exception {
public:
  const char* what() const noexcept override { return "BadAlloc"; }
};

class MsgException final : public Exception {
public:
  MsgException(const char* msg) noexcept : _msg(msg) {}

public:
  const char* what() const noexcept override { return _msg; }

private:
  const char* _msg;
};

} // namespace ntf

#endif // NTF_CORE_HPP_
