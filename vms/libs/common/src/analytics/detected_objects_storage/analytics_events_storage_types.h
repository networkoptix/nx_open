#pragma once

#include <chrono>
#include <vector>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/network/http/http_types.h>
#include <nx/sql/types.h>

#include <analytics/common/object_detection_metadata.h>
#include <recording/time_period.h>
#include <utils/common/request_param.h>

namespace nx {
namespace analytics {
namespace storage {

// Using 14 bits for coordinates. That allows using only 2 bytes when compressing the value.
static constexpr int kCoordinatesBits = 14;
static constexpr int kCoordinatesPrecision = (2 << kCoordinatesBits) - 1;
static constexpr int kCoordinateDecimalDigits = nx::common::metadata::kCoordinateDecimalDigits;
// NOTE: If the assertion fails, all constants here must be modified and analytics DB data migrated.
static_assert(kCoordinateDecimalDigits == 4);

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
    QnUuid objectAppearanceId;
    QString objectTypeId;
    /** Persistent object attributes. E.g., license plate number. */
    std::vector<common::metadata::Attribute> attributes;
    qint64 firstAppearanceTimeUsec = 0;
    qint64 lastAppearanceTimeUsec = 0;
    std::vector<ObjectPosition> track;

    bool operator==(const DetectedObject& right) const;
};

#define DetectedObject_analytics_storage_Fields \
    (objectAppearanceId)(objectTypeId)(attributes)(firstAppearanceTimeUsec)(lastAppearanceTimeUsec)(track)
QN_FUSION_DECLARE_FUNCTIONS(DetectedObject, (json)(ubjson));

//-------------------------------------------------------------------------------------------------

struct Filter
{
    /**
     * If empty than any device is matched.
     */
    std::vector<QnUuid> deviceIds;
    std::vector<QString> objectTypeId;
    QnUuid objectAppearanceId;
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

    // TODO: #ak Move result options to a separate struct.

    /**
     * Zero value is treated as no limit.
     */
    int maxObjectsToSelect = 0;
    int maxTrackSize = 1;
    /**
     * Found objects are sorted by minimal track time using this order.
     */
    Qt::SortOrder sortOrder = Qt::SortOrder::DescendingOrder;

    bool operator==(const Filter& right) const;
    bool operator!=(const Filter& right) const;
};

void serializeToParams(const Filter& filter, QnRequestParamList* params);
bool deserializeFromParams(const QnRequestParamList& params, Filter* filter);

::std::ostream& operator<<(::std::ostream& os, const Filter& filter);
QString toString(const Filter& filter);

#define Filter_analytics_storage_Fields \
    (deviceIds)(objectTypeId)(objectAppearanceId)(timePeriod)(boundingBox)(requiredAttributes)(freeText)
QN_FUSION_DECLARE_FUNCTIONS(Filter, (json)(ubjson));

//-------------------------------------------------------------------------------------------------

struct TimePeriodsLookupOptions
{
    /**
     * If distance between two time periods less than this value,
     * then those periods SHOULD be merged ignoring gap.
     */
    std::chrono::milliseconds detailLevel = std::chrono::milliseconds::zero();
};

//-------------------------------------------------------------------------------------------------

enum class ResultCode
{
    ok,
    retryLater,
    error,
};

ResultCode dbResultToResultCode(nx::sql::DBResult dbResult);

nx::network::http::StatusCode::Value toHttpStatusCode(ResultCode resultCode);
ResultCode fromHttpStatusCode(nx::network::http::StatusCode::Value statusCode);

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(ResultCode)

} // namespace storage
} // namespace analytics
} // namespace nx

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (nx::analytics::storage::ResultCode), (lexical))
