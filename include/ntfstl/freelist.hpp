#pragma once

#include <ntfstl/optional.hpp>
#include <ntfstl/span.hpp>
#include <ntfstl/types.hpp>

#include <functional>

namespace ntf {

class freelist_handle {
public:
  static constexpr u32 NULL_INDEX = 0xFFFFFFFF;
  static constexpr u32 INIT_VERSION = 0u;

public:
  constexpr freelist_handle() noexcept : _index{NULL_INDEX}, _version{} {}

  constexpr freelist_handle(u32 index, u32 version) noexcept : _index{index}, _version{version} {}

public:
  static constexpr freelist_handle from_u64(u64 handle) noexcept {
    return {static_cast<u32>(handle & 0xFFFFFFFF), static_cast<u32>(handle >> 32)};
  }

public:
  constexpr u64 as_u64() const noexcept {
    return static_cast<u64>(_version) << 32 | static_cast<u64>(_index);
  }

  constexpr u32 index() const noexcept { return _index; }

  constexpr u32 version() const noexcept { return _version; }

  constexpr bool empty() const noexcept { return _index == NULL_INDEX; }

public:
  constexpr bool operator==(const freelist_handle& other) const noexcept {
    return _index == other._index && _version == other._version;
  }

  constexpr bool operator!=(const freelist_handle& other) const noexcept {
    return !(*this == other);
  }

  constexpr explicit operator bool() noexcept { return !empty(); }

private:
  u32 _index;
  u32 _version;
};

template<>
struct optional_null<freelist_handle> {
  static constexpr freelist_handle value{};
};

static_assert(meta::optimized_optional_type<freelist_handle>);

namespace meta {

template<typename F, typename T>
concept freelist_elem_remove_pred = requires(F func, const T& elem) {
  { std::invoke(func, elem) } -> std::convertible_to<bool>;
};

template<typename F, typename T>
concept freelist_pair_remove_pred = requires(F func, const T& elem, freelist_handle handle) {
  { std::invoke(func, elem, handle) } -> std::convertible_to<bool>;
};

template<typename F, typename T>
concept freelist_remove_pred = freelist_elem_remove_pred<F, T> || freelist_pair_remove_pred<F, T>;

template<typename F, typename T>
concept freelist_sort_pred = requires(F func, const T& a, const T& b) {
  { std::invoke(func, a, b) } -> std::convertible_to<bool>;
};

template<typename F, typename T>
concept freelist_find_pred = requires(F func, const T& obj) {
  { std::invoke(func, obj) } -> std::convertible_to<bool>;
};

} // namespace meta

namespace impl {

template<typename T>
requires(std::is_nothrow_destructible_v<T>)
class freelist_slot {
private:
  using handle_type = freelist_handle;
  using pair_type = std::pair<T, handle_type>;

public:
  freelist_slot() noexcept {}; // initialized with memset or emplaced

  template<typename... Args>
  freelist_slot(u32 idx, u32 version, Args&&... args) :
      obj(std::piecewise_construct, std::forward_as_tuple(std::forward<Args>(args)...),
          std::forward_as_tuple(idx, version)),
      flag{0u} {}

  freelist_slot(const freelist_slot& other)
  requires(std::copy_constructible<T>)
  {
    if (!other.is_empty()) {
      new (&obj) pair_type{other.obj};
    }
    flag = other.flag;
  }

  ~freelist_slot() noexcept
  requires(std::is_trivially_destructible_v<T>)
  = default;

  ~freelist_slot() noexcept
  requires(!std::is_trivially_destructible_v<T>)
  {
    if (!is_empty()) {
      std::destroy_at(&obj);
    }
  }

public:
  template<typename... Args>
  void construct(u32 idx, u32 version, Args&&... args) {
    NTF_ASSERT(is_empty());
    new (&obj)
      pair_type(std::piecewise_construct, std::forward_as_tuple(std::forward<Args>(args)...),
                std::forward_as_tuple(idx, version));
    flag = 0u;
  }

  void destroy() {
    NTF_ASSERT(!is_empty());
    std::destroy_at(&obj);
    flag = handle_type::NULL_INDEX;
  }

  bool is_empty() const noexcept { return flag == handle_type::NULL_INDEX; }

public:
  union {
    pair_type obj;
    char _dummy;
  };

  u32 flag;
  u32 next;
  u32 prev;
};

template<typename T, bool is_const>
class freelist_iter {
public:
  using handle_type = freelist_handle;
  using slot_pointer = std::conditional_t<is_const, const freelist_slot<T>*, freelist_slot<T>*>;
  using value_type =
    std::conditional_t<is_const, const std::pair<T, handle_type>, std::pair<T, handle_type>>;

  using size_type = size_t;
  using difference_type = ptrdiff_t;

public:
  constexpr freelist_iter(slot_pointer slot, size_type index) noexcept :
      _begin{slot}, _index{index} {}

public:
  constexpr bool operator==(const freelist_iter& other) const noexcept {
    return _begin == other._begin && _index == other._index;
  }

  constexpr bool operator!=(const freelist_iter& other) const noexcept {
    return !(*this == other);
  }

  constexpr freelist_iter& operator++() {
    NTF_ASSERT(!_begin[_index].is_empty());
    _index = _begin[_index].next;
    return *this;
  }

  constexpr freelist_iter& operator--() {
    NTF_ASSERT(!_begin[_index].is_empty());
    _index = _begin[_index].prev;
    return *this;
  }

  constexpr freelist_iter operator++(int) {
    freelist_iter prev{_begin, _index};
    ++*this;
    return prev;
  }

  constexpr freelist_iter operator--(int) {
    freelist_iter prev{_begin, _index};
    ++*this;
    return prev;
  }

  constexpr value_type& operator*() const {
    NTF_ASSERT(!_begin[_index].is_empty());
    return _begin[_index].obj;
  }

  constexpr value_type* operator->() const {
    NTF_ASSERT(!_begin[_index].is_empty());
    return &_begin[_index].obj;
  }

public:
  constexpr handle_type to_handle() const {
    NTF_ASSERT(!_begin[_index].is_empty());
    return {_index, _begin[_index].obj.version};
  }

private:
  slot_pointer _begin;
  size_type _index;
};

template<typename T, size_t SpanExtent>
class freelist_base {
public:
  using handle_type = freelist_handle;
  using slot_type = freelist_slot<T>;
  using span_type = span<slot_type, SpanExtent>;
  using cspan_type = span<const slot_type, SpanExtent>;

public:
  template<typename... Args>
  handle_type reuse_slot(span_type slots, u32& count, u32& free_head, u32& used_back,
                         Args&&... args) const {
    const u32 idx = free_head;
    NTF_ASSERT(idx < slots.size());
    NTF_ASSERT(free_head != handle_type::NULL_INDEX);
    NTF_ASSERT(used_back != handle_type::NULL_INDEX);
    NTF_ASSERT(used_back < slots.size());

    auto& slot = slots[idx];
    const u32 version = slot.prev;

    slot.construct(idx, version, std::forward<Args>(args)...);
    free_head = slot.next;
    slot.next = handle_type::NULL_INDEX;
    slot.prev = used_back;
    slots[used_back].next = idx;
    used_back = idx;
    ++count;

    return {idx, version};
  }

  template<typename... Args>
  void return_slot(span_type slots, u32& count, u32& free_head, handle_type handle) const {
    const u32 idx = handle.index();
    if (idx >= slots.size()) {
      return;
    }
    slot_type& slot = slots[idx];
    if (slot.is_empty()) {
      return;
    }

    auto& [_, slot_handle] = slot.obj;
    const u32 version = slot_handle.version();

    slot.destroy();
    slot.next = free_head;
    slot.prev = version + 1u; // hack: store the new version in the previous slot index
    free_head = idx;
    --count;
  }

  template<meta::freelist_remove_pred<T> F>
  void return_slot_where(span_type slots, u32& count, u32& free_head, F&& pred) const {
    const auto can_remove = [&](const slot_type& slot) -> bool {
      [[maybe_unused]] const auto& [elem, handle] = slot.obj;
      if constexpr (meta::freelist_elem_remove_pred<F, T>) {
        return std::invoke(pred, elem);
      } else if constexpr (meta::freelist_pair_remove_pred<F, T>) {
        return std::invoke(pred, elem, handle);
      }
    };

    for (u32 idx = 0u; auto& slot : slots) {
      if (!slot.is_empty() && std::invoke(can_remove, slot)) {
        slot.destroy();
        slot.next = free_head;
        auto& [_, handle] = slot.obj;
        handle = {idx, handle.version() + 1};
        free_head = idx;
        --count;
      }
      ++idx;
    }
  }

  template<meta::freelist_find_pred<T> F>
  optional<handle_type> find_slot(cspan_type slots, u32 front, F&& pred) const {
    if (slots.empty()) {
      return {ntf::nullopt};
    }
    if (front == handle_type::NULL_INDEX) {
      return {ntf::nullopt};
    }
    do {
      NTF_ASSERT(front < slots.size());
      const auto& slot = slots[front];
      NTF_ASSERT(!slot.is_empty());
      const auto& [obj, handle] = slot.obj;
      if (std::invoke(pred, obj)) {
        return {ntf::in_place, handle};
      }
      front = slot.next;
    } while (front != handle_type::NULL_INDEX);
    return {ntf::nullopt};
  }

  template<meta::freelist_sort_pred<T> F>
  void sort_slots(span_type slots, F&& pred) const {
    NTF_ASSERT(false, "TODO");
  }

  bool is_valid(cspan_type slots, handle_type handle) const noexcept {
    const auto idx = handle.index();
    if (idx >= slots.size()) {
      return false;
    }
    const slot_type& slot = slots[idx];
    if (slot.is_empty()) {
      return false;
    }
    const auto& [_, slot_handle] = slot.obj;
    return slot_handle == handle;
  }

  const T& get_cref(cspan_type slots, handle_type handle) const {
    const auto idx = handle.index();
    NTF_THROW_IF(idx >= slots.size(), std::out_of_range,
                 fmt::format("Index {} out of range in handle", idx));
    const slot_type& slot = slots[idx];
    NTF_THROW_IF(slot.is_empty(), std::out_of_range,
                 fmt::format("Empty item at index {} from handle", idx));
    const auto& [obj, slot_handle] = slot.obj;
    NTF_THROW_IF(slot_handle != handle, std::out_of_range,
                 fmt::format("Invalid version {} in handle", version));
    return obj;
  }

  T& get_ref(span_type slots, handle_type handle) const {
    const auto idx = handle.index();
    NTF_THROW_IF(idx >= slots.size(), std::out_of_range,
                 fmt::format("Index {} out of range in handle", idx));
    slot_type& slot = slots[idx];
    NTF_THROW_IF(slot.is_empty(), std::out_of_range,
                 fmt::format("Empty item at index {} from handle", idx));
    auto& [obj, slot_handle] = slot.obj;
    NTF_THROW_IF(slot_handle != handle, std::out_of_range,
                 fmt::format("Invalid version {} in handle", version));
    return obj;
  }

  const T* get_cptr(cspan_type slots, handle_type handle) const noexcept {
    const auto idx = handle.index();
    if (idx >= slots.size()) {
      return nullptr;
    }
    const slot_type& slot = slots[idx];
    if (slot.is_empty()) {
      return nullptr;
    }
    const auto& [obj, slot_handle] = slot.obj;
    if (slot_handle != handle) {
      return nullptr;
    }
    return &obj;
  }

  T* get_ptr(span_type slots, handle_type handle) const noexcept {
    const auto idx = handle.index();
    if (idx >= slots.size()) {
      return nullptr;
    }
    slot_type& slot = slots[idx];
    if (slot.is_empty()) {
      return nullptr;
    }
    auto& [obj, slot_handle] = slot.obj;
    if (slot_handle != handle) {
      return nullptr;
    }
    return &obj;
  }

  const T& get_craw(cspan_type slots, handle_type handle) const {
    const auto idx = handle.index();
    NTF_ASSERT(idx < slots.size());
    const slot_type& slot = slots[idx];
    NTF_ASSERT(!slot.is_empty());
    [[maybe_unused]] const auto& [obj, slot_handle] = slot.obj;
    NTF_ASSERT(slot_handle == handle);
    return obj;
  }

  T& get_raw(span_type slots, handle_type handle) const {
    const auto idx = handle.index();
    NTF_ASSERT(idx < slots.size());
    slot_type& slot = slots[idx];
    NTF_ASSERT(!slot.is_empty());
    [[maybe_unused]] auto& [obj, slot_handle] = slot.obj;
    NTF_ASSERT(slot_handle == handle);
    return obj;
  }
};

} // namespace impl

template<typename T, size_t MaxExtent = dynamic_extent>
class freelist : private impl::freelist_base<T, MaxExtent> {
private:
  using base_t = impl::freelist_base<T, MaxExtent>;
  using slot_type = base_t::slot_type;

public:
  using element_type = T;
  using handle_type = base_t::handle_type;
  using value_type = std::pair<element_type, handle_type>;
  using size_type = std::array<slot_type, MaxExtent>::size_type;

  using reference = element_type&;
  using const_reference = const element_type&;
  using pointer = element_type*;
  using const_pointer = const element_type*;

  using iterator = impl::freelist_iter<T, false>;
  using const_iterator = impl::freelist_iter<T, true>;

  static constexpr size_type extent = MaxExtent;

public:
  constexpr freelist() noexcept :
      _slots(), _used_front{handle_type::NULL_INDEX}, _used_back{handle_type::NULL_INDEX},
      _free_head{handle_type::NULL_INDEX}, _count{0u} {
    std::memset(_slots.data(), 0xFF, capacity() * sizeof(_slots[0]));
  }

  constexpr freelist(std::initializer_list<T> il) noexcept(std::is_nothrow_copy_constructible_v<T>)
  requires(std::copy_constructible<T>)
  {
    u32 idx = 0u;
    for (const auto& obj : il) {
      if (idx >= _slots.size()) {
        break;
      }
      auto& slot = _slots[idx];
      slot.construct(idx, 0u, obj);
      slot.prev = idx ? idx - 1 : handle_type::NULL_INDEX;
      slot.next = idx != il.size() - 1 ? idx + 1 : handle_type::NULL_INDEX;
      ++idx;
    }
    _count = idx;
    _used_front = 0u;
    _used_back = _count - 1;
    _free_head = handle_type::NULL_INDEX;
  }

public:
  template<typename... Args>
  [[nodiscard]] optional<handle_type> emplace(Args&&... args) {
    if (_free_head != handle_type::NULL_INDEX) {
      return base_t::reuse_slot(_slots, _count, _free_head, _used_back,
                                std::forward<Args>(args)...);
    }
    if (size() == capacity()) {
      return {ntf::nullopt};
    }

    const u32 idx = size();
    NTF_ASSERT(idx < _slots.size());
    auto& slot = _slots[idx];
    slot.construct(idx, handle_type::INIT_VERSION, std::forward<Args>(args)...);
    slot.prev = _used_back;
    slot.next = handle_type::NULL_INDEX;

    if (_used_back != handle_type::NULL_INDEX) {
      NTF_ASSERT(_used_back < _slots.size());
      _slots[_used_back].next = idx;
    }
    if (_used_front == handle_type::NULL_INDEX) {
      _used_front = idx;
    }
    _used_back = idx;
    ++_count;

    return {ntf::in_place, idx, handle_type::INIT_VERSION};
  }

  void remove(handle_type handle) { base_t::return_slot(_slots, _count, _free_head, handle); }

  template<meta::freelist_remove_pred<T> F>
  void remove_where(F&& pred) {
    base_t::return_slot_where(_slots, _count, _free_head, std::forward<F>(pred));
  }

  bool is_valid(handle_type handle) const noexcept { return base_t::is_valid(_slots, handle); }

  template<meta::freelist_find_pred<T> F>
  optional<handle_type> find(F&& pred) const {
    return base_t::find_slot({_slots.data(), _slots.end()}, _used_front, std::forward<F>(pred));
  }

  template<meta::freelist_sort_pred<T> F>
  void sort(F&& pred) {
    base_t::sort_slots({_slots.data(), _slots.size()}, std::forward<F>(pred));
  }

  void sort() { base_t::sort_slots({_slots.data(), _slots.size()}, std::less<T>{}); }

  void clear() noexcept {
    if constexpr (std::is_trivially_destructible_v<T>) {
      std::memset(_slots.data(), 0xFF, capacity() * sizeof(_slots[0]));
    } else {
      for (slot_type& slot : _slots) {
        if (!slot.is_empty()) {
          slot.destroy();
          slot.prev = handle_type::NULL_INDEX;
          slot.next = handle_type::NULL_INDEX;
        }
      }
    }
    _used_front = handle_type::NULL_INDEX;
    _used_back = handle_type::NULL_INDEX;
    _free_head = handle_type::NULL_INDEX;
    _count = 0u;
  }

public:
  reference at(handle_type handle) {
    return base_t::get_ref({_slots.data(), _slots.size()}, handle);
  }

  const_reference at(handle_type handle) const {
    return base_t::get_cref({_slots.data(), _slots.size()}, handle);
  }

  pointer at_opt(handle_type handle) noexcept {
    return base_t::get_ptr({_slots.data(), _slots.size()}, handle);
  }

  const_pointer at_opt(handle_type handle) const noexcept {
    return base_t::get_cptr({_slots.data(), _slots.size()}, handle);
  }

  reference operator[](handle_type handle) {
    return base_t::get_raw({_slots.data(), _slots.size()}, handle);
  }

  const_reference operator[](handle_type handle) const {
    return base_t::get_craw({_slots.data(), _slots.size()}, handle);
  }

public:
  iterator begin() noexcept { return {_slots.data(), _used_front}; }

  const_iterator begin() const noexcept { return {_slots.data(), _used_front}; }

  const_iterator cbegin() const noexcept { return {_slots.data(), _used_front}; }

  iterator end() noexcept { return {_slots.data(), handle_type::NULL_INDEX}; }

  const_iterator end() const noexcept { return {_slots.data(), handle_type::NULL_INDEX}; }

  const_iterator cend() const noexcept { return {_slots.data(), handle_type::NULL_INDEX}; }

  reference front() {
    NTF_ASSERT(!empty());
    return _slots[_used_front].obj.first;
  }

  const_reference front() const {
    NTF_ASSERT(!empty());
    return _slots[_used_front].obj.first;
  }

  reference back() {
    NTF_ASSERT(!empty());
    return _slots[_used_back].obj.first;
  }

  const_reference back() const {
    NTF_ASSERT(!empty());
    return _slots[_used_back].obj.first;
  }

  constexpr bool empty() const noexcept { return size() == 0u; }

  constexpr size_type size() const noexcept { return static_cast<size_type>(_count); }

  constexpr size_type capacity() const noexcept { return MaxExtent; }

private:
  std::array<slot_type, MaxExtent> _slots;
  u32 _count;
  u32 _used_front;
  u32 _used_back;
  u32 _free_head;
};

template<typename T>
class freelist<T, dynamic_extent> : private impl::freelist_base<T, dynamic_extent> {
private:
  using base_t = impl::freelist_base<T, dynamic_extent>;
  using slot_type = base_t::slot_type;

public:
  using element_type = T;
  using handle_type = base_t::handle_type;
  using value_type = std::pair<element_type, handle_type>;
  using size_type = std::vector<slot_type>::size_type;

  using reference = element_type&;
  using const_reference = const element_type&;
  using pointer = element_type*;
  using const_pointer = const element_type*;

  using iterator = impl::freelist_iter<element_type, false>;
  using const_iterator = impl::freelist_iter<element_type, true>;

  static constexpr size_type extent = dynamic_extent;

public:
  constexpr freelist() noexcept :
      _slots(), _used_front{handle_type::NULL_INDEX}, _used_back{handle_type::NULL_INDEX},
      _free_head{handle_type::NULL_INDEX}, _count{0u} {}

  constexpr freelist(std::initializer_list<T> il)
  requires(std::copy_constructible<T>)
  {
    _slots.reserve(il.size());
    for (u32 idx = 0u; const auto& obj : il) {
      auto& slot = _slots.emplace_back(idx, handle_type::INIT_VERSION, obj);
      slot.prev = idx ? idx - 1 : handle_type::NULL_INDEX;
      slot.next = idx != il.size() - 1 ? idx + 1 : handle_type::NULL_INDEX;
      ++idx;
    }
    _count = static_cast<u32>(il.size());
    _used_front = 0u;
    _used_back = _count - 1;
    _free_head = handle_type::NULL_INDEX;
  }

public:
  template<typename... Args>
  [[nodiscard]] handle_type emplace(Args&&... args) {
    if (_free_head != handle_type::NULL_INDEX) {
      return base_t::reuse_slot({_slots.data(), _slots.size()}, _count, _free_head, _used_back,
                                std::forward<Args>(args)...);
    }

    NTF_ASSERT(_slots.size() == size());
    const u32 idx = _slots.size();
    auto& slot = _slots.emplace_back(idx, handle_type::INIT_VERSION, std::forward<Args>(args)...);
    slot.prev = _used_back;
    slot.next = handle_type::NULL_INDEX;

    if (_used_back != handle_type::NULL_INDEX) {
      NTF_ASSERT(_used_back < _slots.size());
      _slots[_used_back].next = idx;
    }
    if (_used_front == handle_type::NULL_INDEX) {
      _used_front = idx;
    }
    _used_back = idx;
    ++_count;

    return {idx, handle_type::INIT_VERSION};
  }

  void remove(handle_type handle) {
    base_t::return_slot({_slots.data(), _slots.size()}, _count, _free_head, handle);
  }

  template<meta::freelist_remove_pred<T> F>
  void remove_where(F&& pred) {
    base_t::return_slot_where({_slots.data(), _slots.size()}, _count, _free_head,
                              std::forward<F>(pred));
  }

  bool is_valid(handle_type handle) const noexcept {
    return base_t::is_valid({_slots.data(), _slots.size()}, handle);
  }

  template<meta::freelist_find_pred<T> F>
  optional<handle_type> find(F&& pred) const {
    return base_t::find_slot({_slots.data(), _slots.size()}, _used_front, std::forward<F>(pred));
  }

  template<meta::freelist_sort_pred<T> F>
  void sort(F&& pred) {
    base_t::sort_slots({_slots.data(), _slots.size()}, std::forward<F>(pred));
  }

  void sort() { base_t::sort_slots({_slots.data(), _slots.size()}, std::less<T>{}); }

  void clear() noexcept {
    _slots.clear();
    _used_front = handle_type::NULL_INDEX;
    _used_back = handle_type::NULL_INDEX;
    _free_head = handle_type::NULL_INDEX;
    _count = 0u;
  }

  void reserve(size_type count) { _slots.reserve(count); }

public:
  reference at(handle_type handle) {
    return base_t::get_ref({_slots.data(), _slots.size()}, handle);
  }

  const_reference at(handle_type handle) const {
    return base_t::get_cref({_slots.data(), _slots.size()}, handle);
  }

  pointer at_opt(handle_type handle) noexcept {
    return base_t::get_ptr({_slots.data(), _slots.size()}, handle);
  }

  const_pointer at_opt(handle_type handle) const noexcept {
    return base_t::get_cptr({_slots.data(), _slots.size()}, handle);
  }

  reference operator[](handle_type handle) {
    return base_t::get_raw({_slots.data(), _slots.size()}, handle);
  }

  const_reference operator[](handle_type handle) const {
    return base_t::get_craw({_slots.data(), _slots.size()}, handle);
  }

public:
  iterator begin() noexcept { return {_slots.data(), _used_front}; }

  const_iterator begin() const noexcept { return {_slots.data(), _used_front}; }

  const_iterator cbegin() const noexcept { return {_slots.data(), _used_front}; }

  iterator end() noexcept { return {_slots.data(), handle_type::NULL_INDEX}; }

  const_iterator end() const noexcept { return {_slots.data(), handle_type::NULL_INDEX}; }

  const_iterator cend() const noexcept { return {_slots.data(), handle_type::NULL_INDEX}; }

  reference front() {
    NTF_ASSERT(!empty());
    return _slots[_used_front].obj.first;
  }

  const_reference front() const {
    NTF_ASSERT(!empty());
    return _slots[_used_front].obj.first;
  }

  reference back() {
    NTF_ASSERT(!empty());
    return _slots[_used_back].obj.first;
  }

  const_reference back() const {
    NTF_ASSERT(!empty());
    return _slots[_used_back].obj.first;
  }

  constexpr bool empty() const noexcept { return size() == 0u; }

  constexpr size_type size() const noexcept { return static_cast<size_type>(_count); }

  constexpr size_type capacity() const noexcept { return _slots.capacity(); }

private:
  std::vector<impl::freelist_slot<T>> _slots;
  u32 _count;
  u32 _used_front;
  u32 _used_back;
  u32 _free_head;
};

} // namespace ntf
