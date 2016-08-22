#include "sync_executor.h"

namespace cf {
void sync_executor::post(const detail::task_type& f) {
  f();
}
}