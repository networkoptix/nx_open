#pragma once

#include <nx/sdk_new/v0/metadata/abstract_meta_data_payload.h>

namespace nx {
namespace sdk {
namespace v0 {
namespace metadata {

class AbstractEventPayload: public AbstractMetaDataPayload
{
    virtual ~AbstractEventPayload() {}

    virtual void caption(char* buffer, int bufferSize) const = 0;

    virtual void description(char* buffer, int bufferSize) const = 0;
};

} // namespace metadata
} // namespace v0
} // namespace sdk
} // namespace nx
