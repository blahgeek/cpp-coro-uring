#pragma once

#include <optional>

#include <glog/logging.h>

#include "coro_uring/coroutines_compat.h"
#include "coro_uring/debug.h"

namespace coro_uring {

template <typename T, typename ReturnObjectT, typename InitialSuspendT>
class PromiseBase {
 public:
  auto initial_suspend() noexcept {
    TRACE_FUNCTION();
    return InitialSuspendT();
  }

  auto final_suspend() noexcept {
    TRACE_FUNCTION();
    return GetPrecursorAwaitable();
  }

  ReturnObjectT get_return_object() {
    TRACE_FUNCTION();
    using promise_t = typename ReturnObjectT::promise_type;
    return ReturnObjectT(std::coroutine_handle<promise_t>::from_promise(
        *static_cast<promise_t*>(this)));
  }

  void unhandled_exception() {
    TRACE_FUNCTION();
    throw;
  }

  void SetValue(T value) {
    value_ = std::move(value);
  }

  std::optional<T> PopValue() {
    std::optional<T> result = std::move(value_);
    return result;
  }

  // Only Future can detach
  void SetDetached(bool detach) {
    detached_ = detach;
  }

  void SetPrecursor(std::coroutine_handle<> precursor) {
    if (precursor) {
      DCHECK(!precursor_) << "Cannot set precursor multiple times";
    }
    precursor_ = precursor;
  }

 protected:
  auto GetPrecursorAwaitable() noexcept {
    struct Awaitable {
      bool await_ready() noexcept { return false; }
      void await_resume() noexcept {}
      std::coroutine_handle<> await_suspend(std::coroutine_handle<> h) noexcept {
        TRACE_FUNCTION();
        if (detached) {
          DCHECK(!precursor) << "Detached promise cannot have precursor";
          h.destroy();
        }
        return precursor ? precursor : std::noop_coroutine();
      }

      bool detached;
      std::coroutine_handle<> precursor;
    };
    Awaitable result{.detached = detached_, .precursor = precursor_};
      // reset precursor, because it should be set again
    precursor_ = std::coroutine_handle<>();
    return result;
  }

 private:
  std::optional<T> value_;
  bool detached_ = false;
  std::coroutine_handle<> precursor_;
};

}
