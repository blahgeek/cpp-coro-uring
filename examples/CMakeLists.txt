link_libraries(glog::glog)
link_libraries(coro_uring)

add_executable(coroutine-basic coroutine-basic.cc)

add_executable(cat cat.cc)
