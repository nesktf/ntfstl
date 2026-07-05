#ifndef NTF_SPAN_HPP_
#define NTF_SPAN_HPP_

#include <ntf/impl/concepts.hpp>
#include <ntf/impl/iterator.hpp>

namespace ntf {

constexpr size_t dynamic_extent = (size_t)-1;

namespace impl {

template<size_t Extent>
struct SpanExtent {
protected:
  constexpr SpanExtent(size_t ext) { NTF_ASSERT(ext == Extent); }

  constexpr size_t get_extent() const noexcept { return Extent; }

  constexpr void assign_extent(size_t) noexcept {}
};

template<>
struct SpanExtent<dynamic_extent> {
protected:
  constexpr SpanExtent() noexcept = default;

  constexpr SpanExtent(size_t ext) noexcept : _ext{ext} {}

  constexpr size_t get_extent() const noexcept { return _ext; }

  constexpr void assign_extent(size_t ext) noexcept { _ext = ext; }

private:
  size_t _ext;
};

} // namespace impl

template<typename T, size_t Extent = dynamic_extent>
class Span : public impl::SpanExtent<Extent> {
public:
  using element_type = T;
  using value_type = meta::remove_cv_t<T>;

  using pointer = T*;
  using const_pointer = const T*;
  using reference = T&;
  using const_reference = const T&;

  using iterator = pointer;
  using const_iterator = const_pointer;

  using reverse_iterator = impl::reverse_iter_wrap<iterator>;
  using const_reverse_iterator = impl::reverse_iter_wrap<const_iterator>;

  using size_type = size_t;
  using difference_type = ptrdiff_t;

  static constexpr size_type extent = dynamic_extent;

public:
  constexpr Span() noexcept
  requires(extent == dynamic_extent)
      : impl::SpanExtent<Extent>{0u}, _data{nullptr} {}

  constexpr explicit Span(reference obj) noexcept
  requires(extent == 1u || extent == dynamic_extent)
      : impl::SpanExtent<Extent>{1u}, _data{::ntf::addressof(obj)} {}

  template<typename U>
  explicit(extent != dynamic_extent) constexpr Span(U* data, size_type count)
      // requires(meta::convertible_to<U (*)[], element_type (*)[]>)
      : impl::SpanExtent<Extent>{count}, _data{data} {}

  template<typename It>
  // requires(std::contiguous_iterator<It> && std::sized_sentinel_for<End, It>)
  requires(!meta::same_as<It, size_t>)
  explicit(extent != dynamic_extent) constexpr Span(It first, It last)
  requires(meta::convertible_to<meta::remove_reference_t<decltype(*declval<It>())> (*)[],
                                element_type (*)[]>)
      : impl::SpanExtent<Extent>{last - first}, _data{&*first} {}

  template<typename U, size_t N>
  constexpr Span(meta::type_identity_t<U> (&arr)[N]) noexcept
  requires(meta::convertible_to<U (*)[], element_type (*)[]> &&
           (extent == dynamic_extent || extent == N))
      : impl::SpanExtent<Extent>{N}, _data(arr) {}

#if 0
  template<typename U, size_t N>
  constexpr Span(std::array<U, N>& arr) noexcept
  requires(std::is_convertible_v<std::remove_pointer_t<decltype(std::data(arr))> (*)[],
                                 element_type (*)[]> &&
           (extent == dynamic_extent || extent == N))
      : impl::SpanExtent<Extent>{N}, _data{std::data(arr)} {}

  template<typename U, size_t N>
  constexpr Span(const std::array<U, N>& arr) noexcept
  requires(std::is_convertible_v<std::remove_pointer_t<decltype(std::data(arr))> (*)[],
                                 element_type (*)[]> &&
           (extent == dynamic_extent || extent == N))
      : impl::SpanExtent<Extent>{N}, _data{std::data(arr)} {}

  explicit(extent != dynamic_extent) constexpr Span(std::initializer_list<value_type> il) :
      impl::SpanExtent<Extent>{il.size()}, _data{il.begin()} {}
#endif

  template<typename U, size_t N>
  explicit(extent != dynamic_extent &&
           N == dynamic_extent) constexpr Span(const Span<U, N>& src) noexcept
  requires(N != extent && meta::convertible_to<U (*)[], element_type (*)[]> &&
           (extent == dynamic_extent || N == dynamic_extent || extent == N))
      : impl::SpanExtent<Extent>{N}, _data{src.data()} {}

  template<typename U>
  constexpr Span(const Span<U, dynamic_extent>& src) noexcept
  requires(meta::convertible_to<U (*)[], element_type (*)[]> && extent == dynamic_extent)
      : impl::SpanExtent<dynamic_extent>{src.size()}, _data{src.data()} {}

  constexpr Span(const Span&) noexcept = default;
  constexpr Span(Span&&) noexcept = default;

  constexpr ~Span() noexcept = default;

public:
  template<size_t N>
  constexpr Span<element_type, N> first() const
  requires(N <= extent || extent == dynamic_extent)
  {
    if constexpr (extent == dynamic_extent) {
      NTF_ASSERT(N < size());
    }
    return {data(), N};
  }

  constexpr Span<element_type, dynamic_extent> first(size_type count) const {
    NTF_ASSERT(count < size());
    return {data(), count};
  }

  template<size_t N>
  constexpr Span<element_type, N> last() const
  requires(N <= extent || extent == dynamic_extent)
  {
    if constexpr (extent == dynamic_extent) {
      NTF_ASSERT(N < size());
    }
    return {data() + (size() - N), N};
  }

  constexpr Span<element_type, dynamic_extent> last(size_type count) const {
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

  constexpr size_type size() const noexcept { return impl::SpanExtent<Extent>::get_extent(); }

  constexpr size_type size_bytes() const { return size() * sizeof(T); }

  constexpr reference front() const { return *begin(); }

  constexpr reference back() const { return *(end() - 1); }

  constexpr bool empty() const noexcept { return size() == 0u; }

  constexpr reference at(size_type idx) const {
    NTF_THROW_IF(idx >= size(), MsgException("Index out of range in Span"));
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

  constexpr Span& operator=(const Span& other) noexcept = default;
  constexpr Span& operator=(Span&& other) noexcept = default;

  template<typename U, size_t N>
  requires(meta::convertible_to<U (*)[], element_type (*)[]> &&
           (extent == dynamic_extent || N == dynamic_extent || extent == N))
  constexpr Span& operator=(const Span<U, N>& other) noexcept {
    impl::SpanExtent<Extent>::assign_extent(other.size());
    _data = other.data();
    return *this;
  }

private:
  pointer _data;
};

} // namespace ntf

#endif // NTF_SPAN_HPP_
