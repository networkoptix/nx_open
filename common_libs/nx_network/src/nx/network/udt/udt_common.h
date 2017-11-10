#pragma once

#include <nx/utils/system_error.h>

namespace nx {
namespace network {
namespace detail {

SystemError::ErrorCode convertToSystemError(int udtErrorCode);

} // namespace detail
} // namespace network
} // namespace nx
