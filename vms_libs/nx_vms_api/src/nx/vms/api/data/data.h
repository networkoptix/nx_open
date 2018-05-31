#pragma once

#include <nx/vms/api/data_fwd.h>

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API Data
{
    Data() = default;
    Data(const Data&) = default;
    Data(Data&&) = default;
    virtual ~Data() = default;

    Data& operator=(const Data&) = default;
    Data& operator=(Data&&) = default;
};

} // namespace api
} // namespace vms
} // namespace nx
