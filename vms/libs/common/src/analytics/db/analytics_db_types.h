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
#include <utils/common/byte_array.h>

#include "abstract_object_type_dictionary.h"
#include "text_search_utils.h"
#include <nx/utils/latin1_array.h>

class QnResourcePool;

namespace nx::analytics::db {

struct ObjectPosition
{
    /** Device the object has been detected on. */
    QnUuid deviceId;

    qint64 timestampUs = 0;
    qint64 durationUs = 0;
    QRectF boundingBox;

    /** Variable object attributes. E.g., car speed. */
    nx::common::metadata::Attributes attributes;
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

struct Image
{
    QnLatin1Array imageDataFormat;
    QByteArray imageData;

    bool isEmpty() const { return imageData.isEmpty();  }
};
#define Image_analytics_storage_Fields (imageDataFormat)(imageData)
QN_FUSION_DECLARE_FUNCTIONS(Image, (json)(ubjson));

struct BestShot
{
    qint64 timestampUs = 0;
    QRectF rect = QRectF{-1, -1, -1, -1};
    nx::vms::api::StreamIndex streamIndex = nx::vms::api::StreamIndex::undefined;
    Image image;

    bool initialized() const { return timestampUs > 0; }
};

#define BestShot_analytics_storage_Fields \
    (timestampUs)(rect)(streamIndex)(image)

QN_FUSION_DECLARE_FUNCTIONS(BestShot, (json)(ubjson));

struct BestShotEx: public BestShot
{
    QnUuid deviceId;
};
#define BestShotEx_analytics_storage_Fields BestShot_analytics_storage_Fields(deviceId)
QN_FUSION_DECLARE_FUNCTIONS(BestShotEx, (json)(ubjson));

struct ObjectTrack
{
    /** Device object has been detected on. */
    QnUuid id;
    QnUuid deviceId;
    QString objectTypeId; //< TODO: #rvasilenko Rename to bestShotObjectTypeId
    nx::common::metadata::Attributes attributes; //< TODO: #rvasilenko Rename to bestShotAttributes
    qint64 firstAppearanceTimeUs = 0;
    qint64 lastAppearanceTimeUs = 0;
    ObjectRegion objectPosition;
    BestShot bestShot;
};

struct ObjectTrackEx: public ObjectTrack
{
    std::vector<ObjectPosition> objectPositionSequence;

    ObjectTrackEx() = default;
    ObjectTrackEx(const ObjectTrack& data);
};

#define ObjectTrack_analytics_storage_Fields \
    (id)(deviceId)(objectTypeId)(attributes)(firstAppearanceTimeUs) \
    (lastAppearanceTimeUs)(objectPosition)(bestShot)

#define ObjectTrackEx_analytics_storage_Fields \
    ObjectTrack_analytics_storage_Fields (objectPositionSequence)

QN_FUSION_DECLARE_FUNCTIONS(ObjectTrack, (json)(ubjson));
QN_FUSION_DECLARE_FUNCTIONS(ObjectTrackEx, (json)(ubjson));

//-------------------------------------------------------------------------------------------------

struct Filter
{
    enum Option
    {
        none = 0x0,
        ignoreTextFilter = 0x1,
        ignoreBoundingBox = 0x2,
        ignoreTimePeriod = 0x4,
    };

    Q_DECLARE_FLAGS(Options, Option)

    /** If empty, any device is matched. */
    std::vector<QnUuid> deviceIds;

    // TODO: #mshevchenko Why 'Id' and not `Ids`? And why not `nx::analytics::ObjectTypeId`?
    std::vector<QString> objectTypeId;

    QnUuid objectTrackId;
    QnTimePeriod timePeriod;

    /** Coordinates in range [0;1]. */
    std::optional<QRectF> boundingBox;

    /**
     * Set of words separated by spaces, commas, etc. The search is done across all attribute
     * values, using wildcards.
     */
    QString freeText;

    // TODO: #ak Move result options to a separate struct.

    /** Zero value is treated as no limit. */
    int maxObjectTracksToSelect = 0;

    /** If true, track details (geometry data) will be selected. */
    bool needFullTrack = false;

    /** Found tracks are sorted by the minimum track time using this order. */
    Qt::SortOrder sortOrder = Qt::SortOrder::DescendingOrder;

    /** If true, track with best shots only will be selected. */
    bool withBestShotOnly = false;

    Filter();

    bool empty() const;

    bool acceptsMetadata(
        const QnUuid& deviceId,
        const nx::common::metadata::ObjectMetadata& metadata,
        const AbstractObjectTypeDictionary& objectTypeDictionary,
        bool checkBoundingBox = true) const;

    bool acceptsTrack(
        const ObjectTrack& track,
        const AbstractObjectTypeDictionary& objectTypeDictionary,
        Options options = Option::none) const;

    bool acceptsTrackEx(
        const ObjectTrackEx& track,
        const AbstractObjectTypeDictionary& objectTypeDictionary,
        Options options = Option::none) const;

    bool operator==(const Filter& right) const;
    bool operator!=(const Filter& right) const;

private:
    template <typename ObjectTrackType>
    bool acceptsTrackInternal(
        const ObjectTrackType& track,
        const AbstractObjectTypeDictionary& objectTypeDictionary,
        Options options) const;

    bool matchText(
        TextMatcher* textMatcher,
        const ObjectTrack& track,
        const AbstractObjectTypeDictionary& objectTypeDictionary) const;
};

void serializeToParams(const Filter& filter, QnRequestParamList* params);
bool deserializeFromParams(const QnRequestParamList& params, Filter* filter,
    QnResourcePool* resourcePool);

::std::ostream& operator<<(::std::ostream& os, const Filter& filter);
QString toString(const Filter& filter);

#define Filter_analytics_storage_Fields \
    (deviceIds)(objectTypeId)(objectTrackId)(timePeriod)(boundingBox)(freeText)\
    (maxObjectTracksToSelect)(needFullTrack)(sortOrder)(withBestShotOnly)
QN_FUSION_DECLARE_FUNCTIONS(Filter, (json));

//-------------------------------------------------------------------------------------------------

using LookupResult = std::vector<ObjectTrackEx>;

//-------------------------------------------------------------------------------------------------

struct TimePeriodsLookupOptions
{
    /**
     * If distance between two time periods less than this value, then those periods must be merged
     * ignoring the gap.
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
