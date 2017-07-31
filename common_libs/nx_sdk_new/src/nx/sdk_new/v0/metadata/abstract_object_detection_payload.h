#pragma once

#include <nx/sdk_new/v0/metadata/abstract_meta_data_payload.h>

namespace nx {
namespace sdk {
namespace v0 {
namespace metadata {

class AbstractObjectDetectionPayload: public AbstractMetaDataPayload
{
    virtual ~AbstractObjectDetectionPayload() {}

    virtual int detectedObjectCount() const = 0;

    virtual int detectedObjects(
        nx::sdk::v0::metadata::DetectedObject detectedObjects[],
        int maxDetectedObjects) const = 0;
};

} // namespace metadata
} // namespace v0
} // namespace sdk
} // namespace nx
