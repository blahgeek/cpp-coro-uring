#pragma once

#include <glog/logging.h>

#define TRACE_FUNCTION() LOG(INFO) << __PRETTY_FUNCTION__ << "; "
