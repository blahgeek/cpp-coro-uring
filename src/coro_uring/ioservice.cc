#include "coro_uring/ioservice.h"

#include <memory>

#include <glog/logging.h>
#include <liburing.h>
#include <liburing/io_uring.h>

#include "coro_uring/debug.h"
#include "coro_uring/future.h"

namespace coro_uring {

namespace {

struct Awaitable {
  ~Awaitable() {
    CHECK(result_.has_value())
        << "Cannot destroy IOService::Awaitable while it's executing";
  }
  Awaitable() = default;
  Awaitable(Awaitable&) = delete;
  Awaitable(Awaitable&&) = delete;

  bool await_ready() noexcept { return result_.has_value(); }
  int32_t await_resume() noexcept {
    DCHECK(result_.has_value());
    return result_.value();
  }
  void await_suspend(std::coroutine_handle<> handle) {
    DCHECK(!precursor_);
    precursor_ = handle;
  }

  std::optional<int32_t> result_;
  std::coroutine_handle<> precursor_;
};

}

IOService::IOService(const IOService::Options& options) {
  PCHECK(io_uring_queue_init(options.queue_size, &ring_, 0) == 0);
}

IOService::~IOService() {
  io_uring_queue_exit(&ring_);
}

Future<int32_t> IOService::ExecuteInternal(struct io_uring_sqe* sqe) {
  Awaitable awaitable;
  io_uring_sqe_set_data(sqe, &awaitable);
  io_uring_submit(&ring_);  // TODO: maybe add an option to not submit it
  pending_count_ += 1;
  TRACE_FUNCTION() << "Submit";

  co_return (co_await awaitable);
}

void IOService::RunUntilFinish() {
  while (pending_count_ > 0) {
    struct io_uring_cqe* cqe = nullptr;
    PCHECK(io_uring_wait_cqe(&ring_, &cqe) == 0);

    TRACE_FUNCTION() << cqe->res;
    Awaitable* awaitable = static_cast<Awaitable*>(io_uring_cqe_get_data(cqe));
    awaitable->result_ = cqe->res;
    io_uring_cqe_seen(&ring_, cqe);
    pending_count_ -= 1;

    if (awaitable->precursor_) {
      awaitable->precursor_();
    }
  }
}

}
