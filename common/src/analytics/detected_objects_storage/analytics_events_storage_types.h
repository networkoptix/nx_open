#pragma once

#include <vector>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/network/http/http_types.h>
#include <nx/utils/db/types.h>

#include <analytics/common/object_detection_metadata.h>
#include <recording/time_period.h>
#include <utils/common/request_param.h>

namespace nx {
namespace analytics {
namespace storage {

struct ObjectPosition
{
    /** Device object has been detected on. */
    QnUuid deviceId;
    qint64 timestampUsec = 0;
    qint64 durationUsec = 0;
    QRectF boundingBox;
    /** Variable object attributes. E.g., car speed. */
    std::vector<common::metadata::Attribute> attributes;

    bool operator==(const ObjectPosition& right) const;
};

#define ObjectPosition_analytics_storage_Fields \
    (deviceId)(timestampUsec)(durationUsec)(boundingBox)(attributes)
QN_FUSION_DECLARE_FUNCTIONS(ObjectPosition, (json)(ubjson));

struct DetectedObject
{
    QnUuid objectId;
    QnUuid objectTypeId;
    /** Persistent object attributes. E.g., license plate number. */
    std::vector<common::metadata::Attribute> attributes;
    std::vector<ObjectPosition> track;

    bool operator==(const DetectedObject& right) const;
};

#define DetectedObject_analytics_storage_Fields \
    (objectId)(objectTypeId)(attributes)(track)
QN_FUSION_DECLARE_FUNCTIONS(DetectedObject, (json)(ubjson));

//-------------------------------------------------------------------------------------------------

struct Filter
{
    QnUuid deviceId;
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
    /**
     * Zero value is treated as no limit.
     */
    int maxObjectsToSelect = 0;

    bool operator==(const Filter& right) const;
    bool operator!=(const Filter& right) const;
};

void serializeToParams(const Filter& filter, QnRequestParamList* params);
bool deserializeFromParams(const QnRequestParamList& params, Filter* filter);

::std::ostream& operator<<(::std::ostream& os, const Filter& filter);

#define Filter_analytics_storage_Fields \
    (deviceId)(objectTypeId)(objectId)(timePeriod)(boundingBox)(requiredAttributes)(freeText)
QN_FUSION_DECLARE_FUNCTIONS(Filter, (json)(ubjson));

//-------------------------------------------------------------------------------------------------

enum class ResultCode
{
    ok,
    retryLater,
    error,
};

ResultCode dbResultToResultCode(nx::utils::db::DBResult dbResult);

nx_http::StatusCode::Value toHttpStatusCode(ResultCode resultCode);
ResultCode fromHttpStatusCode(nx_http::StatusCode::Value statusCode);

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(ResultCode)

} // namespace storage
} // namespace analytics
} // namespace nx

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (nx::analytics::storage::ResultCode), (lexical))
