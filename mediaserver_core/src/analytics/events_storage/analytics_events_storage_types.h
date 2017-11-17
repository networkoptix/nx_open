#pragma once

#include <boost/optional.hpp>

#include <analytics/common/object_detection_metadata.h>

namespace nx {
namespace mediaserver {
namespace analytics {
namespace storage {

struct TimestampRange
{
    qint64 startUsec = 0;
    qint64 stopUsec = 0;
};

struct Filter
{
    boost::optional<QnUuid> objectTypeId;
    boost::optional<QnUuid> objectId;
    boost::optional<TimestampRange> timestampRange;
    boost::optional<QRectF> boundingBox;
    std::vector<common::metadata::Attribute> requiredAttributes;
    /**
     * Set of words separated by spaces, commas, etc...
     * Search is done across all attributes (names and values).
     */
    QString freeText;
};

enum class ResultCode
{
    ok,
    error,
};

} // namespace storage
} // namespace analytics
} // namespace mediaserver
} // namespace nx
