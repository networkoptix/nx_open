#pragma once

#if defined(__APPLE__)
#include <experimental/optional>

namespace std {

using experimental::optional;

} // namespace std

#else
#include <optional>
#endif
