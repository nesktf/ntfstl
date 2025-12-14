#pragma once

#include <ntfstl/core.hpp>

#include <array>
#include <iterator>
#include <utility>

namespace ntf {

constexpr size_t dynamic_extent = 0u;

namespace impl {

template<size_t SpanExtent>
struct span_extent {
  constexpr span_extent(size_t ext) { NTF_ASSERT(ext == SpanExtent); }

  constexpr size_t get_extent() const noexcept { return SpanExtent; }

  constexpr void assign_extent(size_t) noexcept {};
};

template<>
struct span_extent<dynamic_extent> {
  constexpr span_extent() noexcept = default;

  constexpr span_extent(size_t ext) noexcept : _ext{ext} {}

  constexpr size_t get_extent() const noexcept { return _ext; }

  constexpr void assign_extent(size_t ext) noexcept { _ext = ext; };

private:
  size_t _ext;
};

} // namespace impl

template<typename T, size_t SpanExtent = dynamic_extent>
class span : public impl::span_extent<SpanExtent> {
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

  static constexpr size_type extent = dynamic_extent;

public:
  constexpr span() noexcept
  requires(extent == dynamic_extent)
  = default;

  constexpr explicit span(reference obj) noexcept
  requires(extent == 1u || extent == dynamic_extent)
      : impl::span_extent<SpanExtent>{1u}, _data{std::addressof(obj)} {}

  template<typename It>
  requires(std::contiguous_iterator<It>)
  explicit(extent != dynamic_extent) constexpr span(It first, size_type count)
  requires(std::is_convertible_v<std::remove_reference_t<std::iter_reference_t<It>> (*)[],
                                 element_type (*)[]>)
      : impl::span_extent<SpanExtent>{count}, _data{std::to_address(first)} {}

  template<typename It, typename End>
  requires(std::contiguous_iterator<It> && std::sized_sentinel_for<End, It>)
  explicit(extent != dynamic_extent) constexpr span(It first, End last)
  requires(std::is_convertible_v<std::remove_reference_t<std::iter_reference_t<It>> (*)[],
                                 element_type (*)[]>)
      : impl::span_extent<SpanExtent>{last - first}, _data{std::to_address(first)} {}

  template<size_t N>
  constexpr span(std::type_identity_t<element_type> (&arr)[N]) noexcept
  requires(std::is_convertible_v<std::remove_pointer_t<decltype(std::data(arr))> (*)[],
                                 element_type (*)[]> &&
           (extent == dynamic_extent || extent == N))
      : impl::span_extent<SpanExtent>{N}, _data{std::data(arr)} {}

  template<typename U, size_t N>
  constexpr span(std::array<U, N>& arr) noexcept
  requires(std::is_convertible_v<std::remove_pointer_t<decltype(std::data(arr))> (*)[],
                                 element_type (*)[]> &&
           (extent == dynamic_extent || extent == N))
      : impl::span_extent<SpanExtent>{N}, _data{std::data(arr)} {}

  template<typename U, size_t N>
  constexpr span(const std::array<U, N>& arr) noexcept
  requires(std::is_convertible_v<std::remove_pointer_t<decltype(std::data(arr))> (*)[],
                                 element_type (*)[]> &&
           (extent == dynamic_extent || extent == N))
      : impl::span_extent<SpanExtent>{N}, _data{std::data(arr)} {}

  explicit(extent != dynamic_extent) constexpr span(std::initializer_list<value_type> il) :
      impl::span_extent<SpanExtent>{il.size()}, _data{il.begin()} {}

  template<typename U, size_t N>
  explicit(extent != dynamic_extent &&
           N == dynamic_extent) constexpr span(const span<U, N>& src) noexcept
  requires(std::is_convertible_v<U (*)[], element_type (*)[]> &&
           (extent == dynamic_extent || N == dynamic_extent || extent == N))
      : impl::span_extent<SpanExtent>{N}, _data{src.data()} {}

  constexpr span(const span&) noexcept = default;
  constexpr span(span&&) noexcept = default;

  constexpr ~span() noexcept = default;

public:
  template<size_t N>
  constexpr span<element_type, N> first() const
  requires(N <= extent || extent == dynamic_extent)
  {
    if constexpr (extent == dynamic_extent) {
      NTF_ASSERT(N < size());
    }
    return {data(), N};
  }

  constexpr span<element_type, dynamic_extent> first(size_type count) const {
    NTF_ASSERT(count < size());
    return {data(), count};
  }

  template<size_t N>
  constexpr span<element_type, N> last() const
  requires(N <= extent || extent == dynamic_extent)
  {
    if constexpr (extent == dynamic_extent) {
      NTF_ASSERT(N < size());
    }
    return {data() + (size() - N), N};
  }

  constexpr span<element_type, dynamic_extent> last(size_type count) const {
    NTF_ASSERT(count < size());
    return {data() + (size() - count), count};
  }

public:
  constexpr iterator begin() const noexcept { return _data; }

  constexpr const_iterator cbegin() const noexcept { return _data; }

  constexpr iterator end() const noexcept { return _data + size(); }

  constexpr const_iterator cend() const noexcept { return _data + size(); }

  constexpr reverse_iterator rbegin() const noexcept { return {_data + size() - 1}; }

  constexpr const_reverse_iterator crbegin() const noexcept { return {_data + size() - 1}; }

  constexpr reverse_iterator rend() const noexcept { return {_data - 1}; }

  constexpr const_reverse_iterator crend() const noexcept { return {_data - 1}; }

public:
  constexpr pointer data() const noexcept { return _data; }

  constexpr size_type size() const noexcept { return impl::span_extent<SpanExtent>::get_extent(); }

  constexpr size_type size_bytes() const { return size() * sizeof(T); }

  constexpr reference front() const { return *begin(); }

  constexpr reference back() const { return *(end() - 1); }

  constexpr bool empty() const noexcept { return size() == 0u; }

  constexpr reference at(size_type idx) const {
    NTF_THROW_IF(idx >= size(), std::out_of_range, fmt::format("Index {} out of range", idx));
    return _data[idx];
  }

  constexpr pointer at_opt(size_type idx) noexcept {
    return idx >= size() ? nullptr : data() + idx;
  }

public:
  constexpr explicit operator bool() const noexcept { return !empty(); }

  constexpr reference operator[](size_type idx) const {
    NTF_ASSERT(idx < size());
    return _data[idx];
  }

  constexpr span& operator=(const span& other) noexcept = default;
  constexpr span& operator=(span&& other) noexcept = default;

  template<typename U, size_t N>
  requires(std::is_convertible_v<U (*)[], element_type (*)[]> &&
           (extent == dynamic_extent || N == dynamic_extent || extent == N))
  constexpr span& operator=(const span<U, N>& other) noexcept {
    impl::span_extent<SpanExtent>::assign_extent(other.size());
    _data = other.data();
    return *this;
  }

private:
  pointer _data;
};

template<typename T, size_t SpanSize = dynamic_extent>
using cspan = span<const std::remove_cv_t<T>, SpanSize>;

} // namespace ntf
