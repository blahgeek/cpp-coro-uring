#pragma once

#include <optional>
#include <type_traits>

#include "coro_uring/coroutines_compat.h"
#include "coro_uring/promise.h"
#include "coro_uring/debug.h"

namespace coro_uring {
namespace internal {

template <typename ValueT, typename ReturnObjectT>
class FuturePromise
    : public PromiseBase<ValueT, ReturnObjectT, std::suspend_never> {
 public:
  template <typename Arg>
  void return_value(Arg value) {
    TRACE_FUNCTION() << value;
    this->SetValue(std::forward<Arg>(value));
  }
};

template <typename ReturnObjectT>
class FuturePromise<void, ReturnObjectT>
    : public PromiseBase<void, ReturnObjectT, std::suspend_never> {
 public:
  void return_void() { TRACE_FUNCTION(); }
};

template <typename ValueT, typename FutureT>
class FutureBase {
 public:
  using promise_type = FuturePromise<ValueT, FutureT>;

  explicit FutureBase(std::coroutine_handle<promise_type> handle)
      : handle_(handle) {}

  ~FutureBase() {
    TRACE_FUNCTION();
    if (handle_) {
      handle_.destroy();
    }
  }

  bool await_ready() noexcept {
    return handle_.done();
  }

  void await_suspend(std::coroutine_handle<> handle) {
    TRACE_FUNCTION();
    handle_.promise().SetPrecursor(handle);
  }

  void Detach() {
    handle_.promise().SetDetached(true);
    handle_ = std::coroutine_handle<promise_type>();
  }

 protected:
  std::coroutine_handle<promise_type> handle_;
};

}

template <typename T = void>
class Future : public internal::FutureBase<T, Future<T>> {
 public:
  using internal::FutureBase<T, Future<T>>::FutureBase;
  T&& await_resume() noexcept {
    TRACE_FUNCTION();
    std::optional<T>&& value = this->handle_.promise().GetValue();
    DCHECK(value.has_value());
    return std::move(*value);
  }
};

template <>
class Future<void> : public internal::FutureBase<void, Future<void>> {
 public:
  using internal::FutureBase<void, Future<void>>::FutureBase;
  void await_resume() noexcept {}
};

}

