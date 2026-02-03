#pragma once

#include <ntfstl/meta.hpp>
#include <ntfstl/types.hpp>

#include <ntfstl/memory.hpp>

#include <vector>

namespace ntf {

template<typename T, typename Alloc = ::ntf::mem::default_pool::allocator<T>>
class dynvec {
  // TBD
};

template<typename T, size_t VecCap>
class fixed_dynvec {
  // TBD
};

template<typename T, size_t SmallCap, typename Alloc = ::ntf::mem::default_pool::allocator<T>>
class small_dynvec {
  // TBD
};

namespace meta {

template<typename T, typename Vec>
struct vec_rebinder {
  using type = ::ntf::meta::rebind_first_arg_t<T, Vec>;
};

template<typename T, typename Vec>
using vec_rebinder_t = vec_rebinder<T, Vec>::type;

template<typename T, typename U, typename Alloc>
struct vec_rebinder<T, std::vector<U, Alloc>> {
  using type = std::vector<T, typename std::allocator_traits<Alloc>::template rebind_alloc<T>>;
};

template<typename T, typename U, typename Alloc>
struct vec_rebinder<T, ::ntf::dynvec<U, Alloc>> {
  using type = ::ntf::dynvec<T, typename std::allocator_traits<Alloc>::template rebind_alloc<T>>;
};

template<typename T, typename U, size_t VecCap>
struct vec_rebinder<T, ::ntf::fixed_dynvec<U, VecCap>> {
  using type = ::ntf::fixed_dynvec<T, VecCap>;
};

template<typename T, typename U, size_t SmallCap, typename Alloc>
struct vec_rebinder<T, ::ntf::small_dynvec<U, SmallCap, Alloc>> {
  using type =
    ::ntf::small_dynvec<T, SmallCap,
                        typename std::allocator_traits<Alloc>::template rebind_alloc<T>>;
};

} // namespace meta

// Stable index vector based on https://github.com/johnBuffer/StableIndexVector
template<::ntf::meta::move_swappable_type T, typename Cont = std::vector<T>>
class si_dynvec {
public:
  using data_vec_type = Cont;
  using allocator_type = data_vec_type::allocator_type;

  using value_type = T;
  static_assert(std::same_as<typename Cont::value_type, value_type>,
                "Cont must be a container of T");

  using iterator = data_vec_type::iterator;
  static_assert(std::contiguous_iterator<iterator>, "Cont must be a contiguous container");

  using const_iterator = data_vec_type::const_iterator;
  using reverse_iterator = data_vec_type::reverse_iterator;
  using const_reverse_iterator = data_vec_type::const_reverse_iterator;

private:
  struct metadata_t {
    u32 ridx = 0;
    u32 current_epoch = 0;
  };

  template<bool IsConst>
  class data_handle_t {
  private:
    using vec_t = std::conditional_t<IsConst, const si_dynvec, si_dynvec>;
    using data_value_type = std::conditional_t<IsConst, const value_type, value_type>;

  public:
    data_handle_t() noexcept : _id(), _vec() {}

    data_handle_t(epoch_id id, vec_t& vec) noexcept : _id(id), _vec(&vec) {}

  public:
    epoch_id id() const noexcept { return _id; }

    bool empty() const noexcept { return !_vec || !_vec->is_valid(_id); }

    data_value_type& get() const {
      NTF_ASSERT(_vec);
      return (*_vec)[_id];
    }

  public:
    data_value_type* operator->() const {
      NTF_ASSERT(_vec);
      return &(*_vec)[_id];
    }

    data_value_type& operator*() const {
      NTF_ASSERT(_vec);
      return (*_vec)[_id];
    }

  public:
    explicit operator bool() const noexcept { return !empty(); }

  private:
    epoch_id _id;
    vec_t* _vec;

  public:
    friend si_dynvec;
  };

public:
  using data_handle = data_handle_t<false>;
  using const_data_handle = data_handle_t<true>;

public:
  si_dynvec() : _data(), _metadata(), _indexes() {}

  si_dynvec(const allocator_type& alloc) :
      _data(alloc),
      _metadata(
        typename std::allocator_traits<allocator_type>::template rebind_alloc<metadata_t>{alloc}),
      _indexes(typename std::allocator_traits<allocator_type>::template rebind_alloc<u32>{alloc}) {
  }

private:
  epoch_id _find_free_slot() {
    u32 id = -1;
    u32 epoch = 0;
    if (_metadata.size() > _data.size()) {
      // Slot available
      epoch = ++_metadata[_data.size()].current_epoch;
      id = static_cast<u32>(_metadata[_data.size()].ridx);
    } else {
      // Create new slot
      const auto new_id = _data.size();
      _metadata.emplace_back(new_id, 0);
      _indexes.emplace_back(new_id);
      id = static_cast<u32>(new_id);
    }
    NTF_ASSERT(id < _indexes.size());
    _indexes[id] = _data.size();
    return {id, epoch};
  }

public:
  epoch_id push_back(const value_type& object) {
    const auto id = _find_free_slot();
    _data.push_back(object);
    return id;
  }

  epoch_id push_back(value_type&& object) {
    const auto id = _find_free_slot();
    _data.push_back(std::move(object));
    return id;
  }

  template<typename... Args>
  epoch_id emplace_back(Args&&... args) {
    const auto id = _find_free_slot();
    _data.emplace_back(std::forward<Args>(args)...);
    return id;
  }

  void erase(epoch_id id) {
    const auto [index, epoch] = id.as_pair();
    NTF_ASSERT(index < _indexes.size());

    const u32 data_idx = _indexes[index];
    NTF_ASSERT(data_idx < _metadata.size());
    auto& metadata = _metadata[data_idx];
    NTF_ASSERT(metadata.current_epoch == epoch);

    const u32 last_data_idx = static_cast<u32>(_data.size() - 1);
    const u32 last_idx = metadata.ridx;
    ++metadata.current_epoch;

    std::swap(_data[data_idx], _data[last_data_idx]);
    std::swap(_metadata[data_idx], _metadata[last_data_idx]);
    std::swap(_indexes[index], _indexes[last_idx]);

    _data.pop_back();
  }

  void erase(const data_handle& handle) {
    NTF_ASSERT(handle.m_vector == this);
    NTF_ASSERT(!handle.empty());
    erase(handle.id());
  }

  void erase_index(u32 idx) {
    NTF_ASSERT(idx < _metadata.size());
    erase(epoch_id{_metadata[idx].ridx, _metadata[idx].current_epoch});
  }

public:
  template<typename F>
  void remove_if(F&& pred) {
    static_assert(std::is_invocable_v<std::remove_cvref_t<F>, value_type&>,
                  "F must be invocable with T&");
    using ret_t = std::invoke_result_t<F, value_type&>;
    static_assert(std::convertible_to<ret_t, bool>, "F must return a boolean value");
    for (u32 i = 0; i < _data.size();) {
      if (std::invoke(pred, _data[i])) {
        erase_index(i);
      } else {
        ++i;
      }
    }
  }

  void reserve(size_t size) {
    _data.reserve(size);
    _metadata.reserve(size);
    _indexes.reserve(size);
  }

  void clear() {
    _data.clear();
    for (auto& m : _metadata) {
      ++m.current_epoch;
    }
  }

  bool is_valid(epoch_id id) const {
    return id.index() < _indexes.size() &&
           _metadata[_indexes[id.index()]].current_epoch == id.epoch();
  }

  data_handle make_handle(epoch_id id) {
    NTF_ASSERT(is_valid(id));
    return {id, *this};
  }

  const_data_handle make_handle(epoch_id id) const {
    NTF_ASSERT(is_valid(id));
    return {id, *this};
  }

  u32 data_index_of(epoch_id id) const {
    NTF_ASSERT(id.index() < _indexes.size());
    return _indexes[id.index()];
  }

  data_handle make_index_handle(u32 idx) {
    NTF_ASSERT(idx < size());
    const auto& meta = _metadata[idx];
    epoch_id id(meta.ridx, meta.current_epoch);
    return {id, *this};
  }

  const_data_handle make_index_handle(u32 idx) const {
    NTF_ASSERT(idx < size());
    const auto& meta = _metadata[idx];
    epoch_id id(meta.ridx, meta.current_epoch);
    return {id, *this};
  }

  epoch_id next_id() const noexcept {
    if (_metadata.size() > _data.size()) {
      const auto& meta = _metadata[_data.size()];
      return {meta.ridx, meta.current_epoch + 1};
    } else {
      return {static_cast<u32>(_data.size()), 0};
    }
  }

public:
  const value_type& operator[](epoch_id id) const {
    NTF_ASSERT(is_valid(id));
    return _data[_indexes[id.index()]];
  }

  value_type& operator[](epoch_id id) { return const_cast<T&>(std::as_const(*this)[id]); }

  const T& at(epoch_id id) const {
    if (!is_valid(id)) {
      throw std::out_of_range("Id out of range");
    }
    return _data[_indexes[id.index()]];
  }

  value_type& at(epoch_id id) { return const_cast<T&>(std::as_const(*this).at(id)); }

  const T* at_opt(epoch_id id) const noexcept {
    if (!is_valid(id)) {
      return nullptr;
    }
    return &_data[_indexes[id.index()]];
  }

  value_type* at_opt(epoch_id id) noexcept {
    return const_cast<T*>(std::as_const(*this).at_opt(id));
  }

public:
  value_type* data() { return _data.data(); }

  size_t size() const { return _data.size(); }

  bool empty() const { return _data.empty(); }

  size_t capacity() const { return _data.capacity(); }

  span<T> as_span() noexcept { return {_data.data(), _data.size()}; }

  span<const T> as_span() const noexcept { return {_data.data(), _data.size()}; }

public:
  iterator begin() noexcept { return _data.begin(); }

  const_iterator begin() const noexcept { return _data.begin(); }

  const_iterator cbegin() const noexcept { return _data.cbegin(); }

  iterator end() noexcept { return _data.end(); }

  const_iterator end() const noexcept { return _data.end(); }

  const_iterator cend() const noexcept { return _data.cend(); }

  reverse_iterator rbegin() noexcept { return _data.rbegin(); }

  const_reverse_iterator rbegin() const noexcept { return _data.rbegin(); }

  const_reverse_iterator crbegin() const noexcept { return _data.crbegin(); }

  reverse_iterator rend() noexcept { return _data.rend(); }

  const_reverse_iterator rend() const noexcept { return _data.rend(); }

  const_reverse_iterator crend() const noexcept { return _data.crend(); }

private:
  data_vec_type _data;
  ::ntf::meta::vec_rebinder_t<metadata_t, data_vec_type> _metadata;
  ::ntf::meta::vec_rebinder_t<u32, data_vec_type> _indexes;
};

} // namespace ntf
