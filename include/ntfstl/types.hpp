#pragma once

#include <ntfstl/core.hpp>

#include <cstddef>
#include <cstdint>
#include <utility>

#define NTF_DECLARE_TAG_TYPE(_name) \
  struct _name##_t {};              \
  constexpr _name##_t _name {}

#define NTF_DECLARE_OPAQUE_HANDLE(_name) typedef struct _name##_* _name

namespace ntf {

namespace numdefs {

using size_t = std::size_t;
using ptrdiff_t = std::ptrdiff_t;
using uintptr_t = std::uintptr_t;

using uint8 = std::uint8_t;
using u8 = uint8;

using uint16 = std::uint16_t;
using u16 = uint16;

using uint32 = std::uint32_t;
using u32 = uint32;

using uint64 = std::uint64_t;
using u64 = uint64;

using int8 = std::int8_t;
using i8 = int8;

using int16 = std::int16_t;
using i16 = int16;

using int32 = std::int32_t;
using i32 = int32;

using int64 = std::int64_t;
using i64 = int64;

using float32 = float;
using f32 = float32;

using float64 = double;
using f64 = float64;

} // namespace numdefs

using namespace numdefs;

NTF_DECLARE_TAG_TYPE(unchecked);
NTF_DECLARE_TAG_TYPE(uninitialized);
using in_place_t = std::in_place_t;
constexpr inline in_place_t in_place{};

class epoch_id {
public:
  static constexpr u32 null_index = static_cast<u32>(-1);

public:
  constexpr epoch_id() noexcept : _idx(null_index), _epoch() {}

  constexpr epoch_id(u32 idx, u32 epoch) noexcept : _idx(idx), _epoch(epoch) {}

  explicit constexpr epoch_id(u64 handle) noexcept : epoch_id(from_u64(handle)) {}

public:
  static constexpr epoch_id from_u64(u64 handle) noexcept {
    return {static_cast<u32>(handle & 0xFFFFFFFF), static_cast<u32>(handle >> 32)};
  }

public:
  constexpr u32 index() const noexcept { return _idx; }

  constexpr u32 version() const noexcept { return _epoch; }

  constexpr u32 epoch() const noexcept { return _epoch; }

  constexpr bool empty() const noexcept { return _idx == null_index; }

public:
  constexpr u64 as_u64() const noexcept {
    return static_cast<u64>(_epoch) << 32 | static_cast<u64>(_idx);
  }

  constexpr std::pair<u32, u32> as_pair() const noexcept { return {_idx, _epoch}; }

public:
  constexpr bool operator==(const epoch_id& other) const noexcept {
    return _idx == other._idx && _epoch == other._epoch;
  }

  constexpr bool operator!=(const epoch_id& other) const noexcept { return !(*this == other); }

public:
  constexpr explicit operator bool() noexcept { return !empty(); }

private:
  u32 _idx;
  u32 _epoch;
};

} // namespace ntf
