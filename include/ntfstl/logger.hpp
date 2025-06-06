#pragma once

#include <ntfstl/core.hpp>

#include <fmt/format.h>

#define NTF_LOG_ERROR_COL "[0;31m"
#define NTF_LOG_WARNING_COL "[0;33m"
#define NTF_LOG_INFO_COLOR "[0;34m"
#define NTF_LOG_DEBUG_COLOR "[0;32m"
#define NTF_LOG_VERBOSE_COLOR "[0;37m"

#define NTF_LOG_PRINT_TIME

#ifdef NTF_LOG_PRINT_TIME
#include <fmt/chrono.h>
#endif

namespace ntf {

enum class log_level : uint8_t {
  error = 0,
  warning,
  info,
  debug,
  verbose
};

class logger {
public:
  static inline void set_level(log_level new_level) {
    _log_level = new_level;
  }

  template<typename... Args>
  static inline void _log(log_level level, const std::string& prefix, const std::string& str_color,
                          fmt::format_string<Args...> format, Args&&... args) {
    if (_log_level < level) {
      return;
    }

#ifdef NTF_LOG_PRINT_TIME
    using TimePoint = std::chrono::system_clock::time_point;
    TimePoint now = std::chrono::system_clock::now();
    uint64_t ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;
    std::tm time_tm = fmt::localtime(std::chrono::system_clock::to_time_t(now));

    std::string log = fmt::format(format, std::forward<Args>(args)...);
    fmt::print("[{:%H:%M:%S}.{:03d}]\033{}[{}]\033[0m{}\n", time_tm, (int)ms,
               str_color, prefix, log);
#else
    fmt::print("\033{}[{}]\033[0m{}\n", str_color, prefix, 
               fmt::format(format, std::forward<Args>(args)...));
#endif
  }

  template<typename... Args>
  static inline void fatal(fmt::format_string<Args...> format, Args&&... args) {
    _log(log_level::error, "FATAL", NTF_LOG_ERROR_COL, format, std::forward<Args>(args)...);
  }

  template <typename... Args>
  static inline void error(fmt::format_string<Args...> format, Args&&... args) {
    _log(log_level::error, "ERROR", NTF_LOG_ERROR_COL, format, std::forward<Args>(args)...);
  }

  template <typename... Args>
  static inline void warning(fmt::format_string<Args...> format, Args&&... args) {
    _log(log_level::warning, "WARNING", NTF_LOG_WARNING_COL, format, std::forward<Args>(args)...);
  }

  template <typename... Args>
  static inline void info(fmt::format_string<Args...> format, Args&&... args) {
    _log(log_level::info, "INFO", NTF_LOG_INFO_COLOR, format, std::forward<Args>(args)...);
  }

  template <typename... Args>
  static inline void debug(fmt::format_string<Args...> format, Args&&... args) {
    _log(log_level::debug, "DEBUG", NTF_LOG_DEBUG_COLOR, format, std::forward<Args>(args)...);
  }

  template <typename... Args>
  static inline void verbose(fmt::format_string<Args...> format, Args&&... args) {
    _log(log_level::verbose, "VERBOSE", NTF_LOG_VERBOSE_COLOR, format, std::forward<Args>(args)...);
  }

private:
  static inline log_level _log_level {log_level::info};
};

} // namespace ntf
