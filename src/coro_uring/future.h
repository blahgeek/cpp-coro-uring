#pragma once

#include <optional>

#include "coro_uring/coroutines_compat.h"
#include "coro_uring/promise.h"
#include "coro_uring/debug.h"

namespace coro_uring {

template <typename T>
class Future {
 public:
  class Promise : public PromiseBase<T, Future<T>, std::suspend_never> {
   public:
    void return_value(T value) {
      TRACE_FUNCTION() << value;
      this->SetValue(std::move(value));
    }
  };

  using promise_type = Promise;

  explicit Future(std::coroutine_handle<promise_type> handle)
      : handle_(handle) {}

  ~Future() {
    TRACE_FUNCTION();
    if (handle_) {
      handle_.destroy();
    }
  }

  bool await_ready() noexcept {
    return handle_.done();
  }

  T await_resume() noexcept {
    TRACE_FUNCTION();
    std::optional<T> value = handle_.promise().PopValue();
    DCHECK(value.has_value());
    return std::move(*value);
  }

  void await_suspend(std::coroutine_handle<> handle) {
    TRACE_FUNCTION();
    handle_.promise().SetPrecursor(handle);
  }

  void Detach() {
    handle_.promise().SetDetached(true);
    handle_ = std::coroutine_handle<promise_type>();
  }

 private:
  std::coroutine_handle<promise_type> handle_;
};

}

