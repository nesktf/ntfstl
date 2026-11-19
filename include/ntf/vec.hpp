#ifndef NTF_VEC_HPP
#define NTF_VEC_HPP

#include <ntf/impl/iterator.hpp>
#include <ntf/memory.hpp>

#ifndef NTF_NO_STD
#include <initializer_list>
#endif

namespace ntf {

template<typename T, size_t N>
class InlineVec {
public:
  static_assert(N > 0, "Invalid size");
  using value_type = T;
  using size_type = size_t;
  using difference_type = ptrdiff_t;
  using iterator = T*;
  using const_iterator = const T*;
  using reverse_iterator = impl::reverse_iter_wrap<iterator>;
  using const_reverse_iterator = impl::reverse_iter_wrap<const_iterator>;

public:
  InlineVec() noexcept : _size() {}

#ifndef NTF_NO_STD
  InlineVec(std::initializer_list<T> il) : _size(il.size()) {
    size_type i = 0;
    for (const auto& v : il) {
      _construct(i++, v);
    }
  }

  template<typename It>
  InlineVec(It first, It last)
  requires(std::input_iterator<meta::remove_cvref_t<It>>)
      : _size() {
    for (; first != last; ++first, ++_size) {
      _construct(_size, *first);
    }
  }
#endif

  InlineVec(size_type n, const T& val) noexcept(meta::nothrow_copy_constructible<T>) :
      _size(n >= capacity() ? capacity() - 1 : n) {
    for (size_type i = 0; i < _size; ++i) {
      _construct(i, val);
    }
  }

  InlineVec(const InlineVec& other) noexcept
  requires(meta::trivially_copy_constructible<T>)
  = default;

  InlineVec(InlineVec&& other) noexcept
  requires(meta::trivially_move_constructible<T>)
  = default;

  InlineVec(const InlineVec& other) noexcept(meta::nothrow_copy_constructible<T>)
  requires(!meta::trivially_copy_constructible<T>)
      : _size(other.size()) {
    for (size_type i = 0; i < other.size(); ++i) {
      _construct(i, other[i]);
    }
  }

  InlineVec(InlineVec&& other) noexcept(meta::nothrow_move_constructible<T>)
  requires(!meta::trivially_move_constructible<T>)
      : _size(other.size()) {
    for (size_type i = 0; i < other.size(); ++i) {
      _construct(i, move(other[i]));
    }
  }

  ~InlineVec()
  requires(!meta::trivially_destructible<T>)
  {
    _destroy_range(begin(), end());
  }

  ~InlineVec()
  requires(meta::trivially_destructible<T>)
  = default;

public:
  InlineVec& operator=(const InlineVec& other) noexcept(meta::trivially_copyable<T> ||
                                                        meta::nothrow_copy_constructible<T>) {
    if (this == &other) {
      return *this;
    }
    if constexpr (!meta::trivially_destructible<T>) {
      _destroy_range(begin(), end());
    }
    if constexpr (meta::trivially_copyable<T>) {
      if (other._size > 0) {
        memcpy(_ptr(), other._ptr(), other._size * sizeof(T));
      }
    } else {
      for (size_t i = 0; i < other._size; ++i) {
        NTF_PNEW(_ptr() + i) T(other._ptr()[i]);
      }
    }
    _size = other._size;
    return *this;
  }

  InlineVec& operator=(InlineVec&& other) {
    if (this == &other) {
      return *this;
    }
    if constexpr (!meta::trivially_destructible<T>) {
      _destroy_range(begin(), end());
    }
    if constexpr (meta::trivially_move_constructible<T> && meta::trivially_copyable<T>) {
      if (other._size > 0) {
        memcpy(_ptr(), other._ptr(), other._size * sizeof(T));
      }
    } else {
      for (size_t i = 0; i < other._size; ++i) {
        NTF_PNEW(_ptr() + i) T(move(other._ptr()[i]));
      }
      if constexpr (!meta::trivially_destructible<T>) {
        other._destroy_range(other.begin(), other.end());
      }
    }
    _size = other._size;
    other._size = 0;
    return *this;
  }

public:
  template<typename... Args>
  T& emplace_back(Args&&... args) {
    NTF_THROW_IF(_size == capacity(), BadAlloc());
    return *_construct(_size++, forward<Args>(args)...);
  }

  T& push_back(const T& obj) { return emplace_back(obj); }

  T& push_back(T&& obj) { return emplace_back(move(obj)); }

  template<typename... Args>
  T& unchecked_emplace_back(Args&&... args) {
    NTF_ASSERT(_size < capacity());
    return *_construct(_size++, forward<Args>(args)...);
  }

  T& unchecked_push_back(const T& obj) { return unchecked_emplace_back(obj); }

  T& unchecked_push_back(T&& obj) { return unchecked_emplace_back(move(obj)); }

  template<typename... Args>
  T* try_emplace_back(Args&&... args) {
    if (_size == capacity()) {
      return nullptr;
    }
    return _construct(_size++, forward<Args>(args)...);
  }

  T* try_push_back(const T& obj) { return try_emplace_back(obj); }

  T* try_push_back(T&& obj) { return try_emplace_back(move(obj)); }

  void pop_back() {
    if (empty()) {
      return;
    }
    if constexpr (!meta::trivially_destructible<T>) {
      (_ptr() + _size)->~T();
    }
    _size--;
  }

  void clear() {
    if constexpr (!meta::trivially_destructible<T>) {
      _destroy_range(begin(), end());
    }
    _size = 0;
  }

  static void reserve(size_type n) { NTF_ASSERT(n < capacity()); }

  static void shrink_to_fit() noexcept {}

  void resize(size_type n, const T& val) {
    if (n == _size) {
      return;
    }

    if (n < _size) {
      if constexpr (!meta::trivially_destructible<T>) {
        _destroy_range(begin() + n, begin() + _size);
      }
      _size = n;
    } else {
      n = n >= capacity() ? capacity() - 1 : n;
      for (size_t i = _size; i < n; ++i) {
        _construct(i, val);
      }
    }
  }

  void resize(size_type n) {
    if (n == _size) {
      return;
    }

    if (n < _size) {
      if constexpr (!meta::trivially_destructible<T>) {
        _destroy_range(begin() + n, begin() + _size);
      }
      _size = n;
    } else {
      n = n >= capacity() ? capacity() - 1 : n;
      for (size_t i = _size; i < n; ++i) {
        _construct(i);
      }
    }
  }

public:
  T& back() {
    NTF_ASSERT(!empty());
    return _ptr()[_size - 1];
  }

  T& front() {
    NTF_ASSERT(!empty());
    return *_ptr();
  }

  const T& back() const {
    NTF_ASSERT(!empty());
    return _ptr()[_size - 1];
  }

  const T& front() const {
    NTF_ASSERT(!empty());
    return *_ptr();
  }

  T& operator[](size_type i) {
    NTF_ASSERT(i < _size);
    return _ptr()[i];
  }

  const T& operator[](size_type i) const {
    NTF_ASSERT(i < _size);
    return _ptr()[i];
  }

  T& at(size_type i) {
    NTF_THROW_IF(i >= size(), BadAlloc());
    return _ptr()[i];
  }

  const T& at(size_type i) const {
    NTF_THROW_IF(i >= size(), BadAlloc());
    return _ptr()[i];
  }

public:
  size_type size() const noexcept { return _size; }

  bool empty() const noexcept { return _size == 0; }

  static constexpr size_type capacity() noexcept { return N; }

  static constexpr size_type max_size() noexcept { return N; }

public:
  iterator begin() { return _ptr(); }

  const_iterator begin() const { return _ptr(); }

  const_iterator cbegin() const { return _ptr(); }

  reverse_iterator rbegin() { return _ptr(); }

  const_reverse_iterator rbegin() const { return _ptr(); }

  const_reverse_iterator crbegin() const { return _ptr(); }

  iterator end() { return _ptr() + _size; }

  const_iterator end() const { return _ptr() + _size; }

  const_iterator cend() const { return _ptr() + _size; }

  reverse_iterator rend() { return _ptr() + _size; }

  const_reverse_iterator rend() const { return _ptr() + _size; }

  const_reverse_iterator crend() const { return _ptr() + _size; }

private:
  static void _destroy_range(iterator first, iterator last) {
    for (; first != last; ++first) {
      first->~T();
    }
  }

  template<typename... Args>
  T* _construct(size_type i, Args&&... args) {
    NTF_ASSERT(i < capacity());
    return NTF_PNEW(reinterpret_cast<T*>(_data) + i) T(forward<Args>(args)...);
  }

  T* _ptr() { return launder(reinterpret_cast<T*>(_data)); }

  const T* _ptr() const { return launder(reinterpret_cast<const T*>(_data)); }

private:
  alignas(T) uint8_t _data[sizeof(T) * N];
  size_type _size;
};

template<typename T, size_t SmallSz, typename Alloc = DefaultAlloc<T>>
class SmallVec : private Alloc {
public:
  static_assert(SmallSz > 0, "SmallSz must be greater than 0");

public:
  using value_type = T;
  using size_type = size_t;
  using difference_type = ptrdiff_t;
  using reference = T&;
  using const_reference = const T&;
  using pointer = T*;
  using const_pointer = const T*;
  using iterator = T*;
  using const_iterator = const T*;
  using reverse_iterator = impl::reverse_iter_wrap<iterator>;
  using const_reverse_iterator = impl::reverse_iter_wrap<const_iterator>;

public:
  SmallVec() noexcept(meta::nothrow_default_constructible<Alloc>) :
      _data(_inline_ptr()), _size(), _capacity(SmallSz) {}

  explicit SmallVec(size_t count) : SmallVec() { resize(count); }

  explicit SmallVec(const Alloc& alloc) noexcept(meta::nothrow_copy_constructible<Alloc>) :
      Alloc(alloc), _data(_inline_ptr()), _size(), _capacity(SmallSz) {}

  explicit SmallVec(size_t count, const Alloc& alloc) : SmallVec(alloc) { resize(count); }

#ifndef NTF_NO_STD
  SmallVec(std::initializer_list<T> init) : SmallVec() {
    reserve(init.size());
    for (const auto& item : init) {
      push_back(item);
    }
  }

  template<typename It>
  SmallVec(It first, It last)
  requires(std::input_iterator<meta::remove_cvref_t<It>>)
      : SmallVec() {
    for (; first != last; ++first) {
      push_back(*first);
    }
  }
#endif

  SmallVec(const SmallVec& other) :
      Alloc(other.allocator()), _data(_inline_ptr()), _size(other._size), _capacity(SmallSz) {
    if (other._size > SmallSz) {
      _data = Alloc::allocate(other._size);
      _capacity = other._size;
    }
    if constexpr (meta::trivially_copy_constructible<T>) {
      if (other._size > 0) {
        memcpy(_data, other._data, other._size * sizeof(T));
      }
    } else {
      for (size_t i = 0; i < other._size; ++i) {
        NTF_PNEW(_data + i) T(other._data[i]);
      }
    }
  }

  SmallVec(SmallVec&& other) noexcept(meta::nothrow_move_constructible<T> &&
                                      meta::nothrow_copy_constructible<Alloc>) :
      Alloc(other.allocator()), _data(_inline_ptr()), _size(other._size), _capacity(SmallSz) {
    if (other.is_small()) {
      if constexpr (meta::trivially_copyable<T>) {
        if (other._size > 0) {
          memcpy(_data, other._data, other._size * sizeof(T));
        }
      } else {
        for (size_t i = 0; i < other._size; ++i) {
          NTF_PNEW(_data + i) T(move(other._data[i]));
        }
        other._destroy_elements();
      }
      other._size = 0;
    } else {
      _data = other._data;
      _capacity = other._capacity;
      other._data = other._inline_ptr();
      other._size = 0;
      other._capacity = SmallSz;
    }
  }

  ~SmallVec() noexcept { _release_storage(); }

  SmallVec& operator=(const SmallVec& other) {
    if (this == &other) {
      return *this;
    }
    _release_storage();
    if (other._size > SmallSz) {
      _data = Alloc::allocate(other._size);
      _capacity = other._size;
    }
    if constexpr (meta::trivially_copyable<T>) {
      if (other._size > 0) {
        memcpy(_data, other._data, other._size * sizeof(T));
      }
    } else {
      for (size_t i = 0; i < other._size; ++i) {
        NTF_PNEW(_data + i) T(other._data[i]);
      }
    }
    _size = other._size;
    return *this;
  }

  SmallVec& operator=(SmallVec&& other) noexcept(meta::nothrow_move_constructible<T>) {
    if (this == &other) {
      return *this;
    }
    _release_storage();
    _size = other._size;
    if (other.is_small()) {
      if constexpr (meta::trivially_copyable<T>) {
        if (other._size > 0) {
          memcpy(_data, other._data, other._size * sizeof(T));
        }
      } else {
        for (size_t i = 0; i < other._size; ++i) {
          NTF_PNEW(_data + i) T(move(other._data[i]));
        }
        other._destroy_elements();
      }
      other._size = 0;
    } else {
      _data = other._data;
      _capacity = other._capacity;
      other._data = other._inline_ptr();
      other._size = 0;
      other._capacity = SmallSz;
    }
    return *this;
  }

public:
  void reserve(size_t new_cap) {
    if (new_cap <= _capacity || new_cap <= SmallSz) {
      return;
    }
    T* new_buf = Alloc::allocate(new_cap);
    if constexpr (meta::trivially_copyable<T>) {
      if (_size > 0) {
        memcpy(new_buf, _data, _size * sizeof(T));
      }
    } else {
      for (size_t i = 0; i < _size; ++i) {
        NTF_PNEW(new_buf + i) T(move(_data[i]));
        _data[i].~T();
      }
    }
    if (!is_small()) {
      Alloc::deallocate(_data, _capacity);
    }
    _data = new_buf;
    _capacity = new_cap;
  }

  void resize(size_t new_size) {
    if (new_size < _size) {
      if constexpr (!meta::trivially_destructible<T>) {
        for (size_t i = new_size; i < _size; ++i) {
          _data[i].~T();
        }
      }
    } else if (new_size > _size) {
      reserve(new_size);
      if constexpr (!meta::trivially_constructible<T>) {
        for (size_t i = _size; i < new_size; ++i) {
          NTF_PNEW(_data + i) T();
        }
      }
    }
    _size = new_size;
  }

  void resize(size_t new_size, const T& val) {
    if (new_size < _size) {
      if constexpr (!meta::trivially_destructible<T>) {
        for (size_t i = new_size; i < _size; ++i) {
          _data[i].~T();
        }
      }
    } else if (new_size > _size) {
      reserve(new_size);
      for (size_t i = _size; i < new_size; ++i) {
        if constexpr (meta::trivially_copyable<T>) {
          memcpy(_data + i, addressof(val), sizeof(T));
        } else {
          NTF_PNEW(_data + i) T(val);
        }
      }
    }
    _size = new_size;
  }

  void push_back(const T& val) {
    if (_size == _capacity) {
      reserve(_capacity * 2);
    }
    if constexpr (meta::trivially_copyable<T>) {
      memcpy(_data + _size, addressof(val), sizeof(T));
    } else {
      NTF_PNEW(_data + _size) T(val);
    }
    ++_size;
  }

  void push_back(T&& val) {
    if (_size == _capacity) {
      reserve(_capacity * 2);
    }
    if constexpr (meta::trivially_copyable<T>) {
      memcpy(_data + _size, addressof(val), sizeof(T));
    } else {
      NTF_PNEW(_data + _size) T(move(val));
    }
    ++_size;
  }

  template<typename... Args>
  T& emplace_back(Args&&... args) {
    if (_size == _capacity) {
      reserve(_capacity * 2);
    }
    T* p = NTF_PNEW(_data + _size) T(forward<Args>(args)...);
    ++_size;
    return *p;
  }

  void pop_back() noexcept {
    if (_size == 0) {
      return;
    }
    --_size;
    if constexpr (!meta::trivially_destructible<T>) {
      _data[_size].~T();
    }
  }

  void clear() noexcept { _destroy_elements(); }

public:
  bool operator==(const SmallVec& other) const noexcept {
    if (_size != other._size) {
      return false;
    }
    for (size_t i = 0; i < _size; ++i) {
      if (!(_data[i] == other._data[i])) {
        return false;
      }
    }
    return true;
  }

public:
  bool empty() const noexcept { return _size == 0; }

  size_t size() const noexcept { return _size; }

  size_t capacity() const noexcept { return _capacity; }

  bool is_small() const noexcept { return _data == _inline_ptr(); }

  T* data() noexcept { return _data; }

  const T* data() const noexcept { return _data; }

  T& operator[](size_t idx) {
    NTF_ASSERT(idx < _size);
    return _data[idx];
  }

  const T& operator[](size_t idx) const {
    NTF_ASSERT(idx < _size);
    return _data[idx];
  }

  T& front() {
    NTF_ASSERT(_size > 0);
    return _data[0];
  }

  const T& front() const {
    NTF_ASSERT(_size > 0);
    return _data[0];
  }

  T& back() {
    NTF_ASSERT(_size > 0);
    return _data[_size - 1];
  }

  const T& back() const {
    NTF_ASSERT(_size > 0);
    return _data[_size - 1];
  }

  iterator begin() noexcept { return _data; }

  const_iterator begin() const noexcept { return _data; }

  const_iterator cbegin() const noexcept { return _data; }

  reverse_iterator rbegin() noexcept { return _data; }

  const_reverse_iterator rbegin() const noexcept { return _data; }

  const_reverse_iterator crbegin() const noexcept { return _data; }

  iterator end() noexcept { return _data + _size; }

  const_iterator end() const noexcept { return _data + _size; }

  const_iterator cend() const noexcept { return _data + _size; }

  reverse_iterator rend() noexcept { return _data + _size; }

  const_reverse_iterator rend() const noexcept { return _data + _size; }

  const_reverse_iterator crend() const noexcept { return _data + _size; }

  Alloc& allocator() noexcept { return static_cast<Alloc&>(*this); }

  const Alloc& allocator() const noexcept { return static_cast<const Alloc&>(*this); }

private:
  void _destroy_elements() noexcept {
    if constexpr (!meta::trivially_destructible<T>) {
      for (size_t i = 0; i < _size; ++i) {
        _data[i].~T();
      }
    }
    _size = 0;
  }

  void _release_storage() noexcept {
    _destroy_elements();
    if (!is_small() && _data) {
      Alloc::deallocate(_data, _capacity);
    }
    _data = _inline_ptr();
    _capacity = SmallSz;
    _size = 0;
  }

  T* _inline_ptr() noexcept { return launder(reinterpret_cast<T*>(_inline_buf)); }

  const T* _inline_ptr() const noexcept {
    return launder(reinterpret_cast<const T*>(_inline_buf));
  }

private:
  alignas(T) u8 _inline_buf[sizeof(T) * SmallSz];
  T* _data;
  size_t _size;
  size_t _capacity;
};

} // namespace ntf

#endif // NTF_VEC_HPP
