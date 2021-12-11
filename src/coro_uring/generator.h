#pragma once

#include <optional>

#include "coro_uring/coroutines_compat.h"
#include "coro_uring/promise.h"

namespace coro_uring {

template <typename T>
class Generator {
 public:
  class Promise
      : public internal::PromiseBase<T, Generator<T>, std::suspend_always> {
   public:
    constexpr void return_void() {}

    template <typename Arg>
    auto yield_value(Arg value) {
      this->SetValue(std::forward<Arg>(value));
      return this->GetPrecursorAwaitable();
    }
  };

  using promise_type = Promise;

  explicit Generator(std::coroutine_handle<promise_type> handle)
      : handle_(handle) {}

  ~Generator() {
    if (handle_) {
      handle_.destroy();
    }
  }

  bool await_ready() noexcept {
    return handle_.done();
  }

  std::optional<T>&& await_resume() noexcept {
    if (handle_.done()) {
      static std::optional<T> nullval;
      nullval.reset();
      return std::move(nullval);
    }
    return handle_.promise().GetValue();
  }

  std::coroutine_handle<> await_suspend(std::coroutine_handle<> handle) {
    handle_.promise().SetPrecursor(handle);
    DCHECK(!handle_.done());
    return handle_;  // run myself
  }

 private:
  std::coroutine_handle<promise_type> handle_;
};

}
