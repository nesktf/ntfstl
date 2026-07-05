#ifndef NTF_STRING_HPP_
#define NTF_STRING_HPP_

#include <ntf/impl/iterator.hpp>

namespace ntf {

namespace impl {

template<typename Char>
constexpr NTF_INLINE void memcpy_char(Char* dst, const Char* src, size_t n) {
  if (is_constant_evaluated()) {
    for (size_t i = 0; i < n; ++i) {
      dst[i] = src[i];
    }
  } else {
    memcpy(dst, src, n * sizeof(Char));
  }
}

template<typename Char>
constexpr NTF_INLINE void memset_char(Char* dst, int val, size_t n) {
  if (is_constant_evaluated()) {
    for (size_t i = 0; i < n; ++i) {
      dst[i] = static_cast<Char>(val);
    }
  } else {
    memset(dst, val, n * sizeof(Char));
  }
}

} // namespace impl

template<size_t N, typename Char = char>
class StringBuf {
public:
  static_assert(N > 0, "N can't be 0");
  using size_type = size_t;
  using char_type = Char;

  static constexpr size_type BUFFER_SIZE = N;
  static constexpr size_type MAX_STRING_SIZE = N - 1;

  using iterator = Char*;
  using const_iterator = const Char*;

  using reverse_iterator = impl::reverse_iter_wrap<iterator>;
  using const_reverse_iterator = impl::reverse_iter_wrap<const_iterator>;

private:
  static constexpr size_type _cap_len(size_type size) noexcept {
    return size > MAX_STRING_SIZE ? MAX_STRING_SIZE : size;
  }

public:
  constexpr StringBuf(uninitialized_t, size_type size) noexcept : _size(_cap_len(size)) {}

  constexpr StringBuf() noexcept : _size(0) { impl::memset_char(_data, 0x00, N); }

  constexpr explicit StringBuf(size_type size) noexcept : _size(_cap_len(size)) {
    impl::memset_char(_data, 0x00, N);
  }

  template<size_t M>
  constexpr StringBuf(const Char (&str)[M]) noexcept : _size(M - 1) {
    static_assert(M <= N, "Char array is bigger than string buffer");
    impl::memset_char(_data, 0x00, N);
    impl::memcpy_char(_data, str, M);
  }

  constexpr explicit StringBuf(const Char* str) noexcept : _size(_cap_len(strlen(str))) {
    impl::memset_char(_data, 0x00, N);
    impl::memcpy_char(_data, str, _size);
  }

  constexpr StringBuf(const Char* str, size_type size) noexcept : _size(_cap_len(size)) {
    impl::memset_char(_data, 0x00, N);
    impl::memcpy_char(_data, str, _size);
  }

public:
  constexpr void resize(size_type size) noexcept {
    size = _cap_len(size);
    if (size == _size) {
      return;
    } else if (size < _size) {
      impl::memset_char(_data + size, 0x00, N - size);
    } else {
      impl::memset_char(_data + _size, 0x00, size - _size);
    }
    _size = size;
  }

  constexpr void resize(size_type size, const Char& ch) noexcept {
    size = _cap_len(size);
    if (size == _size) {
      return;
    } else if (size < _size) {
      impl::memset_char(_data + size, 0x00, N - size);
    } else {
      impl::memset_char(_data + _size, ch, size - _size);
    }
    _size = size;
  }

public:
  constexpr Char& at(size_type i) {
    NTF_THROW_IF(i >= size(), MsgException("Index out of range in StringBuf"));
    return _data[i];
  }

  constexpr const Char& at(size_type i) const {
    NTF_THROW_IF(i >= size(), MsgException("Index out of range in StringBuf"));
    return _data[i];
  }

  constexpr Char& operator[](size_type i) {
    NTF_ASSERT(i < _size, "Out of range in StringBuf");
    return _data[i];
  }

  constexpr const Char& operator[](size_type i) const {
    NTF_ASSERT(i < _size, "Out of range in StringBuf");
    return _data[i];
  }

public:
  constexpr Char* data() noexcept { return _data; }

  constexpr const Char* data() const noexcept { return _data; }

  constexpr const Char* c_str() const noexcept { return _data; }

  constexpr size_t size() const noexcept { return _size; }

  constexpr size_t capacity() const noexcept { return MAX_STRING_SIZE; }

  constexpr bool empty() const noexcept { return size() == 0; }

public:
  constexpr iterator begin() noexcept { return data(); }

  constexpr const_iterator begin() const noexcept { return data(); }

  constexpr const_iterator cbegin() const noexcept { return data(); }

  constexpr iterator end() noexcept { return data() + size(); }

  constexpr const_iterator end() const noexcept { return data() + size(); }

  constexpr const_iterator cend() const noexcept { return data() + size(); }

  constexpr reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }

  constexpr const_reverse_iterator rbegin() const noexcept {
    return const_reverse_iterator(end());
  }

  constexpr const_reverse_iterator crbegin() const noexcept {
    return const_reverse_iterator(end());
  }

  constexpr reverse_iterator rend() noexcept { return reverse_iterator(begin()); }

  constexpr const_reverse_iterator rend() const noexcept {
    return const_reverse_iterator(begin());
  }

  constexpr const_reverse_iterator crend() const noexcept {
    return const_reverse_iterator(begin());
  }

private:
  Char _data[N];
  size_type _size;
};

template<size_t N, typename Char>
StringBuf(const Char (&)[N]) -> StringBuf<N, Char>;

} // namespace ntf

#endif // NTF_STRING_HPP_
