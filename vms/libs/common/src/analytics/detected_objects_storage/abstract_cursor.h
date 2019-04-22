#pragma once

#include <analytics/common/object_detection_metadata.h>

namespace nx::analytics::storage {

class AbstractCursor
{
public:
    virtual ~AbstractCursor() = default;

    virtual common::metadata::ConstDetectionMetadataPacketPtr next() = 0;

    /**
     * The implementation MUST free the DB cursor in the method and every subsequent
     * AbstractCursor::next() call MUST return nullptr.
     */
    virtual void close() = 0;
};

} // namespace nx::analytics::storage
