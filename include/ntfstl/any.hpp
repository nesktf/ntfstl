#pragma once

#include <ntfstl/meta.hpp>

namespace ntf {

class bad_any_access : public std::exception {
public:
  bad_any_access() noexcept = default;
  bad_any_access(const bad_any_access&) noexcept = default;
  ~bad_any_access() noexcept = default;

  bad_any_access& operator=(const bad_any_access&) noexcept = default;

public:
  const char* what() const noexcept override { return "bad_any_access"; }
};

enum class move_policy : uint8 {
  non_movable = 0,
  movable = 1,
  copyable = 2,
};

constexpr inline bool check_policy(move_policy src, move_policy dst) {
  const auto src_val = static_cast<std::underlying_type_t<move_policy>>(src);
  const auto dst_val = static_cast<std::underlying_type_t<move_policy>>(dst);
  return src_val >= dst_val;
}

namespace impl {

template<size_t buff_sz, size_t max_align>
class inplace_storage {
protected:
  template<typename T>
  static constexpr bool is_storable =
    (sizeof(std::decay_t<T>) <= buff_sz) && (alignof(std::decay_t<T>) <= max_align) &&
    (max_align % alignof(std::decay_t<T>) == 0);

protected:
  template<typename T>
  static meta::type_id_t _id_from() noexcept {
    return meta::meta_traits<std::decay_t<T>>::get_id();
  }

protected:
  inplace_storage() noexcept : _type_id{meta::NULL_TYPE_ID} {}

  inplace_storage(meta::type_id_t id) noexcept : _type_id{id} {}

  template<typename T>
  inplace_storage(std::in_place_type_t<T>) noexcept : _type_id{_id_from<T>()} {}

protected:
  template<typename T, typename... Args>
  std::decay_t<T>&
  _construct(Args&&... args) noexcept(std::is_nothrow_constructible_v<std::decay_t<T>, Args...>)
  requires(is_storable<T>)
  {
    new (_storage) std::decay_t<T>(std::forward<Args>(args)...);
    return *std::launder(reinterpret_cast<std::decay_t<T>*>(_storage));
  }

public:
  template<typename T>
  T& get() noexcept(NTF_NOEXCEPT) {
    NTF_THROW_IF(meta::meta_traits<std::decay_t<T>>::get_id() != _type_id,
                 ::ntf::bad_any_access());
    return *std::launder(reinterpret_cast<std::decay_t<T>*>(_storage));
  }

  template<typename T>
  const T& get() const noexcept(NTF_NOEXCEPT) {
    NTF_THROW_IF(meta::meta_traits<std::decay_t<T>>::get_id() != _type_id,
                 ::ntf::bad_any_access());
    return *std::launder(reinterpret_cast<const std::decay_t<T>*>(_storage));
  }

  template<typename T>
  bool has_type() const noexcept {
    return _id_from<T>() == _type_id;
  }

  meta::type_id_t type_id() const noexcept { return _type_id; }

protected:
  bool _has_object() const { return _type_id != meta::NULL_TYPE_ID; }

public:
  template<typename T>
  explicit operator const T&() const& {
    return get<T>();
  }

  template<typename T>
  explicit operator T&() & {
    return get<T>();
  }

  template<typename T>
  explicit operator T&&() && {
    return std::move(get<T>());
  }

protected:
  alignas(max_align) uint8 _storage[buff_sz];
  meta::type_id_t _type_id;
};

} // namespace impl

template<size_t buff_sz = sizeof(uint64), size_t max_align = alignof(uint64)>
class inplace_trivial : public impl::inplace_storage<buff_sz, max_align> {
private:
  using base_t = impl::inplace_storage<buff_sz, max_align>;

  template<typename T>
  static constexpr bool _can_store =
    !std::same_as<inplace_trivial, std::decay_t<T>> && std::is_trivial_v<std::decay_t<T>> &&
    base_t::template is_storable<T>;

public:
  inplace_trivial() noexcept : base_t{} {}

  inplace_trivial(std::nullptr_t) noexcept : inplace_trivial{} {}

  template<typename T, typename... Args>
  inplace_trivial(std::in_place_type_t<T> tag, Args&&... args) noexcept(
    std::is_nothrow_constructible_v<std::decay_t<T>, Args...>)
  requires(_can_store<T>)
      : base_t{tag} {
    base_t::template _construct<T>(std::forward<Args>(args)...);
  }

  template<typename T, typename U, typename... Args>
  inplace_trivial(
    std::in_place_type_t<T> tag, std::initializer_list<U> il,
    Args&&... args) noexcept(std::is_nothrow_constructible_v<std::decay_t<T>,
                                                             std::initializer_list<U>, Args...>)
  requires(_can_store<T>)
      : base_t{tag} {
    base_t::template _construct<T>(il, std::forward<Args>(args)...);
  }

  template<typename T>
  inplace_trivial(T&& obj) noexcept(meta::is_nothrow_forward_constructible<T>)
  requires(_can_store<T>)
      : base_t{std::in_place_type_t<std::decay_t<T>>{}} {
    base_t::template _construct<T>(std::forward<T>(obj));
  }

  inplace_trivial(const inplace_trivial& other) noexcept : base_t{other._type_id} {
    _copy_from(other._storage);
  }

public:
  template<typename T, typename... Args>
  std::decay_t<T>&
  emplace(Args&&... args) noexcept(std::is_nothrow_constructible_v<std::decay_t<T>, Args...>)
  requires(_can_store<T>)
  {
    this->_type_id = base_t::template _id_from<T>();
    return base_t::template _construct<T>(std::forward<Args>(args)...);
  }

  template<typename T, typename U, typename... Args>
  std::decay_t<T>& emplace(std::initializer_list<U> il, Args&&... args) noexcept(
    std::is_nothrow_constructible_v<std::decay_t<T>, std::initializer_list<U>, Args...>)
  requires(_can_store<T>)
  {
    this->_type_id = base_t::template _id_from<T>();
    return base_t::template _construct<T>(il, std::forward<Args>(args)...);
  }

public:
  template<typename T>
  inplace_trivial& operator=(T&& obj) noexcept(meta::is_nothrow_forward_constructible<T>)
  requires(_can_store<T>)
  {
    this->_type_id = base_t::template _id_from<T>();
    base_t::template _construct<T>(std::forward<T>(obj));
    return *this;
  }

  inplace_trivial& operator=(const inplace_trivial& other) noexcept {
    if (std::addressof(other) == this) {
      return *this;
    }
    _copy_from(other._storage);
    this->_type_id = other._type_id;
    return *this;
  }

  inplace_trivial& operator=(std::nullptr_t) noexcept {
    this->_type_id = meta::NULL_TYPE_ID;
    return *this;
  }

public:
  bool empty() const noexcept { return !base_t::_has_object(); }

private:
  void _copy_from(const uint8* other) noexcept {
    std::memcpy(static_cast<void*>(this->_storage), other, buff_sz);
  }
};

namespace impl {

template<typename Derived, size_t buff_sz, move_policy policy, size_t max_align>
struct inplace_nontrivial : public inplace_storage<buff_sz, max_align> {
protected:
  static constexpr bool _can_copy = check_policy(policy, move_policy::copyable);
  static constexpr bool _can_move = check_policy(policy, move_policy::movable);

  template<typename T>
  static constexpr bool _can_move_type = std::copy_constructible<std::decay_t<T>> && _can_move;

  template<typename T>
  static constexpr bool _can_copy_type = std::move_constructible<std::decay_t<T>> && _can_copy;

  template<typename T>
  static constexpr bool _can_forward_type = (std::is_rvalue_reference_v<T> && _can_move_type<T>) ||
                                            (std::is_lvalue_reference_v<T> && _can_copy_type<T>);

  template<typename T>
  static constexpr bool _can_store_type =
    !std::same_as<Derived, std::decay_t<T>> &&
    inplace_storage<buff_sz, max_align>::template is_storable<std::decay_t<T>> &&
    std::is_nothrow_destructible_v<std::decay_t<T>>;

  static constexpr uint8 CALL_DESTROY = 0;
  static constexpr uint8 CALL_COPY_CONSTRUCT = 1;
  static constexpr uint8 CALL_MOVE_CONSTRUCT = 2;

  using call_dispatcher_t = void (*)(uint8*, uint8, uint8*, const uint8*);

  template<typename T>
  static void _dispatcher_for(uint8* buffer, uint8 call, uint8* move_other,
                              const uint8* copy_other) {
    using decayed_t = std::decay_t<T>;
    auto* obj = std::launder(reinterpret_cast<decayed_t*>(buffer));
    switch (call) {
      case CALL_DESTROY: {
        std::destroy_at(obj);
        break;
      }
      case CALL_COPY_CONSTRUCT: {
        if constexpr (check_policy(policy, move_policy::copyable)) {
          const auto* other = std::launder(reinterpret_cast<const decayed_t*>(copy_other));
          new (obj) decayed_t(*other);
        } else {
          NTF_UNREACHABLE();
        }
        break;
      }
      case CALL_MOVE_CONSTRUCT: {
        if constexpr (check_policy(policy, move_policy::movable)) {
          auto* other = std::launder(reinterpret_cast<decayed_t*>(move_other));
          new (obj) decayed_t(std::move(*other));
        } else {
          NTF_UNREACHABLE();
        }
        break;
      }
      default:
        NTF_UNREACHABLE();
    }
  }

  void _destroy() noexcept {
    if (inplace_storage<buff_sz, max_align>::_has_object()) {
      auto& self = static_cast<Derived&>(*this);
      std::invoke(self._dispatcher, this->_storage, CALL_DESTROY, nullptr, nullptr);
    }
  }

  void _copy_from(const uint8* other) {
    auto& self = static_cast<Derived&>(*this);
    std::invoke(self._dispatcher, this->_storage, CALL_COPY_CONSTRUCT, nullptr, other);
  }

  void _move_from(uint8* other) {
    auto& self = static_cast<Derived&>(*this);
    std::invoke(self._dispatcher, this->_storage, CALL_MOVE_CONSTRUCT, other, nullptr);
  }

protected:
  using inplace_storage<buff_sz, max_align>::inplace_storage;
};

} // namespace impl

template<size_t buff_sz, move_policy policy = move_policy::copyable,
         size_t max_align = alignof(std::max_align_t)>
class inplace_any :
    public impl::inplace_nontrivial<inplace_any<buff_sz, policy, max_align>, buff_sz, policy,
                                    max_align> {
private:
  using base_t =
    impl::inplace_nontrivial<inplace_any<buff_sz, policy, max_align>, buff_sz, policy, max_align>;
  friend base_t;

public:
  inplace_any() noexcept : base_t{}, _dispatcher{nullptr} {}

  inplace_any(std::nullptr_t) noexcept : inplace_any{} {}

  template<typename T, typename... Args>
  inplace_any(std::in_place_type_t<T> tag,
              Args&&... args) noexcept(std::is_nothrow_constructible_v<std::decay_t<T>, Args...>)
  requires(base_t::template _can_store_type<T>)
      : base_t{tag}, _dispatcher{&base_t::template _dispatcher_for<T>} {
    base_t::template _construct<T>(std::forward<Args>(args)...);
  }

  template<typename T, typename U, typename... Args>
  inplace_any(std::in_place_type_t<T> tag, std::initializer_list<U> il, Args&&... args) noexcept(
    std::is_nothrow_constructible_v<std::decay_t<T>, std::initializer_list<U>, Args...>)
  requires(base_t::template _can_store_type<T>)
      : base_t{tag}, _dispatcher{&base_t::template _dispatcher_for<T>} {
    base_t::template _construct<T>(il, std::forward<Args>(args)...);
  }

  template<typename T>
  inplace_any(T&& obj) noexcept(meta::is_nothrow_forward_constructible<T>)
  requires(base_t::template _can_store_type<T> && base_t::template _can_forward_type<T>)
      : base_t{std::in_place_type_t<T>{}}, _dispatcher{&base_t::template _dispatcher_for<T>} {
    base_t::template _construct<T>(std::forward<T>(obj));
  }

  inplace_any(const inplace_any& other)
  requires(base_t::_can_copy)
      : base_t{other._type_id}, _dispatcher{other._dispatcher} {
    if (!other.empty()) {
      base_t::_copy_from(other._storage);
    }
  }

  inplace_any(inplace_any&& other)
  requires(base_t::_can_move)
      : base_t{other._type_id}, _dispatcher{other._dispatcher} {
    if (!other.empty()) {
      base_t::_move_from(other._storage);
    }
  }

  ~inplace_any() noexcept { base_t::_destroy(); }

public:
  template<typename T, typename... Args>
  std::decay_t<T>&
  emplace(Args&&... args) noexcept(std::is_nothrow_constructible_v<std::decay_t<T>, Args...>)
  requires(base_t::template _can_store_type<T>)
  {
    base_t::_destroy();
    _set_meta<T>();
    try {
      return base_t::template _construct<T>(std::forward<Args>(args)...);
    } catch (...) {
      _nullify();
      NTF_RETHROW();
    }
  }

  template<typename T, typename U, typename... Args>
  std::decay_t<T>& emplace(std::initializer_list<U> il, Args&&... args) noexcept(
    std::is_nothrow_constructible_v<std::decay_t<T>, std::initializer_list<U>, Args...>)
  requires(base_t::template _can_store_type<T>)
  {
    base_t::_destroy();
    _set_meta<T>();
    try {
      return base_t::template _construct<T>(il, std::forward<Args>(args)...);
    } catch (...) {
      _nullify();
      NTF_RETHROW();
    }
  }

  template<typename T>
  inplace_any& operator=(T&& obj) noexcept(meta::is_nothrow_forward_constructible<T>)
  requires(base_t::template _can_store_type<T>)
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

  inplace_any& operator=(const inplace_any& other)
  requires(base_t::_can_copy)
  {
    if (std::addressof(other) == this) {
      return *this;
    }

    base_t::_destroy();
    _set_meta(other);

    if (!other.empty()) {
      try {
        base_t::_copy_from(other._storage);
      } catch (...) {
        _nullify();
        NTF_RETHROW();
      }
    }

    return *this;
  }

  inplace_any& operator=(inplace_any&& other)
  requires(base_t::_can_move)
  {
    if (std::addressof(other) == this) {
      return *this;
    }

    base_t::_destroy();
    _set_meta(other);

    if (!other.empty()) {
      try {
        base_t::_move_from(other._storage);
      } catch (...) {
        _nullify();
        NTF_RETHROW();
      }
    }

    return *this;
  }

public:
  bool empty() const noexcept { return !base_t::_has_object(); }

private:
  void _nullify() noexcept {
    _dispatcher = nullptr;
    this->_type_id = meta::NULL_TYPE_ID;
  }

  void _set_meta(const inplace_any& other) noexcept {
    _dispatcher = other._dispatcher;
    this->_type_id = other._type_id;
  }

  template<typename T>
  void _set_meta() noexcept {
    _dispatcher = &base_t::template _dispatcher_for<T>;
    this->_type_id = base_t::template _id_from<T>();
  }

private:
  base_t::call_dispatcher_t _dispatcher;
};

} // namespace ntf
