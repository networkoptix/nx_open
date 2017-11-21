#pragma once

#include <vector>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/db/types.h>

#include <analytics/common/object_detection_metadata.h>
#include <recording/time_period.h>

namespace nx {
namespace mediaserver {
namespace analytics {
namespace storage {

struct DetectionEvent
{
    QnUuid deviceId;
    qint64 timestampUsec = 0;
    qint64 durationUsec = 0;
    common::metadata::DetectedObject object;

    bool operator==(const DetectionEvent& right) const;
};

struct Filter
{
    std::vector<QnUuid> objectTypeId;
    QnUuid objectId;
    QnTimePeriod timePeriod;
    /**
     * Coordinates are in range [0;1].
     */
    QRectF boundingBox;
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
    retryLater,
    error,
};

ResultCode dbResultToResultCode(nx::utils::db::DBResult dbResult);

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(ResultCode)

} // namespace storage
} // namespace analytics
} // namespace mediaserver
} // namespace nx

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (nx::mediaserver::analytics::storage::ResultCode), (lexical))
