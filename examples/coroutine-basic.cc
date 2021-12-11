#include <iostream>
#include <queue>

#include <glog/logging.h>

#include "coro_uring/future.h"
#include "coro_uring/generator.h"

namespace coro_uring {

class IOService {
 public:
  auto SchedYield() {
    struct awaitable {
      bool await_ready() noexcept { return false; }
      void await_resume() noexcept {}
      void await_suspend(std::coroutine_handle<> handle) {
        ioservice_->handles_.push(handle);
      };

      IOService* ioservice_;
    };
    return awaitable{.ioservice_ = this};
  }

  void RunUntilFinish() {
    while (!handles_.empty()) {
      handles_.front().resume();
      handles_.pop();
    }
  }

 private:
  std::queue<std::coroutine_handle<>> handles_;
};

Generator<int> range(IOService& ioservice, int n) {
  for (int i = 0 ; i < n ; i ++) {
    std::cout << "YIELD " << i << std::endl;
    co_await ioservice.SchedYield();
    co_yield i;
  }
}

Future<int> run(IOService& ioservice) {
  Generator<int> g = range(ioservice, 5);
  while (true) {
    std::optional<int> v = co_await g;
    if (v.has_value()) {
      std::cout << v.value() << std::endl;
    } else {
      break;
    }
  }
  co_return 0;
}

Future<> run2(IOService& ioservice) {
  co_await run(ioservice);
}

}

int main(int argc, char *argv[]) {
  google::InitGoogleLogging(argv[0]);

  LOG(INFO) << "Hello";
  LOG(ERROR) << "World";

  coro_uring::IOService ioservice;
  auto result1 = coro_uring::run2(ioservice);
  auto result2 = coro_uring::run2(ioservice);
  result1.Detach();
  result2.Detach();
  ioservice.RunUntilFinish();
  return 0;
}
