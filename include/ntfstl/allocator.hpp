#pragma once

#include <ntfstl/any.hpp>
#include <ntfstl/memory_pool.hpp>

namespace ntf {

namespace meta {

template<typename Alloc, typename T>
concept allocator_type = requires(Alloc& alloc, std::add_pointer_t<T> ptr, size_t n) {
  { alloc.allocate(n) } -> std::convertible_to<std::add_pointer_t<T>>;
  { alloc.deallocate(ptr, n) } -> std::same_as<void>;
  requires std::same_as<typename Alloc::value_type, T>;
};

// Avoid using std::allocator_traits for templates with non-type arguments
template<typename Alloc, typename T>
struct rebind_alloc;

template<typename Alloc, typename T>
requires requires() { typename Alloc::template rebind<T>; }
struct rebind_alloc<Alloc, T> {
  using type = typename Alloc::template rebind<T>;
};

template<typename Alloc, typename T>
struct rebind_alloc : public rebind_first_arg<Alloc, T> {};

template<typename Alloc, typename T>
using rebind_alloc_t = typename rebind_alloc<Alloc, T>::type;

template<typename Deleter, typename T>
struct rebind_deleter;

template<typename Deleter, typename T>
requires requires() { typename Deleter::template rebind<T>; }
struct rebind_deleter<Deleter, T> {
  using type = typename Deleter::template rebind<T>;
};

template<typename Deleter, typename T>
struct rebind_deleter : public rebind_first_arg<Deleter, T> {};

template<typename Deleter, typename T>
using rebind_deleter_t = rebind_deleter<Deleter, T>::type;

} // namespace meta

namespace impl {

template<typename T, typename Alloc>
struct alloc_del_dealloc : private Alloc {
  alloc_del_dealloc() :
    Alloc{} {}

  alloc_del_dealloc(const Alloc& alloc) :
    Alloc{alloc} {}

  alloc_del_dealloc(Alloc&& alloc) :
    Alloc{std::move(alloc)} {}

  void _dealloc(T* ptr, size_t sz) {
    Alloc::deallocate(ptr, sz);
  }

  Alloc& _get_allocator() { return static_cast<Alloc&>(*this); }
  const Alloc& _get_allocator() const { return static_cast<const Alloc&>(*this); }
};

} // namespace impl

template<meta::allocable_type T, meta::allocator_type<T> Alloc>
class allocator_delete : private impl::alloc_del_dealloc<T, Alloc> {
public:
  template<typename U>
  using rebind = allocator_delete<U, meta::rebind_alloc_t<Alloc, U>>;

private:
  using deall_base = impl::alloc_del_dealloc<T, Alloc>;

  template<meta::allocable_type U, meta::allocator_type<U> AllocU>
  friend class allocator_delete;

public:
  allocator_delete()
  noexcept(std::is_nothrow_default_constructible_v<Alloc>) :
    deall_base{} {}

  allocator_delete(Alloc&& alloc)
  noexcept(std::is_nothrow_move_constructible_v<Alloc>) :
    deall_base{std::move(alloc)} {}

  allocator_delete(const Alloc& alloc)
  noexcept(std::is_nothrow_copy_constructible_v<Alloc>):
    deall_base{alloc} {}

  allocator_delete(const allocator_delete& other)
  noexcept(std::is_nothrow_copy_constructible_v<Alloc>) :
    deall_base{other.get_allocator()} {}

  template<typename U>
  requires(!std::same_as<T, U>)
  allocator_delete(const rebind<U>& other)
  noexcept(std::is_nothrow_copy_constructible_v<Alloc>) :
    deall_base{other.get_allocator()} {}

public:
  template<typename U = T>
  requires(std::convertible_to<T*, U*>)
  void operator()(U* ptr)
  noexcept(std::is_nothrow_destructible_v<T>)
  {
    static_assert(meta::is_complete<T>, "Cannot destroy incomplete type");
    if constexpr (!std::is_trivially_destructible_v<T>) {
      std::destroy_at(ptr);
    }
    deall_base::_dealloc(ptr, 1u);
  }

  template<typename U = T>
  requires(std::convertible_to<T*, U*>)
  void operator()(U* ptr, size_t n)
  noexcept(std::is_nothrow_destructible_v<T>)
  {
    static_assert(meta::is_complete<T>, "Cannot destroy incomplete type");
    if constexpr (!std::is_trivially_destructible_v<T>) {
      std::destroy_n(ptr, n);
    }
    deall_base::_dealloc(ptr, n);
  }

  Alloc& get_allocator() noexcept { return deall_base::_get_allocator(); }
  const Alloc& get_allocator() const noexcept { return deall_base::_get_allocator(); }
};

template<typename T>
using default_alloc_del = allocator_delete<T, std::allocator<T>>;
template<
  meta::allocable_type T,
  size_t buff_sz = sizeof(malloc_funcs),
  move_policy policy = move_policy::copyable,
  size_t max_align = alignof(malloc_funcs)
>
class virtual_allocator
  : public impl::inplace_nontrivial<
    virtual_allocator<T, buff_sz, policy, max_align>,
    buff_sz, policy, max_align
  >
{
private:
  using base_t = impl::inplace_nontrivial<
    virtual_allocator<T, buff_sz, policy, max_align>,
    buff_sz, policy, max_align
  >;
  friend base_t;

public:
  using value_type = T;
  using pointer = T*;
  using size_type = size_t;
  using difference_type = ptrdiff_t;

  template<typename U>
  using rebind = virtual_allocator<U, buff_sz, policy, max_align>;

  template<meta::allocable_type U, size_t, move_policy, size_t>
  friend class virtual_allocator;

private:
  template<typename U>
  static constexpr bool _valid_pool = ntf::meta::allocator_pool_type<std::decay_t<U>>;

  using allocator_call_t = void*(*)(uint8*, void*, size_t, size_t);

  template<typename U>
  static void* _caller_for(uint8* buff, void* ptr, size_t sz, size_t align) {
    auto& alloc = *std::launder(reinterpret_cast<std::decay_t<U>*>(buff));
    if (ptr) {
      alloc.deallocate(ptr, sz);
    } else {
      ptr = alloc.allocate(sz, align);
    }
    return ptr;
  }

public:
  template<typename U, typename... Args>
  virtual_allocator(std::in_place_type_t<U> tag, Args&&... args)
  noexcept(std::is_nothrow_constructible_v<std::decay_t<U>, Args...>)
  requires(_valid_pool<U> && base_t::template _can_store_type<U>) :
    base_t{tag},
    _dispatcher{&base_t::template _dispatcher_for<U>},
    _alloc_call{&_caller_for<U>}
  {
    base_t::template _construct<U>(std::forward<Args>(args)...);
  }

  template<typename U, typename V, typename... Args>
  virtual_allocator(std::in_place_type_t<U> tag, std::initializer_list<V> il, Args&&... args)
  noexcept(std::is_nothrow_constructible_v<std::decay_t<U>, std::initializer_list<V>, Args...>)
  requires(_valid_pool<U> && base_t::template _can_store_type<U>) :
    base_t{tag},
    _dispatcher{&base_t::template _dispatcher_for<U>},
    _alloc_call{&_caller_for<U>}
  {
    base_t::template _construct<U>(il, std::forward<Args>(args)...);
  }

  template<typename U>
  virtual_allocator(U&& obj)
  noexcept(meta::is_nothrow_forward_constructible<U>)
  requires(_valid_pool<U> && base_t::template _can_store_type<U> &&
           base_t::template _can_forward_type<U>) :
    base_t{std::in_place_type_t<U>{}},
    _dispatcher{&base_t::template _dispatcher_for<U>},
    _alloc_call{&_caller_for<U>}
  {
    base_t::template _construct<U>(std::forward<U>(obj));
  }

  template<typename U>
  virtual_allocator(const rebind<U>& other)
  requires(!std::same_as<T, std::decay_t<U>> && base_t::_can_copy) :
    base_t{other._type_id},
    _dispatcher{other._dispatcher},
    _alloc_call{other._alloc_call}
  {
    base_t::_copy_from(other._storage);
  }

  template<typename U>
  virtual_allocator(rebind<U>&& other)
  requires(!std::same_as<T, std::decay_t<U>> && base_t::_can_move) :
    base_t{other._type_id},
    _dispatcher{other._dispatcher},
    _alloc_call{other._alloc_call}
  {
    base_t::_move_from(other._storage);
  }

  virtual_allocator(const virtual_allocator& other)
  requires(base_t::_can_copy) :
    base_t{other._type_id},
    _dispatcher{other._dispatcher},
    _alloc_call{other._alloc_call}
  {
    base_t::_copy_from(other._storage);
  }

  virtual_allocator(virtual_allocator&& other)
  requires(base_t::_can_move) :
    base_t{other._type_id},
    _dispatcher{other._dispatcher},
    _alloc_call{other._alloc_call}
  {
    base_t::_move_from(other._storage);
  }

  ~virtual_allocator() noexcept { base_t::_destroy(); }

public:
  template<typename U, typename... Args>
  std::decay_t<U>& emplace(Args&&... args)
  noexcept(std::is_nothrow_constructible_v<std::decay_t<U>, Args...>)
  requires(_valid_pool<U> && base_t::template _can_store_type<U>)
  {
    base_t::_destroy();
    _set_meta<U>();
    try {
      return base_t::template _construct<U>(std::forward<Args>(args)...);
    } catch(...) {
      _nullify();
      NTF_RETHROW();
    }
  }

  template<typename U, typename V, typename... Args>
  std::decay_t<U>& emplace(std::initializer_list<V> li, Args&&... args)
  noexcept(std::is_nothrow_constructible_v<std::decay_t<U>, std::initializer_list<V>, Args...>)
  requires(_valid_pool<U> && base_t::template _can_store_type<U>)
  {
    base_t::_destroy();
    _set_meta<U>();
    try {
      return base_t::template _construct<U>(li, std::forward<Args>(args)...);
    } catch(...) {
      _nullify();
      NTF_RETHROW();
    }
  }

  template<typename U>
  virtual_allocator& operator=(U&& obj)
  noexcept(meta::is_nothrow_forward_constructible<U>)
  requires(_valid_pool<U> && base_t::template _can_store_type<U>)
  {
    base_t::_destroy();
    _set_meta<U>();
    try {
      base_t::template _construct<U>(std::forward<U>(obj));
    } catch(...) {
      _nullify();
      NTF_RETHROW();
    }
    return *this;
  }

  virtual_allocator& operator=(const virtual_allocator& other)
  requires(base_t::_can_copy)
  {
    if (std::addressof(other) == this) {
      return *this;
    }
    base_t::_destroy();
    _set_meta(other);
    try {
      base_t::_copy_from(other._storage);
    } catch(...) {
      _nullify();
      NTF_RETHROW();
    }
    return *this;
  }

  virtual_allocator& operator=(virtual_allocator&& other)
  requires(base_t::_can_move)
  {
    if (std::addressof(other) == this) {
      return *this;
    }
    base_t::_destroy();
    _set_meta(other);
    try {
      base_t::_copy_from(other._storage);
    } catch(...) {
      _nullify();
      NTF_RETHROW();
    }
    return *this;
  }

  template<typename U>
  virtual_allocator& operator=(const rebind<U>& other)
  requires(!std::same_as<T, std::decay_t<U>> && base_t::_can_copy)
  {
    base_t::_destroy();
    _set_meta(other);
    try {
      base_t::_copy_from(other._storage);
    } catch(...) {
      _nullify();
      NTF_RETHROW();
    }
    return *this;
  }

  template<typename U>
  virtual_allocator& operator=(rebind<U>&& other)
  requires(!std::same_as<T, std::decay_t<U>> && base_t::_can_move)
  {
    base_t::_destroy();
    _set_meta(other);
    try {
      base_t::_copy_from(other._storage);
    } catch(...) {
      _nullify();
      NTF_RETHROW();
    }
    return *this;
  }

public:
  pointer allocate(size_type n) {
    NTF_ASSERT(!valueless_by_exception(), "Valueless allocator");
    void* ptr = std::invoke(_alloc_call, this->_storage,
                            nullptr, n*sizeof(value_type), alignof(value_type));
    NTF_THROW_IF(!ptr, std::bad_alloc);
    return reinterpret_cast<pointer>(ptr);
  }

  void deallocate(pointer ptr, size_type n) {
    NTF_ASSERT(!valueless_by_exception(), "Valueless allocator");
    std::invoke(_alloc_call, this->_storage,
                reinterpret_cast<void*>(ptr), n*sizeof(value_type), alignof(value_type));
  }

  template<typename... Args>
  pointer construct(Args&&... args) {
    auto* ptr = allocate(1u);
    new (ptr) T(std::forward<Args>(args)...);
    return ptr;
  }

  template<typename U, typename... Args>
  pointer construct(std::initializer_list<U> il, Args&&... args) {
    auto* ptr = allocate(1u);
    new (ptr) T(il, std::forward<Args>(args)...);
    return ptr;
  }

  void destroy(pointer ptr) {
    ptr->~T();
    deallocate(ptr, 1u);
  }

public:
  bool valueless_by_exception() const noexcept { return _alloc_call == nullptr; }

private:
  template<typename U>
  void _set_meta() noexcept {
    _alloc_call = &_caller_for<U>;
    _dispatcher = &base_t::template _dispatcher_for<U>;
    this->_type_id = base_t::template _id_from<U>();
  }

  template<typename U>
  void _set_meta(const virtual_allocator<U, buff_sz, policy, max_align>& other) noexcept {
    _alloc_call = other._alloc_call;
    _dispatcher = other._dispatcher;
    this->_type_id = other._type_id;
  }

  void _nullify() noexcept {
    _alloc_call = nullptr;
    _dispatcher = nullptr;
    this->_type_id = meta::NULL_TYPE_ID;
  }

private:
  base_t::call_dispatcher_t _dispatcher;
  allocator_call_t _alloc_call;
};

} // namespace ntf
