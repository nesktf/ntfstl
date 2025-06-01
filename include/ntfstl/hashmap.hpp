#pragma once

#include <ntfstl/allocator.hpp>
#include <ntfstl/expected.hpp>

namespace ntf {

namespace impl {

using fhashmap_flag = uint32;
constexpr size_t FHASHMAP_FLAGS_PER_ENTRY  = 16; // 2 bits each, uses 32 bits
constexpr fhashmap_flag FHASHMAP_EMPTY_FLAG = 0b00;
constexpr fhashmap_flag FHASHMAP_USED_FLAG  = 0b01;
constexpr fhashmap_flag FHASHMAP_TOMB_FLAG  = 0b10;
constexpr fhashmap_flag FHASHMAP_FLAG_MASK  = 0b11;

constexpr size_t fhashamp_flag_count(size_t capacity) {
  return std::ceil(static_cast<f32>(capacity)/static_cast<f32>(FHASHMAP_FLAGS_PER_ENTRY));
}

template<typename MapT, typename ValT>
class fhashmap_forward_it {
private:
  friend MapT;

public:
  using difference_type = std::ptrdiff_t;
  using value_type = ValT;
  using pointer = value_type*;
  using reference = value_type&;
  using iterator_category = std::forward_iterator_tag;

public:
  bool operator==(const fhashmap_forward_it& other) const {
    return other._idx == _idx && other._map == _map;
  }

  bool operator!=(const fhashmap_forward_it& other) const {
    return !(other == *this);
  }

  fhashmap_forward_it& operator++() {
    ++_idx;
    _next_valid();
    return *this;
  }

  reference operator*() const { return _map->_values[_idx]; }
  pointer operator->() const { return _map->_values+_idx; }

private:
  explicit fhashmap_forward_it(MapT* map, size_t idx) :
    _map{map}, _idx{idx} { _next_valid(); }

  void _next_valid() {
    while (_idx < _map->capacity() && _map->_flag_at(_idx) != FHASHMAP_USED_FLAG) {
      ++_idx;
    }
  }

private:
  MapT* _map;
  size_t _idx;
};

template<typename K, typename HashT, typename EqualsT>
struct fixed_hashmap_ops : 
  private HashT, private EqualsT
{
  fixed_hashmap_ops() :
    HashT{}, EqualsT{} {}

  fixed_hashmap_ops(const HashT& hash, const EqualsT& equals) :
    HashT{hash}, EqualsT{equals} {}

  bool _equals(const K& a, const K& b) const {
    return EqualsT::operator()(a, b);
  }

  EqualsT& _key_eq() { return static_cast<EqualsT&>(*this); }
  const EqualsT& _key_eq() const { return static_cast<const EqualsT&>(*this); }

  size_t _hash(const K& x) const {
    return HashT::operator()(x);
  }

  HashT& _hash_function() { return static_cast<HashT&>(*this); }
  const HashT& _hash_function() const { return static_cast<const HashT&>(*this); }
};

template<typename K, typename T, typename Del>
struct fixed_hashmap_del :
  private Del, private meta::rebind_deleter_t<Del, fhashmap_flag>
{
private:
  using value_base_t = Del;
  using flag_base_t = meta::rebind_deleter_t<Del, fhashmap_flag>;

public:
  fixed_hashmap_del() :
    value_base_t{}, flag_base_t{} {}

  template<typename Alloc>
  fixed_hashmap_del(in_place_t, const Alloc& alloc) :
    value_base_t{alloc}, flag_base_t{meta::rebind_alloc_t<Alloc, fhashmap_flag>{alloc}} {}

  fixed_hashmap_del(const Del& del) :
    value_base_t{del}, flag_base_t{del} {}

  void _destroy(std::pair<const K, T>* values, size_t value_count,
                fhashmap_flag* flags, size_t flag_count) {
    value_base_t::operator()(values, value_count);
    flag_base_t::operator()(flags, flag_count);
  }

  Del& _get_deleter() { return static_cast<Del&>(*this); }
  const Del& _get_deleter() const { return static_cast<const Del&>(*this); }
};

} // namespace impl

template<
  typename K, typename T,
  typename HashT = std::hash<K>,
  typename EqualsT = std::equal_to<K>,
  meta::array_deleter_type<std::pair<const K, T>> DelT = default_alloc_del<std::pair<const K, T>>
>
class fixed_hashmap :
  private impl::fixed_hashmap_del<K, T, DelT>,
  private impl::fixed_hashmap_ops<K, HashT, EqualsT>
{
public:
  using key_type = K;
  using mapped_type = T;

  using value_type = std::pair<const key_type, mapped_type>;
  using flags_type = impl::fhashmap_flag;

  using hasher = HashT;
  using key_equal = EqualsT;
  using deleter_type = DelT;

  using size_type = std::size_t;
  using reference = value_type&;
  using const_reference = const value_type&;

  using iterator = impl::fhashmap_forward_it<fixed_hashmap, value_type>;
  using const_iterator = impl::fhashmap_forward_it<const fixed_hashmap, const value_type>;

private:
  static constexpr auto FLAG_USED = impl::FHASHMAP_USED_FLAG;
  static constexpr auto FLAG_TOMB = impl::FHASHMAP_TOMB_FLAG;
  static constexpr auto FLAG_EMPTY = impl::FHASHMAP_EMPTY_FLAG;
  static constexpr auto FLAG_MASK = impl::FHASHMAP_FLAG_MASK;
  static constexpr auto FLAGS_PER_ENTRY = impl::FHASHMAP_FLAGS_PER_ENTRY;

  friend iterator;
  friend const_iterator;

  using del_base_t = impl::fixed_hashmap_del<K, T, DelT>;
  using ops_base_t = impl::fixed_hashmap_ops<K, HashT, EqualsT>;

  template<typename Alloc>
  using alloc_del = allocator_delete<value_type, std::decay_t<Alloc>>;

  template<typename Alloc, typename Hash, typename Equals>
  using alloc_ret = expected<fixed_hashmap<K, T, Hash, Equals, alloc_del<Alloc>>, std::bad_alloc>;

public:
  fixed_hashmap(value_type* values, flags_type* flags, size_t cap, size_t used) :
    del_base_t{}, ops_base_t{},
    _values{values}, _flags{flags}, _used{used}, _cap{cap} {}

  fixed_hashmap(value_type* values, flags_type* flags, size_t cap, size_t used,
                const DelT& del, const HashT& hash, const EqualsT& eqs) :
    del_base_t{del}, ops_base_t{hash, eqs},
    _values{values}, _flags{flags}, _used{used}, _cap{cap} {}

  template<
    typename Alloc = std::allocator<value_type>,
    typename Hash = HashT, typename Equals = EqualsT
  >
  requires(meta::allocator_type<std::decay_t<Alloc>, value_type>)
  fixed_hashmap(size_type size, Alloc&& alloc = {},
                const Hash& hash = {}, const Equals& eqs = {}) :
    del_base_t{in_place, alloc}, ops_base_t{hash, eqs},
    _values{nullptr}, _flags{nullptr}, _used{0u}, _cap{size}
  {
    size_type flag_sz = impl::fhashamp_flag_count(size);
    try {
      _values = alloc.allocate(size);
      NTF_THROW_IF(!_values, std::bad_alloc);
      meta::rebind_alloc_t<std::decay_t<Alloc>, flags_type> flag_alloc{alloc};
      _flags = flag_alloc.allocate(flag_sz);
      if (!_flags) {
        alloc.deallocate(_values, size);
        NTF_THROW(std::bad_alloc);
      }
    } catch (...){
      NTF_RETHROW();
    }
    std::memset(_flags, 0, flag_sz*sizeof(uint32));
  }

  template<
    typename Alloc = std::allocator<value_type>,
    typename Hash = HashT, typename Equals = EqualsT
  >
  requires(meta::allocator_type<std::decay_t<Alloc>, value_type>)
  fixed_hashmap(std::initializer_list<value_type> il, Alloc&& alloc = {},
                const Hash& hash = {}, const Equals& eqs = {}) :
    fixed_hashmap{il.size(), std::forward<Alloc>(alloc), hash, eqs}
  {
    for (const auto& [k, v] : il) {
      auto [_, empl] = try_emplace(k, v);
      NTF_THROW_IF(!empl, std::bad_alloc);
    }
  }

  fixed_hashmap(fixed_hashmap&& other) noexcept :
    del_base_t{static_cast<del_base_t&&>(other)},
    ops_base_t{static_cast<ops_base_t&&>(other)},
    _values{std::move(other._values)}, _flags{std::move(other._flags)}, 
    _used{std::move(other._used)}, _cap{std::move(other._cap)} { other._values = nullptr; }

  fixed_hashmap(const fixed_hashmap&) = delete;

  ~fixed_hashmap() noexcept { _destroy(); }

public:
  template<
    typename Alloc = std::allocator<value_type>,
    typename Hash = HashT, typename Equals = EqualsT
  >
  requires(meta::allocator_type<std::decay_t<Alloc>, value_type>)
  static alloc_ret<Alloc, Hash, Equals> from_size(size_type size,
                                                  Alloc&& alloc = {},
                                                  const Hash& hash = {},
                                                  const Equals& eqs = {})
  {
    size_type flag_sz = impl::fhashamp_flag_count(size);
    value_type* array;
    flags_type* flags;
    try {
      array = alloc.allocate(size);
      if (!array) {
        return unexpected{std::bad_alloc{}};
      }

      meta::rebind_alloc_t<std::decay_t<Alloc>, flags_type> flag_alloc{alloc};
      flags = flag_alloc.allocate(flag_sz);
      if (!flags){
        alloc.deallocate(array, size);
        return unexpected{std::bad_alloc{}};
      }
    } catch (...) {
      alloc.deallocate(array, size); // If flag_alloc throws anywhere, no flag was allocated
      return unexpected{std::bad_alloc{}};
    }
    std::memset(flags, 0, flag_sz*sizeof(uint32));
    return alloc_ret<Alloc, Hash, Equals> {
      in_place, array, flags, size, 0u, alloc_del<Alloc>{std::forward<Alloc>(alloc)}, hash, eqs
    };
  }

  template<
    typename Alloc = std::allocator<value_type>,
    typename Hash = HashT, typename Equals = EqualsT
  >
  requires(meta::allocator_type<std::decay_t<Alloc>, value_type> &&
           std::copy_constructible<K> && std::copy_constructible<T>)
  static alloc_ret<Alloc, Hash, Equals> from_initializer(std::initializer_list<value_type> il,
                                                         Alloc&& alloc = {},
                                                         const Hash& hash = {},
                                                         const Equals& eqs = {})
  {
    return from_size(il.size(), std::forward<Alloc>(alloc), hash, eqs)
    .and_then([&](auto&& arr) -> alloc_ret<Alloc, Hash, Equals> {
      try {
        for (const auto& [k, v] : il) {
          auto [_, empl] = arr.try_emplace(k, v);
          if (!empl) {
            return unexpected{std::bad_alloc{}};
          }
        }
      } catch (...) {
        return unexpected{std::bad_alloc{}};
      }
      return arr;
    });
  }

public:
  template<typename Key, typename... Args>
  std::pair<iterator, bool> try_emplace(Key&& key, Args&&... args) {
    size_t idx = ops_base_t::_hash(key) % capacity();
    for (size_t i = 0; i < capacity(); ++i, idx = (idx+1)%capacity()) {
      if (_flag_at(idx) != FLAG_USED) {
        std::construct_at(_values+idx,
                          std::make_pair(std::forward<Key>(key), T{std::forward<Args>(args)...}));
        ++_used;
        _flag_set(idx, FLAG_USED);
        return std::make_pair(iterator{this, idx}, true);
      }
    }
    return std::make_pair(end(), false);
  }

  template<typename Key, typename U = T>
  std::pair<iterator, bool> try_overwrite(Key&& key, U&& obj) {
    size_t idx = ops_base_t::_hash(key) % capacity();
    for (size_t i = 0; i < capacity(); ++i, idx = (idx+1)%capacity()) {
      const auto flag = _flag_at(idx);
      if (flag == FLAG_EMPTY || flag == FLAG_TOMB) {
        std::construct_at(_values+idx,
                          std::make_pair(std::forward<Key>(key), std::forward<U>(obj)));
        ++_used;
        _flag_set(idx, FLAG_USED);
        return std::make_pair(iterator{this, idx}, true);
      }
      if (flag == FLAG_USED) {
        const key_type k(std::forward<Key>(key));
        if (ops_base_t::_equals(_values[idx].first, k)) {
          _values[idx].second = std::forward<U>(obj);
          return std::make_pair(iterator{this, idx}, true);
        }
      }
    }
    return std::make_pair(end(), false);
  }

  bool erase(const key_type& key) {
    size_t idx = ops_base_t::_hash(key) % capacity();
    for (size_t i = 0; i < capacity(); ++i, idx = (idx+1)%capacity()) {
      const auto flag = _flag_at(idx);
      if (flag == FLAG_EMPTY) {
        return false;
      }
      if (flag == FLAG_TOMB) {
        continue;
      }
      if (ops_base_t::_equals(_values[idx].first, key)) {
        _flag_set(idx, FLAG_TOMB);
        --_used;
        if constexpr (!std::is_trivial_v<value_type>) {
          _values[idx].~value_type();
        }
        return true;
      }
    }
    return false;
  }

  [[nodiscard]] iterator find(const key_type& key) {
    size_t idx = ops_base_t::_hash(key) % capacity();
    for (size_t i = 0; i < capacity(); ++i, idx = (idx+1)%capacity()) {
      const auto flag = _flag_at(idx);
      if (flag == FLAG_EMPTY) {
        return end();
      }
      if (flag == FLAG_TOMB) {
        continue;
      }
      if (ops_base_t::_equals(_values[idx].first, key)) {
        return iterator{this, idx};
      }
    }
    return end();
  }

  [[nodiscard]] const_iterator find(const key_type& key) const {
    size_t idx = ops_base_t::_hash(key) % capacity();
    for (size_t i = 0; i < capacity(); ++i, idx = (idx+1)%capacity()) {
      const auto flag = _flag_at(idx);
      if (flag == FLAG_EMPTY) {
        return end();
      }
      if (flag == FLAG_TOMB) {
        continue;
      }
      if (ops_base_t::_equals(_values[idx].first, key)) {
        return const_iterator{this, idx};
      }
    }
    return end();
  }

  void clear() noexcept {
    if constexpr (!std::is_trivially_destructible_v<value_type>) {
      for (size_t i = 0; i < capacity(); ++i) {
        if (_flag_at(i) == FLAG_USED) {
          std::destroy_at(_values+i);
          _flag_set(i, FLAG_EMPTY);
        }
      }
    }
    _used = 0;
  }

public:
  mapped_type& at(const key_type& key) noexcept(NTF_NOEXCEPT) {
    auto it = find(key);
    NTF_THROW_IF(it == end(), std::out_of_range, "Key not found");
    return it->second;
  }

  const mapped_type& at(const key_type& key) const noexcept(NTF_NOEXCEPT) {
    auto it = find(key);
    NTF_THROW_IF(it == end(), std::out_of_range, "Key not found");
    return it->second;
  }

  template<typename F>
  fixed_hashmap& for_each(F&& fun) {
    if (_used == 0) {
      return *this;
    }
    for (size_t i = 0; i < capacity(); ++i) {
      if (_flag_at(i) == FLAG_USED) {
        std::invoke(fun, _values[i]);
      }
    }
    return *this;
  }

  template<typename F>
  const fixed_hashmap& for_each(F&& fun) const {
    if (_used == 0) {
      return *this;
    }
    for (size_t i = 0; i < capacity(); ++i) {
      if (_flag_at(i) == FLAG_USED) {
        std::invoke(fun, _values[i]);
      }
    }
    return *this;
  }

public:
  fixed_hashmap& operator=(fixed_hashmap&& other) noexcept {
    _destroy();

    ops_base_t::operator=(static_cast<ops_base_t&&>(other));
    del_base_t::operator=(static_cast<del_base_t&&>(other));

    _flags = std::move(other._flags);
    _values = std::move(other._values);
    _used = std::move(other._used);
    _cap = std::move(other._cap);

    other._values = nullptr;

    return *this;
  }

  fixed_hashmap& operator=(const fixed_hashmap&) = delete;

  mapped_type& operator[](const key_type& key) noexcept(NTF_NOEXCEPT) {
    return at(key);
  }

  const mapped_type& operator[](const key_type& key) const noexcept(NTF_NOEXCEPT) {
    return at(key);
  }

public:
  iterator begin() noexcept { return iterator{this, 0u}; }
  const_iterator begin() const noexcept { return const_iterator{this, 0u}; }
  const_iterator cbegin() const noexcept { return const_iterator{this, 0u}; }

  iterator end() noexcept { return iterator{this, capacity()}; }
  const_iterator end() const noexcept { return const_iterator{this, capacity()}; }
  const_iterator cend() const noexcept { return const_iterator{this, capacity()}; }

  size_type size() const noexcept { return _used; }
  size_type capacity() const noexcept { return _cap; }
  float load_factor() const noexcept {
    return static_cast<f32>(size())/static_cast<f32>(capacity());
  }

  hasher& hash_function() noexcept { return ops_base_t::_hash_function(); }
  const hasher& hash_function() const noexcept { return ops_base_t::_hash_function(); }

  key_equal& key_eq() noexcept { return ops_base_t::_key_eq(); }
  const key_equal& key_eq() const noexcept { return ops_base_t::_key_eq(); }

  deleter_type& deleter() noexcept { return del_base_t::_get_deleter(); }
  const deleter_type& deleter() const noexcept { return del_base_t::_get_deleter(); }

private:
  void _destroy() noexcept {
    if (!_values) {
      return;
    }

    clear();
    del_base_t::_destroy(_values, capacity(), _flags, impl::fhashamp_flag_count(capacity()));
  }

  uint32 _flag_at(size_t idx) const {
    const uint32 flag_idx = idx/FLAGS_PER_ENTRY;
    const uint32 shift = (idx%FLAGS_PER_ENTRY)*2;
    return (_flags[flag_idx] >> shift) & FLAG_MASK;
  }

  void _flag_set(size_t idx, uint32 flag) {
    const uint32 flag_idx = idx/FLAGS_PER_ENTRY;
    const uint32 shift = (idx%FLAGS_PER_ENTRY)*2;
    _flags[flag_idx] &= ~(FLAG_MASK << shift);
    _flags[flag_idx] |= (flag & FLAG_MASK) << shift;
  }

private:
  value_type* _values;
  flags_type* _flags;
  size_t _used;
  size_t _cap;
};

} // namespace ntf
