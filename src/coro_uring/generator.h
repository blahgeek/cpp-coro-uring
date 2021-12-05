#pragma once

#include <optional>

#include "coro_uring/coroutines_compat.h"
#include "coro_uring/promise.h"
#include "coro_uring/debug.h"

namespace coro_uring {

template <typename T>
class Generator {
 public:
  class Promise : public PromiseBase<T, Generator<T>, std::suspend_always> {
   public:
    void return_void() {
      TRACE_FUNCTION();
    }

    auto yield_value(T value) {
      TRACE_FUNCTION() << value;
      this->SetValue(std::move(value));
      return this->GetPrecursorAwaitable();
    }
  };

  using promise_type = Promise;

  explicit Generator(std::coroutine_handle<promise_type> handle)
      : handle_(handle) {}

  ~Generator() {
    TRACE_FUNCTION();
    if (handle_) {
      handle_.destroy();
    }
  }

  bool await_ready() noexcept {
    return handle_.done();
  }

  std::optional<T> await_resume() noexcept {
    TRACE_FUNCTION();
    if (handle_.done()) {
      return std::nullopt;
    }
    return handle_.promise().PopValue();
  }

  std::coroutine_handle<> await_suspend(std::coroutine_handle<> handle) {
    TRACE_FUNCTION();
    handle_.promise().SetPrecursor(handle);
    DCHECK(!handle_.done());
    return handle_;  // run myself
  }

 private:
  std::coroutine_handle<promise_type> handle_;
};

}
