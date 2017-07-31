#pragma once

#include <nx/sdk_new/v0/metadata/abstract_meta_data_payload.h>

namespace nx {
namespace sdk {
namespace v0 {
namespace metadata {

class AbstractMotionPayload: public AbstractMetaDataPayload
{
    virtual ~AbstractMotionPayload() {}

    // TODO: #dmishin interface for motion payload;
};

} // namepace metadata
} // namespace v0
} // namespace sdk
} // namespace nx
