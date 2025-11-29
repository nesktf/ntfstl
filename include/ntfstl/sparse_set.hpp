#pragma once

#include <ntfstl/expected.hpp>
#include <ntfstl/optional.hpp>
#include <ntfstl/types.hpp>

namespace ntf {

namespace impl {

template<typename T>
class sparse_set_alloc : private std::allocator<u8> {
private:
  using base_t = std::allocator<u8>;

protected:
  T* alloc_elem(size_t n) { return reinterpret_cast<T*>(base_t::allocate(n * sizeof(T))); }

  void dealloc_elem(T* elems, size_t n) {
    base_t::deallocate(reinterpret_cast<u8*>(elems), n * sizeof(T));
  }

  u32** alloc_sparse(size_t n) {
    u8* ptr = base_t::allocate(n * sizeof(u32*));
    std::memset(ptr, 0, n * sizeof(u32*));
    return reinterpret_cast<u32**>(ptr);
  }

  void dealloc_sparse(u32** sparse, size_t n) {
    base_t::deallocate(reinterpret_cast<u8*>(sparse), n * sizeof(u32*));
  }

  u32* alloc_page(size_t n) {
    u8* ptr = base_t::allocate(n * sizeof(u32));
    std::memset(ptr, 0xFF, n * sizeof(u32));
    return reinterpret_cast<u32*>(ptr);
  }

  void dealloc_page(u32* page, size_t n) {
    base_t::deallocate(reinterpret_cast<u8*>(page), n * sizeof(u32));
  }
};

} // namespace impl

constexpr u32 sparse_dyn_size = std::numeric_limits<u32>::max();

template<typename T, u32 MaxElems = sparse_dyn_size>
class sparse_set {};

template<typename T>
class sparse_set<T, sparse_dyn_size> : public impl::sparse_set_alloc<T> {
private:
  struct make_key_t {};

  using alloc_t = impl::sparse_set_alloc<T>;

  static constexpr size_t PAGE_SIZE = 1024u;
  static constexpr u32 ELEM_TOMB = std::numeric_limits<u32>::max();
  static constexpr f32 DENSE_GROW_FAC = 2.f;

public:
  using element_type = T;
  using element_id = u32;

  using iterator = T*;
  using const_iterator = const T*;

public:
  sparse_set() noexcept :
      _sparse{}, _dense{}, _sparse_cap{}, _sparse_count{}, _dense_cap{}, _dense_count{} {}

public:
  template<typename U>
  T& push(element_id elem, U&& obj) {
    return emplace(elem, std::forward<U>(obj));
  }

  template<typename... Args>
  T& emplace(element_id elem, Args&&... args) {
    const auto [page, page_idx] = _sparse_pos(elem);
    if (page >= _sparse_cap) {
      _grow_sparse(page);
    }

    auto& sparse_page = _sparse[page];
    if (sparse_page == nullptr) {
      sparse_page = alloc_t::alloc_page(PAGE_SIZE);
    }

    u32 idx = _sparse[page][page_idx];
    if (idx != ELEM_TOMB) {
      std::destroy_at(_dense + idx);
      return _construct(idx, std::forward<Args>(args)...);
    }

    idx = _dense_count;
    if (idx == _dense_cap) {
      _grow_dense();
    }

    auto& ret = _construct(idx, std::forward<Args>(args)...);
    ++_dense_count;
    return ret;
  }

  void erase(element_id elem) {}

  void reset() {
    if constexpr (!std::is_trivially_destructible_v<T>) {
      for (u32 i = 0; i < _dense_count; ++i) {
        std::destroy_at(_dense + i);
      }
    }
    _dense_count = 0u;

    alloc_t::dealloc_elem(_dense, _dense_cap);
    _dense_cap = 0u;

    for (u32 i = 0; i < _sparse_cap; ++i) {
      if (_sparse[i] != nullptr) {
        alloc_t::dealloc_page(_sparse[i], PAGE_SIZE);
        ++_sparse_count;
      }
    }
    alloc_t::dealloc_sparse(_sparse, _sparse_cap);
    _sparse_cap = 0u;
    _sparse_count = 0u;
  }

  void clear() {
    if constexpr (!std::is_trivially_destructible_v<T>) {
      for (u32 i = 0; i < _dense_count; ++i) {
        std::destroy_at(_dense + i);
      }
    }
    _dense_count = 0u;

    for (u32 i = 0; i < _sparse_cap; ++i) {
      std::memset(_sparse + i, 0xFF, sizeof(*_sparse));
    }
  }

public:
  T* at_ptr(element_id elem) noexcept {
    const auto [page, page_idx] = _sparse_pos(elem);
    if (page >= _sparse_cap) {
      return nullptr;
    }

    const u32 idx = _sparse[page][page_idx];
    if (idx == ELEM_TOMB) {
      return nullptr;
    }

    return std::addressof(_dense[idx]);
  }

  const T* at_ptr(element_id elem) const noexcept {
    const auto [page, page_idx] = _sparse_pos(elem);
    if (page >= _sparse_cap) {
      return nullptr;
    }

    const u32 idx = _sparse[page][page_idx];
    if (idx == ELEM_TOMB) {
      return nullptr;
    }

    return std::addressof(_dense[idx]);
  }

  T& at(element_id elem) {
    const auto [page, page_idx] = _sparse_pos(elem);
    NTF_THROW_IF(page >= _sparse_cap, std::out_of_range);
    const u32 idx = _sparse[page][page_idx];
    NTF_THROW_IF(idx == ELEM_TOMB, std::out_of_range);
    return _dense[idx];
  }

  const T& at(element_id elem) const {
    const auto [page, page_idx] = _sparse_pos(elem);
    NTF_THROW_IF(page >= _sparse_cap, std::out_of_range);
    const u32 idx = _sparse[page][page_idx];
    NTF_THROW_IF(idx == ELEM_TOMB, std::out_of_range);
    return _dense[idx];
  }

public:
  T& operator[](element_id elem) {
    const auto [page, page_idx] = _sparse_pos(elem);
    NTF_ASSERT(page >= _sparse_cap);
    const u32 idx = _sparse[page][page_idx];
    NTF_ASSERT(idx != ELEM_TOMB);
    return _dense[idx];
  }

  const T& operator[](element_id elem) const {
    const auto [page, page_idx] = _sparse_pos(elem);
    NTF_ASSERT(page >= _sparse_cap);
    const u32 idx = _sparse[page][page_idx];
    NTF_ASSERT(idx != ELEM_TOMB);
    return _dense[idx];
  }

public:
  iterator begin() noexcept { return _dense; }

  const_iterator begin() const noexcept { return _dense; }

  const_iterator cbegin() const noexcept { return _dense; }

  iterator end() noexcept { return _dense + _dense_count; }

  const_iterator end() const noexcept { return _dense + _dense_count; }

  const_iterator cend() const noexcept { return _dense + _dense_count; }

  size_t size() const noexcept { return static_cast<size_t>(_dense_count); }

  size_t capacity() const noexcept { return static_cast<size_t>(_dense_cap); }

  size_t pages() const noexcept { return static_cast<size_t>(_sparse_count); }

  size_t page_capacity() const noexcept { return static_cast<size_t>(_sparse_cap); }

  bool empty() const noexcept { return size() == 0; }

  bool has_element(element_id elem) const noexcept {
    const auto [page, page_idx] = _sparse_pos(elem);
    if (page >= _sparse_cap) {
      return false;
    }
    const u32* sparse_page = _sparse[page];
    if (sparse_page == nullptr) {
      return false;
    }

    return sparse_page[page_idx] != ELEM_TOMB;
  }

private:
  void _grow_sparse(u32 page_count) {
    NTF_ASSERT(page_count < _sparse_cap);
    u32** new_sparse = alloc_t::alloc_sparse(page_count);
    std::memcpy(new_sparse, _sparse, _sparse_cap * sizeof(*_sparse));
    alloc_t::dealloc_sparse(_sparse, _sparse_cap);
    _sparse = new_sparse;
    _sparse_cap = page_count;
  }

  void _grow_dense() {
    T* new_dense = nullptr;
    const u32 new_cap = static_cast<u32>(std::round(DENSE_GROW_FAC * _dense_cap));

    try {
      new_dense = alloc_t::alloc_elem(new_cap);

      if constexpr (std::is_trivially_copyable_v<T>) {
        std::memcpy(new_dense, _dense, _dense_count * sizeof(*_dense));
      } else {
        for (u32 i = 0; i < _dense_count; ++i) {
          if constexpr (std::move_constructible<T>) {
            new (new_dense + i) T{std::move(_dense[i])};
          } else {
            new (new_dense + i) T{_dense[i]};
          }
        }
      }
      if constexpr (!std::is_trivially_destructible_v<T>) {
        for (u32 i = 0; i < _dense_count; ++i) {
          std::destroy_at(_dense + i);
        }
      }

      _dense = new_dense;
      _dense_cap = new_cap;
    } catch (...) {
      if (new_dense) {
        alloc_t::dealloc_elem(new_dense, new_cap);
      }
      NTF_RETHROW();
    }
  }

  template<typename... Args>
  T& _construct(u32 idx, Args&&... args) {
    new (_dense + idx) T{std::forward<Args>(args)...};
    return _dense[idx];
  }

private:
  static std::pair<u32, u32> _sparse_pos(element_id elem) {
    const u32 page = elem / PAGE_SIZE;
    const u32 page_idx = elem % PAGE_SIZE;
    return {page, page_idx};
  }

private:
  u32** _sparse;
  T* _dense;
  u32 _sparse_cap;
  u32 _sparse_count;
  u32 _dense_cap;
  u32 _dense_count;
};

} // namespace ntf
