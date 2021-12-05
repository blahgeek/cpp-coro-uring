cmake_minimum_required(VERSION 3.14)

project(coro-uring)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  add_compile_options(-stdlib=libc++ -fcoroutines-ts)
  add_link_options(-stdlib=libc++)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  add_compile_options(-fcoroutines)
endif()

# cannot use system-installed glog, because we need link with libc++ for clang
set(WITH_GTEST OFF)
set(WITH_GFLAGS OFF)
set(BUILD_TESTING OFF)
add_subdirectory(glog)

find_library(LIBURING_LIBRARY uring REQUIRED)
find_path(LIBURING_INCLUDE_DIR NAMES liburing.h)
include_directories(${LIBURING_INCLUDE_DIR})
link_libraries(${LIBURING_LIBRARY})

include_directories(./src)

add_subdirectory(./src)
add_subdirectory(./examples)
