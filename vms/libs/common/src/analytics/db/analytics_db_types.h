#pragma once

#include <chrono>
#include <optional>
#include <vector>

#include <QtGui/QRegion>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/network/http/http_types.h>
#include <nx/sql/types.h>

#include <analytics/common/object_metadata.h>
#include <recording/time_period.h>
#include <utils/common/request_param.h>
#include <motion/motion_detection.h>
#include <utils/common/byte_array.h>

namespace nx::analytics::db {

struct ObjectPosition
{
    /** Device object has been detected on. */
    QnUuid deviceId;
    qint64 timestampUs = 0;
    qint64 durationUs = 0;
    QRectF boundingBox;
    /** Variable object attributes. E.g., car speed. */
    nx::common::metadata::Attributes attributes;

    bool operator==(const ObjectPosition& right) const;
};

#define ObjectPosition_analytics_storage_Fields \
    (deviceId)(timestampUs)(durationUs)(boundingBox)(attributes)
QN_FUSION_DECLARE_FUNCTIONS(ObjectPosition, (json)(ubjson));

struct ObjectRegion
{
    QByteArray boundingBoxGrid;

    void add(const QRectF& rect);
    void add(const ObjectRegion& region);
    bool intersect(const QRectF& rect) const;
    bool isEmpty() const;
    QRectF boundingBox() const;
    bool isSimpleRect() const;
    void clear();

    bool operator==(const ObjectRegion& right) const;
};

#define ObjectRegion_analytics_storage_Fields (boundingBoxGrid)
QN_FUSION_DECLARE_FUNCTIONS(ObjectRegion, (json)(ubjson));

struct BestShot
{
    qint64 timestampUs = 0;
    QRectF rect;

    bool initialized() const { return timestampUs > 0; }

    bool operator==(const BestShot& right) const;
};

#define BestShot_analytics_storage_Fields \
    (timestampUs)(rect)

QN_FUSION_DECLARE_FUNCTIONS(BestShot, (json)(ubjson));

struct ObjectTrack
{
    /** Device object has been detected on. */
    QnUuid id;
    QnUuid deviceId;
    QString objectTypeId;
    /** Persistent object attributes. E.g., license plate number. */
    nx::common::metadata::Attributes attributes;
    qint64 firstAppearanceTimeUs = 0;
    qint64 lastAppearanceTimeUs = 0;
    ObjectRegion objectPosition;
    BestShot bestShot;

    bool operator==(const ObjectTrack& right) const;
};

struct ObjectTrackEx: public ObjectTrack
{
    ObjectTrackEx(const ObjectTrack& data);
    std::vector<ObjectPosition> objectPositionSequence;
};

#define ObjectTrack_analytics_storage_Fields \
    (id)(deviceId)(objectTypeId)(attributes)(firstAppearanceTimeUs) \
    (lastAppearanceTimeUs)(objectPosition)(bestShot)

QN_FUSION_DECLARE_FUNCTIONS(ObjectTrack, (json)(ubjson));

//-------------------------------------------------------------------------------------------------

struct Filter
{
    /**
     * If empty than any device is matched.
     */
    std::vector<QnUuid> deviceIds;

    // TODO: #mshevchenko Why 'Id' and not `Ids`? And why not `nx::analytics::ObjectTypeId`?
    std::vector<QString> objectTypeId;
    QnUuid objectTrackId;
    QnTimePeriod timePeriod;
    /**
     * Coordinates are in range [0;1].
     */
    std::optional<QRectF> boundingBox;

    /**
     * Set of words separated by spaces, commas, etc...
     * Search is done across all attributes values using wildcards.
     */
    QString freeText;

    // TODO: #ak Move result options to a separate struct.

    /**
     * Zero value is treated as no limit.
     */
    int maxObjectTracksToSelect = 0;
    /**
     * Found tracks are sorted by minimal track time using this order.
     */
    Qt::SortOrder sortOrder = Qt::SortOrder::DescendingOrder;

    Filter();

    bool empty() const;

    bool acceptsObjectType(const QString& typeId) const;
    bool acceptsBoundingBox(const QRectF& boundingBox) const;
    bool acceptsAttributes(const nx::common::metadata::Attributes& attributes) const;
    bool acceptsMetadata(const nx::common::metadata::ObjectMetadata& metadata,
        bool checkBoundingBox = true) const;
    bool acceptsTrack(const ObjectTrack& track) const;

    /**
     * Search is implemented by attribute values only. SqLite fts4 syntax supports only full match
     * or prefix match, so we append `*` to each word of the user input to enable prefix lookup.
     */
    void loadUserInputToFreeText(const QString& userInput);

    /**
     * Search is implemented by attribute values only. SqLite fts4 syntax supports only full match
     * or prefix match, so we append `*` to each word of the user input to enable prefix lookup.
     */
    static QString userInputToFreeText(const QString& userInput);

    bool operator==(const Filter& right) const;
    bool operator!=(const Filter& right) const;
};

void serializeToParams(const Filter& filter, QnRequestParamList* params);
bool deserializeFromParams(const QnRequestParamList& params, Filter* filter);

::std::ostream& operator<<(::std::ostream& os, const Filter& filter);
QString toString(const Filter& filter);

#define Filter_analytics_storage_Fields \
    (deviceIds)(objectTypeId)(objectTrackId)(timePeriod)(boundingBox)(freeText)
QN_FUSION_DECLARE_FUNCTIONS(Filter, (json));

//-------------------------------------------------------------------------------------------------

using LookupResult = std::vector<ObjectTrack>;

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

} // namespace nx::analytics::db

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((nx::analytics::db::ResultCode), (lexical))
