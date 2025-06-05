#pragma once

#include <utility>
#include <iterator>
#include <array>

namespace ntf {

template<typename T>
class span {
public:
  using element_type = T;
  using value_type = std::remove_cv_t<T>;

  using pointer = T*;
  using const_pointer = const T*;
  using reference = T&;
  using const_reference = const T&;

  using iterator = pointer;
  using const_iterator = const_pointer;

  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  using size_type = size_t;
  using difference_type = ptrdiff_t;

public:
  constexpr span() noexcept = default;

  constexpr explicit span(reference obj) noexcept :
    _data{std::addressof(obj)}, _size{1u} {}

  template<typename It>
  requires(std::contiguous_iterator<It>)
  constexpr span(It first, size_type count)
  requires(std::is_convertible_v<
    std::remove_reference_t<std::iter_reference_t<It>>(*)[],
   element_type(*)[]
  >) : _data{std::to_address(first)}, _size{count} {}

  template<typename It, typename End>
  requires(std::contiguous_iterator<It> && std::sized_sentinel_for<End, It>)
  constexpr span(It first, End last) 
  requires(std::is_convertible_v<
    std::remove_reference_t<std::iter_reference_t<It>>(*)[],
   element_type(*)[]
  >) : _data{std::to_address(first)}, _size{last-first} {}

  template<size_t N>
  constexpr span(std::type_identity_t<element_type>(&arr)[N]) noexcept
  requires(std::is_convertible_v<
    std::remove_pointer_t<decltype(std::data(arr))>(*)[],
    element_type(*)[]
  >) : _data{std::data(arr)}, _size{N} {}

  template<typename U, size_t N>
  constexpr span(std::array<U, N>& arr) noexcept
  requires(std::is_convertible_v<
    std::remove_pointer_t<decltype(std::data(arr))>(*)[],
    element_type(*)[]
  >) : _data{std::data(arr)}, _size{N} {}

  template<typename U, size_t N>
  constexpr span(const std::array<U, N>& arr) noexcept
  requires(std::is_convertible_v<
    std::remove_pointer_t<decltype(std::data(arr))>(*)[],
    element_type(*)[]
  >) : _data{std::data(arr)}, _size{N} {}

  constexpr span(std::initializer_list<value_type> il) noexcept :
    _data{il.begin()}, _size{il.size()} {}

  template<typename U>
  requires(std::is_convertible_v<U(*)[], element_type(*)[]>)
  constexpr span(const span<U>& src) noexcept :
    _data{src.data()}, _size{src.size()} {}

  constexpr span(const span&) noexcept = default;
  constexpr span(span&&) noexcept = default;

  constexpr ~span() noexcept = default;

public:
  constexpr span<element_type> first(size_type count) const {
    return {data(), count};
  }
  constexpr span<element_type> last(size_type count) const {
    return {data()+(size()-count), count};
  }

public:
  constexpr iterator begin() const noexcept { return _data; }
  constexpr const_iterator cbegin() const noexcept { return _data; }

  constexpr iterator end() const noexcept { return _data+_size; }
  constexpr const_iterator cend() const noexcept { return _data+_size; }

  constexpr reverse_iterator rbegin() const noexcept { return {_data+_size-1}; }
  constexpr const_reverse_iterator crbegin() const noexcept { return {_data+_size-1}; }

  constexpr reverse_iterator rend() const noexcept { return {_data-1}; }
  constexpr const_reverse_iterator crend() const noexcept { return {_data-1}; }

public:
  constexpr pointer data() const noexcept { return _data; }
  constexpr size_type size() const noexcept { return _size; }
  constexpr size_type size_bytes() const { return size()*sizeof(T); }

  constexpr reference front() const { return *begin(); }
  constexpr reference back() const { return *(end()-1); }

  constexpr bool empty() const noexcept { return size() == 0u; }
  constexpr pointer at(size_type idx) const noexcept {
    return idx >= size() ? nullptr : data()+idx;
  }

public:
  constexpr explicit operator bool() const noexcept { return !empty(); }
  constexpr reference operator[](size_type idx) const { return data()[idx]; }

  constexpr span& operator=(const span& other) noexcept = default;
  constexpr span& operator=(span&& other) noexcept = default;

  template<typename U>
  requires(std::is_convertible_v<U(*)[], element_type(*)[]>)
  constexpr span& operator=(const span<U>& other) noexcept {
    _data = other.data();
    _size = other.size();
    return *this;
  }

private:
  pointer _data;
  size_type _size;
};

template<typename T>
using cspan = span<const std::remove_cv_t<T>>;

} // namespace ntf
