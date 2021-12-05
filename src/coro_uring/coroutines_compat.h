#pragma once

#ifdef __clang__

#include <experimental/coroutine>

namespace std {
using std::experimental::suspend_always;
using std::experimental::suspend_never;
using std::experimental::coroutine_handle;
using std::experimental::noop_coroutine;
}

#else

#include <coroutine>

#endif
