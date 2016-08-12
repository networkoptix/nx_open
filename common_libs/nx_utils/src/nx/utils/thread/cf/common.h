#pragma once

#include <functional>

namespace cf {
namespace detail {
  using task_type = std::function<void()>;
}
}