#include <glog/logging.h>
#include <liburing.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstdlib>

#include "coro_uring/ioservice.h"
#include "coro_uring/future.h"


namespace {

coro_uring::Future<> HandleConnection(coro_uring::IOService& ioservice, int fd) {
  constexpr int kBufferSize = 2048;
  char buffer[kBufferSize];

  while (true) {
    int32_t read_res =
        co_await ioservice.Execute([fd, &buffer](struct io_uring_sqe* sqe) {
          io_uring_prep_recv(sqe, fd, buffer, kBufferSize, 0);
        });
    if (read_res < 0) {
      break;
    }
    int32_t write_res = co_await ioservice.Execute(
        [fd, &buffer, &read_res](struct io_uring_sqe* sqe) {
          io_uring_prep_send(sqe, fd, buffer, read_res, 0);
        });
    if (write_res < 0) {
      break;
    }
  }

  co_await ioservice.Execute([fd](struct io_uring_sqe* sqe) {
    io_uring_prep_shutdown(sqe, fd, SHUT_RDWR);
  });
}

coro_uring::Future<> RunServer(coro_uring::IOService& ioservice, int port) {
  int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
  PCHECK(listen_fd >= 0);

  struct sockaddr_in listen_addr = {0};
  listen_addr.sin_family = AF_INET;
  listen_addr.sin_port = htons(port);
  listen_addr.sin_addr.s_addr = INADDR_ANY;
  PCHECK(bind(listen_fd, (struct sockaddr*)(&listen_addr),
              sizeof(listen_addr)) == 0);
  PCHECK(listen(listen_fd, 16) == 0);

  while (true) {
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    int conn_fd = co_await ioservice.Execute(
        [listen_fd, &client_addr, &client_addr_len](struct io_uring_sqe* sqe) {
          io_uring_prep_accept(sqe, listen_fd, (struct sockaddr*)(&client_addr),
                               &client_addr_len, 0);
        });
    HandleConnection(ioservice, conn_fd).Detach();
  }
}

}

int main(int argc, const char* argv[]) {
  int port = atoi(argv[1]);
  coro_uring::IOService ioservice(coro_uring::IOService::Options{});

  RunServer(ioservice, port).Detach();
  ioservice.RunUntilFinish();

  return 0;
}
