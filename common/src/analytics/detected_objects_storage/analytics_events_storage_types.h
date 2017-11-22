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

struct DetectionEvent
{
    QnUuid deviceId;
    qint64 timestampUsec = 0;
    qint64 durationUsec = 0;
    common::metadata::DetectedObject object;

    bool operator==(const DetectionEvent& right) const;
};

#define DetectionEvent_analytics_storage_Fields (deviceId)(timestampUsec)(durationUsec)(object)
QN_FUSION_DECLARE_FUNCTIONS(DetectionEvent, (json)(ubjson));

//-------------------------------------------------------------------------------------------------

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

    bool operator==(const Filter& right) const;
};

void serializeToParams(const Filter& filter, QnRequestParamList* params);
bool deserializeFromParams(const QnRequestParamList& params, Filter* filter);

#define Filter_analytics_storage_Fields \
    (objectTypeId)(objectId)(timePeriod)(boundingBox)(requiredAttributes)(freeText)
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
