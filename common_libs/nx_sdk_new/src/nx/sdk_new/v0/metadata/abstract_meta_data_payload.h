#pragma once

#include <nx/sdk_new/v0/common/abstract_object.h>

namespace nx {
namespace sdk {
namespace v0 {
namespace metadata {

class AbstractMetaDataPayload: public AbstractObject
{
    bool deserialize(const uint8_t* buffer, int bufferSize) = 0;

    bool serialize(uint8_t* outBuffer, int bufferSize) const = 0;
};

} // namespace metadata
} // namespace v0
} // namespace sdk
} // namespace nx
