#include <iostream>
#include <string>
#include <string_view>

#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <glog/logging.h>
#include <liburing.h>

#include "coro_uring/future.h"
#include "coro_uring/ioservice.h"

namespace {

coro_uring::Future<int> Cat(coro_uring::IOService& ioservice,
                            std::string_view filepath) {
  int fd = open(filepath.data(), O_RDONLY);
  PCHECK(fd >= 0) << ": open " << filepath;

  constexpr int kBufferSize = 4096;
  char buffer[kBufferSize];

  for (int64_t offset = 0 ; ; offset += kBufferSize) {
    int32_t read_result =
        co_await ioservice.Execute([&](struct io_uring_sqe* sqe) {
          io_uring_prep_read(sqe, fd, buffer, kBufferSize, offset);
        });
    if (read_result <= 0) {
      break;
    }

    ioservice
        .Execute([&](struct io_uring_sqe* sqe) {
          io_uring_prep_write(sqe, STDOUT_FILENO, buffer, kBufferSize, 0);
        })
        .Detach();
  }

  PCHECK(close(fd) == 0);
  co_return 0;
}
}

int main(int argc, char* argv[]) {
  google::InitGoogleLogging(argv[0]);

  coro_uring::IOService ioservice(coro_uring::IOService::Options{});
  Cat(ioservice, argv[1]).Detach();

  ioservice.RunUntilFinish();
  return 0;
}
