#pragma once

#include <chrono>
#include <limits>
#include <optional>

#include <glog/logging.h>
#include <liburing.h>
#include <liburing/io_uring.h>

#include "coro_uring/coroutines_compat.h"
#include "coro_uring/future.h"

namespace coro_uring {

class IOService {
 public:
  struct Options {
    int queue_size = 2048;

    // SQEs are not submitted immediately in `Execute`, they are submitted later
    // in batch (in next "iteration", after handling other pending CQEs) to save
    // syscall. The following two options controls when should we submit them.
    // They will be submitted when either of the threshold matches.

    // Submit when this number of SQEs are pending. By default, it's not set and
    // it's effectively limited by queue_size. You may want to set this to a
    // smaller value than queue_size if you want to execute multiple commands at
    // the same time in one coroutine without awaiting them one by one.
    int sq_submit_count_threshold = std::numeric_limits<int>::max();
    // Submit when any of the SQEs is delayed for longer than this duration.
    int64_t sq_submit_delay_threshold_us = 1000;
  };

  explicit IOService(const Options& options);
  ~IOService();

  void RunUntilFinish();

  template <typename SQE_PREPARE_FN>
  Future<int32_t> Execute(const SQE_PREPARE_FN& seq_prepare_fn) {
    struct io_uring_sqe* sqe = CHECK_NOTNULL(io_uring_get_sqe(&ring_));
    seq_prepare_fn(sqe);
    return ExecuteInternal(sqe);
  }

 private:
  Future<int32_t> ExecuteInternal(struct io_uring_sqe* sqe);

  Options options_;
  io_uring ring_;

  int pending_count_ = 0;  // number of operations that we havn't received CQE
  // The timestamp of the ealiest SQE that is not yet submitted
  std::optional<std::chrono::steady_clock::time_point> earliest_queued_sqe_ts_;
};
}
