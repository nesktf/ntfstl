#pragma once

#include <ntfstl/core.hpp>

// Until i make a optional type
#include <optional>

namespace ntf {

template<typename T>
using optional = std::optional<T>;

using nullopt_t = std::nullopt_t;
constexpr nullopt_t nullopt = std::nullopt;

namespace meta {

template<typename T>
struct optional_checker : public std::false_type{};

template<typename T>
struct optional_checker<optional<T>> : public std::true_type{};

template<typename T>
constexpr bool optional_checker_v = optional_checker<T>::value;

template<typename T>
concept optional_type = optional_checker_v<T>;

} // namespace meta

} // namespace ntf
