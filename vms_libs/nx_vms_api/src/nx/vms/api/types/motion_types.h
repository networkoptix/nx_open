#pragma once

#include <nx/vms/api/types_fwd.h>

namespace nx {
namespace vms {
namespace api {

enum class MotionStreamType
{
    automatic,
    primary,
    secondary,
    edge
};
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(MotionStreamType)

} // namespace api
} // namespace vms
} // namespace nx
