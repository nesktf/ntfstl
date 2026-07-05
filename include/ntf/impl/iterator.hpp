#ifndef NTF_ITERATOR_HPP_
#define NTF_ITERATOR_HPP_

#include <ntf/impl/concepts.hpp>

namespace ntf::meta {

template<typename It>
struct iterator_traits {
  // TODO: Define iterator concept
  using pointer = typename It::pointer;
  using difference_type = typename It::difference_type;
  using reference = typename It::reference;
};

template<typename T>
struct iterator_traits<T*> {
  using pointer = T*;
  using difference_type = ptrdiff_t;
  using reference = T&;
};

} // namespace ntf::meta

namespace ntf::impl {

template<typename It>
class reverse_iter_wrap {
public:
  using iterator_type = It;
  using pointer = typename meta::iterator_traits<It>::pointer;
  using difference_type = typename meta::iterator_traits<It>::difference_type;
  using reference = typename meta::iterator_traits<It>::reference;

  template<typename Iter2>
  friend class reverse_iterator;

public:
  constexpr reverse_iter_wrap() noexcept(noexcept(It())) : _curr() {}

  constexpr explicit reverse_iter_wrap(It it) noexcept(noexcept(It(it))) : _curr(it) {}

  constexpr reverse_iter_wrap(const reverse_iter_wrap& other) noexcept(noexcept(It(other._curr))) :
      _curr(other._curr) {}

  constexpr reverse_iter_wrap& operator=(const reverse_iter_wrap&) = default;

  template<typename Iter2>
  constexpr reverse_iter_wrap(const reverse_iter_wrap<Iter2>& other) noexcept(
    noexcept(It(other._curr))) : _curr(other._curr) {}

  template<typename Iter2>
  constexpr reverse_iter_wrap&
  operator=(const reverse_iter_wrap<Iter2>& other) noexcept(noexcept(_curr = other._curr)) {
    _curr = other._curr;
    return *this;
  }

public:
  NTF_NODISCARD constexpr iterator_type base() const noexcept(noexcept(It(_curr))) {
    return _curr;
  }

  NTF_NODISCARD constexpr reference operator*() const {
    It tmp = _curr;
    return *--tmp;
  }

  NTF_NODISCARD constexpr pointer operator->() const {
    It tmp = _curr;
    --tmp;
    to_pointer(tmp);
  }

  constexpr reverse_iter_wrap& operator++() {
    --_curr;
    return *this;
  }

  constexpr reverse_iter_wrap operator++(int) {
    reverse_iter_wrap self = *this;
    --_curr;
    return self;
  }

  constexpr reverse_iter_wrap& operator--() {
    ++_curr;
    return *this;
  }

  constexpr reverse_iter_wrap operator--(int) {
    reverse_iter_wrap self = *this;
    ++_curr;
    return self;
  }

  NTF_NODISCARD constexpr reverse_iter_wrap operator+(difference_type n) const {
    return reverse_iter_wrap(_curr - n);
  }

  constexpr reverse_iter_wrap& operator+=(difference_type n) {
    _curr -= n;
    return *this;
  }

  NTF_NODISCARD constexpr reverse_iter_wrap operator-(difference_type n) const {
    return reverse_iter_wrap(_curr + n);
  }

  constexpr reverse_iter_wrap& operator-=(difference_type n) {
    _curr += n;
    return *this;
  }

  NTF_NODISCARD constexpr reference operator[](difference_type n) const { return *(*this + n); }

private:
  template<typename T>
  static constexpr T* to_pointer(T* p) {
    return p;
  }

  template<typename T>
  static constexpr T to_pointer(T it) {
    return it.operator->();
  }

private:
  It _curr;
};

template<typename Iter1, typename Iter2>
NTF_NODISCARD constexpr bool operator==(const reverse_iter_wrap<Iter1>& a,
                                        const reverse_iter_wrap<Iter2>& b)
requires requires {
  { a.base() == b.base() } -> meta::convertible_to<bool>;
}
{
  return a.base() == b.base();
}

template<typename Iter1, typename Iter2>
NTF_NODISCARD constexpr bool operator!=(const reverse_iter_wrap<Iter1>& a,
                                        const reverse_iter_wrap<Iter2>& b)
requires requires {
  { a.base() != b.base() } -> meta::convertible_to<bool>;
}
{
  return a.base() != b.base();
}

template<typename Iter1, typename Iter2>
NTF_NODISCARD constexpr bool operator<(const reverse_iter_wrap<Iter1>& a,
                                       const reverse_iter_wrap<Iter2>& b)
requires requires {
  { a.base() > b.base() } -> meta::convertible_to<bool>;
}
{
  return a.base() > b.base();
}

template<typename Iter1, typename Iter2>
NTF_NODISCARD constexpr bool operator>(const reverse_iter_wrap<Iter1>& a,
                                       const reverse_iter_wrap<Iter2>& b)
requires requires {
  { a.base() < b.base() } -> meta::convertible_to<bool>;
}
{
  return a.base() < b.base();
}

template<typename Iter1, typename Iter2>
NTF_NODISCARD constexpr bool operator<=(const reverse_iter_wrap<Iter1>& a,
                                        const reverse_iter_wrap<Iter2>& b)
requires requires {
  { a.base() >= b.base() } -> meta::convertible_to<bool>;
}
{
  return a.base() >= b.base();
}

template<typename Iter1, typename Iter2>
NTF_NODISCARD constexpr bool operator>=(const reverse_iter_wrap<Iter1>& a,
                                        const reverse_iter_wrap<Iter2>& b)
requires requires {
  { a.base() <= b.base() } -> meta::convertible_to<bool>;
}
{
  return a.base() <= b.base();
}

template<typename Iter>
NTF_NODISCARD constexpr bool operator==(const reverse_iter_wrap<Iter>& a,
                                        const reverse_iter_wrap<Iter>& b)
requires requires {
  { a.base() == b.base() } -> meta::convertible_to<bool>;
}
{
  return a.base() == b.base();
}

template<typename Iter1, typename Iter2>
NTF_NODISCARD constexpr auto operator-(const reverse_iter_wrap<Iter1>& a,
                                       const reverse_iter_wrap<Iter2>& b)
  -> decltype(b.base() - a.base()) {
  return b.base() - a.base();
}

template<typename Iter>
NTF_NODISCARD constexpr reverse_iter_wrap<Iter>
operator+(typename reverse_iter_wrap<Iter>::difference_type n, const reverse_iter_wrap<Iter>& a) {
  return reverse_iterator<Iter>(a.base() - n);
}

} // namespace ntf::impl

#endif // NTF_ITERATOR_HPP_
