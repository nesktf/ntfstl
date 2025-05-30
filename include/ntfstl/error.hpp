#pragma once

#include <ntfstl/concepts.hpp>

#include <exception>
#include <source_location>

#include <fmt/core.h>

namespace ntf {

template<typename... FmtArgs>
struct error_fmt {
  consteval error_fmt(fmt::format_string<FmtArgs...> fmt_,
                      const std::source_location& loc_ = std::source_location::current()) :
    fmt{fmt_}, loc{loc_} {}

  fmt::format_string<FmtArgs...> fmt;
  std::source_location loc;
};

template<typename T = void>
class error : public std::exception {
public:
  template<typename U>
  error(U&& data, std::string msg,
        const std::source_location& loc = std::source_location::current()) :
    _data{std::forward<U>(data)}, _msg{std::move(msg)}, _loc{loc} {}

public:
  const char* what() const noexcept override { return _msg.c_str();  }
  const std::source_location& where() const noexcept { return _loc; }

  const T& data() const noexcept { return _data; }
  T& data() noexcept { return _data; }

  const std::string& msg() const noexcept { return _msg; }
  std::string& msg() noexcept { return _msg; }

public:
  template<meta::is_forwarding<T> U, typename... Args>
  static error format(U&& data, const error_fmt<Args...>& msg, Args&&... args) {
    return {std::forward<U>(data), fmt::format(msg.fmt, std::forward<Args>(args)...), msg.loc};
  }

protected:
  T _data;
  std::string _msg;
  std::source_location _loc;
};

template<>
class error<void> : public std::exception {
public:
  error(std::string msg, const std::source_location& loc = std::source_location::current()) :
    _msg{std::move(msg)}, _loc{loc} {}

public:
  const char* what() const noexcept override { return _msg.c_str(); }
  const std::source_location& where() const noexcept { return _loc; }

  const std::string& msg() const noexcept { return _msg; }
  std::string& msg() noexcept { return _msg; }

public:
  template<typename... Args>
  static error format(const error_fmt<Args...>& msg, Args&&... args) {
    return {fmt::format(msg.fmt, std::forward<Args>(args)...), msg.loc};
  }

private:
  std::string _msg;
  std::source_location _loc;
};

} // namespace ntf
