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
struct type_identity {
  using type = T;
};

template<typename T>
auto try_add_lvalue_ref(int) -> type_identity<T&>;

template<typename T> // T = void
auto try_add_lvalue_ref(...) -> type_identity<T>;

template<typename T>
auto try_add_rvalue_ref(int) -> type_identity<T&&>;

template<typename T> // T = void
auto try_add_rvalue_ref(...) -> type_identity<T&>;

template<typename T>
struct add_lvalue_reference : public decltype(try_add_rvalue_ref<T>(0)) {};

template<typename T>
using add_lvalue_reference_t = typename add_lvalue_reference<T>::type;

template<typename T>
struct add_rvalue_reference : public decltype(try_add_rvalue_ref<T>(0)) {};

template<typename T>
using add_rvalue_reference_t = typename add_rvalue_reference<T>::type;

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
#if defined(__GNUC__) && defined(__has_builtin)
  return __builtin_launder(ptr);
#else
  return ptr;
#endif
}

template<typename T>
NTF_NODISCARD constexpr T* launder_as(void* ptr) noexcept {
  return launder(static_cast<T*>(ptr));
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
