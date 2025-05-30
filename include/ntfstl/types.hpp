#pragma once

#include <ntfstl/core.hpp>

#include <cstddef>
#include <cstdint>
#include <utility>

#define NTF_DECLARE_TAG_TYPE(_name) \
struct _name##_t {}; \
constexpr _name##_t _name{}

#define NTF_DECLARE_OPAQUE_HANDLE(_name) \
typedef struct _name##_* _name

namespace ntf {

namespace numdefs {

using size_t    = std::size_t;
using ptrdiff_t = std::ptrdiff_t;
using uintptr_t = std::uintptr_t;

using uint8   = std::uint8_t;
using u8      = uint8;

using uint16  = std::uint16_t;
using u16     = uint16;

using uint32  = std::uint32_t;
using u32     = uint32;

using uint64  = std::uint64_t;
using u64     = uint64;

using int8    = std::int8_t;
using i8      = int8;

using int16   = std::int16_t;
using i16     = int16;

using int32   = std::int32_t;
using i32     = int32;

using int64   = std::int64_t;
using i64     = int64;

using float32 = float;
using f32     = float32;

using float64 = double;
using f64     = float64;

} // namespace

using namespace numdefs;

NTF_DECLARE_TAG_TYPE(unchecked);
NTF_DECLARE_TAG_TYPE(uninitialized);
using in_place_t = std::in_place_t;
constexpr inline in_place_t in_place{};

} // namespace ntf
