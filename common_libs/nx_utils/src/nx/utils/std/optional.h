#pragma once

#if defined(__APPLE__)
#include <experimental/optional>

namespace std {

using experimental::optional;
using experimental::nullopt;

} // namespace std

#else
#include <optional>
#endif
