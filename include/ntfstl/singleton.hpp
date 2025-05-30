#pragma once

#include <ntfstl/core.hpp>

namespace ntf {

template <typename T>
class singleton {
protected:
  singleton() noexcept {}

private:
  static auto& _instance() {
    static struct holder {
      holder() noexcept :
        dummy{}, inited{false} {}
      ~holder() noexcept {}

      union {
        T obj;
        char dummy;
      };
      bool inited;
    } h;
    return h;
  }

protected:
  template<typename... Args>
  static T& _construct(Args&&... args)
  noexcept(std::is_nothrow_constructible_v<T, Args...> && NTF_ASSERT_NOEXCEPT)
  {
    auto& storage = _instance();
    NTF_ASSERT(!storage.inited);
    new (&storage.obj) T(std::forward<Args>(args)...);
    storage.inited = true;
    return storage.obj;
  }

public:
  static void destroy()
  noexcept(std::is_nothrow_destructible_v<T> && NTF_ASSERT_NOEXCEPT)
  {
    auto& storage = _instance();
    NTF_ASSERT(storage.inited);
    storage.obj.~T();
    storage.inited = false;
  }

  static T& instance()
  noexcept(NTF_ASSERT_NOEXCEPT)
  {
    auto& storage = _instance();
    NTF_ASSERT(storage.inited);
    return storage.obj;
  }

public:
  singleton(const singleton&) = delete;
  singleton(singleton&&) = delete;
  singleton& operator=(const singleton&) = delete;
  singleton& operator=(singleton&&) = delete;
};

template <typename T>
class lazy_singleton {
protected:
  lazy_singleton() noexcept {}

public:
  static T& instance() {
    static T instance;
    return instance;
  }

public:
  lazy_singleton(const lazy_singleton&) = delete;
  lazy_singleton(lazy_singleton&&) = delete;
  lazy_singleton& operator=(const lazy_singleton&) = delete;
  lazy_singleton& operator=(lazy_singleton&&) = delete;
};

} // namespace ntf
