#pragma once

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
    int queue_size = 128;
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

  io_uring ring_;
  int64_t pending_count_ = 0;
};

}
