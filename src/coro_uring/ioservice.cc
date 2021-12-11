#include "coro_uring/ioservice.h"

#include <chrono>
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

IOService::IOService(const IOService::Options& options): options_(options) {
  struct io_uring_params params;
  memset(&params, 0, sizeof(params));
  PCHECK(io_uring_queue_init_params(options_.queue_size, &ring_, &params) == 0);

  LOG_IF(ERROR, !(params.features & IORING_FEAT_NODROP))
      << "NODROP feature is not supported by kernel, this class may not work "
         "properly when events overflow";
}

IOService::~IOService() {
  io_uring_queue_exit(&ring_);
}

Future<int32_t> IOService::ExecuteInternal(struct io_uring_sqe* sqe) {
  Awaitable awaitable;
  io_uring_sqe_set_data(sqe, &awaitable);

  pending_count_ += 1;
  if (!earliest_queued_sqe_ts_.has_value()) {
    earliest_queued_sqe_ts_ = std::chrono::steady_clock::now();
  }

  co_return (co_await awaitable);
}

void IOService::RunUntilFinish() {
  while (pending_count_ > 0) {
    struct io_uring_cqe* cqe = nullptr;
    if (io_uring_sq_ready(&ring_) > 0) {
      PCHECK(io_uring_submit_and_wait(&ring_, 1) > 0);
      earliest_queued_sqe_ts_.reset();
    } else {
      PCHECK(io_uring_wait_cqe(&ring_, &cqe) == 0);
    }

    DCHECK_EQ(io_uring_sq_ready(&ring_), 0);

    int cq_head = 0;
    int cq_count = 0;
    io_uring_for_each_cqe(&ring_, cq_head, cqe) {
      Awaitable* awaitable = static_cast<Awaitable*>(io_uring_cqe_get_data(cqe));
      DCHECK_NOTNULL(awaitable);
      awaitable->result_ = cqe->res;
      if (awaitable->precursor_) {
        awaitable->precursor_();
      }

      pending_count_ -= 1;
      cq_count += 1;

      int sq_current_count = io_uring_sq_ready(&ring_);
      if (sq_current_count >= options_.sq_submit_count_threshold ||
          sq_current_count >= *ring_.sq.kring_entries) {
        break;
      }

      if (earliest_queued_sqe_ts_.has_value()) {
        auto elapsed =
            std::chrono::steady_clock::now() - earliest_queued_sqe_ts_.value();
        int64_t elapsed_us =
            std::chrono::duration_cast<std::chrono::microseconds>(elapsed)
                .count();
        if (elapsed_us > options_.sq_submit_delay_threshold_us) {
          break;
        }
      }
    }
    io_uring_cq_advance(&ring_, cq_count);
  }
}

}
