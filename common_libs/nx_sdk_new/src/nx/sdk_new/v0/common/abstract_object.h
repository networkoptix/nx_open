#pragma once

#include <nx/sdk_new/v0/common/guid.h>

namespace nx {
namespace sdk {
namespace v0 {

class AbstractObject
{
    // TODO #dmishin define needed GUIDS;
    virtual void* queryInterface(const Guid& interfaceId) = 0;

    virtual void release() = 0;

};

} // namespace v0
} // namespace sdk
} // namespace nx
