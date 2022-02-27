// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "common.h"

namespace cf {
class sync_executor {
public:
  void post(const detail::task_type& f) {
    f();
  }
};
}
