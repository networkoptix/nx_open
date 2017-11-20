#pragma once

#include <analytics/common/object_detection_metadata.h>

#include "analytics_events_storage_types.h"

namespace nx {
namespace mediaserver {
namespace analytics {
namespace storage {

class AbstractCursor
{
public:
    virtual ~AbstractCursor() = default;

    /**
     * @return boost::none if at end or error has occured.
     */
    virtual common::metadata::ConstDetectionMetadataPacketPtr next() = 0;
};

//-------------------------------------------------------------------------------------------------
class Cursor:
    public AbstractCursor
{
public:
    virtual common::metadata::ConstDetectionMetadataPacketPtr next() override;
};

} // namespace storage
} // namespace analytics
} // namespace mediaserver
} // namespace nx
