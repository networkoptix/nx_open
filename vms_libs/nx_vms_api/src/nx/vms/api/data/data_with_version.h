#pragma once

#include "data.h"

namespace nx {
namespace vms {
namespace api {

struct DataWithVersion: Data
{
    int version = 0;
};
#define DataWithVersion_Fields (version)

} // namespace api
} // namespace vms
} // namespace nx
