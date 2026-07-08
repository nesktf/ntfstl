#ifndef NTF_FREELIST_HPP_
#define NTF_FREELIST_HPP_

#include <ntf/memory.hpp>

#include <string.h>

namespace ntf {

using FreelistSlot = u32;

template<typename T, size_t MaxElems>
class FixedFreelist {
public:
  static constexpr size_t max_element_count = MaxElems;
  using value_type = T;
  using element_slot = FreelistSlot;

private:
  struct slot_t {
    alignas(T) u8 elem[sizeof(T)];
    u32 next;
  };

  static constexpr element_slot ELEM_TOMB = static_cast<element_slot>(-1);
  static constexpr element_slot ELEM_ACTIVE = ELEM_TOMB - 1;
  static_assert(MaxElems < ELEM_ACTIVE, "Invalid max element count");

public:
  FixedFreelist() noexcept : _empty_head(0), _count(0) {
    memset(_slots, 0xFF, sizeof(_slots)); // Sets every slot to ELEM_TOMB
  }

  FixedFreelist(FixedFreelist&& other) noexcept(meta::nothrow_move_constructible<T>)
  requires(!meta::trivially_move_constructible<T>)
      : _count(other._count), _empty_head(other._empty_head) {
    memcpy(_slots, other._slots, sizeof(_slots));
    other.for_each([this](value_type& elem, element_slot slot) {
      NTF_PNEW(reinterpret_cast<T*>(_slots[slot].elem)) T(::ntf::move(elem));
    });
  }

  FixedFreelist(FixedFreelist&& other) noexcept
  requires(meta::trivially_move_constructible<T>)
  = default;

  FixedFreelist(const FixedFreelist& other) noexcept(meta::nothrow_copy_constructible<T>)
  requires(!meta::trivially_copy_constructible<T>)
      : _count(other._count), _empty_head(other._empty_head) {
    memcpy(_slots, other._slots, sizeof(_slots));
    other.for_each([this](const value_type& elem, element_slot slot) {
      NTF_PNEW(reinterpret_cast<T*>(_slots[slot].elem)) T(elem);
    });
  }

  FixedFreelist(const FixedFreelist& other) noexcept
  requires(meta::trivially_copy_constructible<T>)
  = default;

  ~FixedFreelist() noexcept
  requires(meta::trivially_destructible<T>)
  = default;

  ~FixedFreelist() noexcept
  requires(!meta::trivially_destructible<T>)
  {
    static_assert(meta::nothrow_destructible<T>);
    for_each([](T& elem) { elem.~T(); });
  }

public:
  element_slot insert(const value_type& elem) { return _do_insert(elem); }

  element_slot insert(value_type&& elem) { return _do_insert(::ntf::move(elem)); }

  template<typename... Args>
  element_slot emplace(Args&&... args) {
    return _do_insert(::ntf::forward<Args>(args)...);
  }

  void remove(element_slot slot) {
    if (!has_element(slot)) {
      return;
    }
    _do_remove(slot);
  }

private:
  template<typename... Args>
  element_slot _do_insert(Args&&... args) {
    NTF_ASSERT(_count < MaxElems);
    element_slot pos = _empty_head;
    NTF_ASSERT(pos < MaxElems);

    auto& slot = _slots[pos];
    NTF_ASSERT(slot.next != ELEM_ACTIVE);
    if (slot.next == ELEM_TOMB) {
      ++_empty_head;
    } else {
      _empty_head = slot.next;
    }

    new (reinterpret_cast<T*>(slot.elem)) T(::ntf::forward<Args>(args)...);
    slot.next = ELEM_ACTIVE;

    ++_count;
    return pos;
  }

  void _do_remove(element_slot pos) {
    auto& slot = _slots[pos];
    NTF_ASSERT(slot.next == ELEM_ACTIVE);
    if constexpr (!meta::trivially_destructible<T>) {
      _elem_at(pos).~T();
    }

    slot.next = _empty_head;
    _empty_head = pos;

    --_count;
  }

public:
  template<typename F>
  void for_each(F&& f) {
    for (element_slot pos = 0, i = 0; pos < _count && i < (element_slot)MaxElems; ++i) {
      if (_slots[i].next == ELEM_ACTIVE) {
        if constexpr (meta::invocable_with<meta::remove_cvref_t<F>, decltype(_slots[i].elem),
                                           element_slot>) {
          f(_elem_at(i), i);
        } else {
          f(_elem_at(i));
        }
        ++pos;
      }
    }
  }

  void clear() {
    if constexpr (!meta::trivially_destructible<T>) {
      for_each([](T& elem) { elem.~T(); });
    }
    memset(_slots, 0xFF, sizeof(_slots));
    _empty_head = 0;
    _count = 0;
  }

public:
  size_t size() const noexcept { return _count; }

  size_t capacity() const noexcept { return max_element_count; }

  bool empty() const noexcept { return size() == 0; }

  bool has_element(element_slot slot) const noexcept {
    return slot < MaxElems && _slots[slot].next == ELEM_ACTIVE;
  }

  const value_type& operator[](element_slot slot) const {
    NTF_ASSERT(has_element(slot));
    return _elem_at(slot);
  }

  value_type& operator[](element_slot slot) {
    return const_cast<value_type&>(as_const(*this)[slot]);
  }

  const value_type& at(element_slot slot) const {
    NTF_THROW_IF(!has_element(slot), MsgException("Slot has no element"));
    return _elem_at(slot);
  }

  value_type& at(element_slot slot) { return const_cast<value_type&>(as_const(*this).at(slot)); }

  const value_type* at_opt(element_slot slot) const noexcept {
    return has_element(slot) ? &_elem_at(slot) : nullptr;
  }

  value_type* at_opt(element_slot slot) noexcept {
    return const_cast<value_type*>(as_const(*this).at_opt(slot));
  }

private:
  const T& _elem_at(element_slot slot) const {
    return *launder(reinterpret_cast<const T*>(&_slots[slot].elem));
  }

  T& _elem_at(element_slot slot) { return const_cast<T&>(as_const(*this)._elem_at(slot)); }

private:
  slot_t _slots[MaxElems];
  element_slot _empty_head;
  u32 _count;
};

} // namespace ntf

#endif // NTF_FREELIST_HPP_
