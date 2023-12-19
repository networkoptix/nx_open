// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <optional>
#include <set>
#include <vector>

#include <QtGui/QRegion>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/network/http/http_types.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/serialization/qt_core_types.h>
#include <nx/utils/url_query.h>

#include <analytics/common/object_metadata.h>
#include <common/common_globals.h>
#include <recording/time_period.h>
#include <utils/common/byte_array.h>

#include "abstract_object_type_dictionary.h"
#include "text_search_utils.h"
#include <nx/utils/latin1_array.h>

class QnResourcePool;
namespace nx::analytics::taxonomy { class AbstractStateWatcher; }
namespace nx::network::rest { class Params; }

namespace nx::analytics::db {

struct ObjectPosition
{
    /**%apidoc Id of a Device the Object has been detected on. */
    QnUuid deviceId;

    qint64 timestampUs = 0;
    qint64 durationUs = 0;
    QRectF boundingBox;

    /**%apidoc Attributes of an Object, like a car speed. */
    nx::common::metadata::Attributes attributes;
};
#define ObjectPosition_analytics_storage_Fields \
    (deviceId)(timestampUs)(durationUs)(boundingBox)(attributes)
QN_FUSION_DECLARE_FUNCTIONS(ObjectPosition, (json)(ubjson), NX_VMS_COMMON_API);
NX_REFLECTION_INSTRUMENT(ObjectPosition, ObjectPosition_analytics_storage_Fields)

struct NX_VMS_COMMON_API ObjectRegion
{
    /**%apidoc
     * Roughly defines a region on a video frame where the Object(s) were detected, as an array of
     * bytes encoded in base64.
     * <br/>
     * The region is represented as a grid of 32 rows and 44 columns, stored by columns. The grid
     * coordinate system starts at the top left corner, the row index grows down and the column
     * index grows right. This grid is represented as a contiguous array, each bit of which
     * corresponds to the state of a particular cell of the grid (1 if the region includes the
     * cell, 0 otherwise). The bit index for a cell with coordinates (column, row) can be
     * calculated using the following formula: bitIndex = gridHeight * column + row.
     * <br/>
     * NOTE: This is the same binary format that is used by the VMS for motion detection metadata.
     */
    QByteArray boundingBoxGrid;

    void add(const QRectF& rect);
    void add(const ObjectRegion& region);
    bool intersect(const QRectF& rect) const;
    bool isEmpty() const;
    QRect boundingBox() const;
    bool isSimpleRect() const;
    void clear();

    bool operator==(const ObjectRegion& right) const;
};

#define ObjectRegion_analytics_storage_Fields (boundingBoxGrid)
QN_FUSION_DECLARE_FUNCTIONS(ObjectRegion, (json)(ubjson), NX_VMS_COMMON_API);
NX_REFLECTION_INSTRUMENT(ObjectRegion, ObjectRegion_analytics_storage_Fields)

struct Image
{
    QnLatin1Array imageDataFormat;
    QByteArray imageData;

    bool isEmpty() const { return imageData.isEmpty();  }
};
#define Image_analytics_storage_Fields (imageDataFormat)(imageData)
QN_FUSION_DECLARE_FUNCTIONS(Image, (json)(ubjson), NX_VMS_COMMON_API);
NX_REFLECTION_INSTRUMENT(Image, Image_analytics_storage_Fields)

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

QN_FUSION_DECLARE_FUNCTIONS(BestShot, (json)(ubjson), NX_VMS_COMMON_API);
NX_REFLECTION_INSTRUMENT(BestShot, BestShot_analytics_storage_Fields)

struct BestShotEx: public BestShot
{
    QnUuid deviceId;

    BestShotEx() = default;

    BestShotEx(const BestShot& bestShot):
        BestShot(bestShot)
    {
    }
};
#define BestShotEx_analytics_storage_Fields BestShot_analytics_storage_Fields(deviceId)
QN_FUSION_DECLARE_FUNCTIONS(BestShotEx, (json)(ubjson));
NX_REFLECTION_INSTRUMENT(BestShotEx, BestShotEx_analytics_storage_Fields)

struct ObjectTrack
{
    QnUuid id;

    /**%apidoc Id of a Device the Object has been detected on. */
    QnUuid deviceId;

    /**%apidoc
     * %// TODO: #rvasilenko Rename to bestShotObjectTypeId when used for Best Shots.
     */
    QString objectTypeId;

    nx::common::metadata::Attributes attributes;

    qint64 firstAppearanceTimeUs = 0;

    qint64 lastAppearanceTimeUs = 0;

    ObjectRegion objectPosition;

    BestShot bestShot;

    QnUuid analyticsEngineId;
};

struct NX_VMS_COMMON_API ObjectTrackEx: public ObjectTrack
{
    std::vector<ObjectPosition> objectPositionSequence;

    ObjectTrackEx() = default;
    ObjectTrackEx(const ObjectTrack& data);
};

#define ObjectTrack_analytics_storage_Fields \
    (id)(deviceId)(objectTypeId)(attributes)(firstAppearanceTimeUs)(lastAppearanceTimeUs)( \
        objectPosition)(bestShot)(analyticsEngineId)

#define ObjectTrackEx_analytics_storage_Fields \
    ObjectTrack_analytics_storage_Fields(objectPositionSequence)

QN_FUSION_DECLARE_FUNCTIONS(ObjectTrack, (json)(ubjson), NX_VMS_COMMON_API);
QN_FUSION_DECLARE_FUNCTIONS(ObjectTrackEx, (json)(ubjson), NX_VMS_COMMON_API);

NX_REFLECTION_INSTRUMENT(ObjectTrack, ObjectTrack_analytics_storage_Fields)
NX_REFLECTION_INSTRUMENT(ObjectTrackEx, ObjectTrackEx_analytics_storage_Fields)

//-------------------------------------------------------------------------------------------------

struct NX_VMS_COMMON_API Filter
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
    std::set<QnUuid> deviceIds;

    // TODO: #mshevchenko Why 'Id' and not `Ids`? And why not `nx::analytics::ObjectTypeId`?
    std::set<QString> objectTypeId;

    QnUuid objectTrackId;
    QnTimePeriod timePeriod;

    /** Coordinates in range [0;1]. */
    std::optional<QRectF> boundingBox;
    std::vector<common::metadata::Attribute> requiredAttributes;
    /**
     * Set of words separated by spaces, commas, etc. The search is done across all attribute
     * values, using wildcards.
     */
    QString freeText;

    // TODO: #akolesnikov Move result options to a separate struct.

    /** Zero value is treated as no limit. */
    int maxObjectTracksToSelect = 0;

    /** If true, track details (geometry data) will be selected. */
    bool needFullTrack = false;

    /** Found tracks are sorted by the minimum track time using this order. */
    Qt::SortOrder sortOrder = Qt::SortOrder::DescendingOrder;

    /** Null value treated as any engine. */
    QnUuid analyticsEngineId;
    Options options{};
    std::chrono::milliseconds maxAnalyticsDetails{};

    Filter();

    bool empty() const;

    bool acceptsMetadata(const QnUuid& deviceId,
        const nx::common::metadata::ObjectMetadata& metadata,
        const AbstractObjectTypeDictionary& objectTypeDictionary,
        bool checkBoundingBox = true) const;

    /*
     * This context is used for multiple 'acceptsTrackInternal' calls.
     * It allows to perform calculation only once to match multiple tracks.
     */
    struct AcceptTrackContext
    {
        AcceptTrackContext(const QString& text = QString()):
            textMatcher(text)
        {
        }

        TextMatcher textMatcher;
    };
    std::optional<AcceptTrackContext> context;

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
    template<typename ObjectTrackType>
    bool acceptsTrackInternal(
        const ObjectTrackType& track,
        const AbstractObjectTypeDictionary& objectTypeDictionary,
        Options options) const;

    bool matchText(
        const TextMatcher* textMatcher,
        const ObjectTrack& track,
        const AbstractObjectTypeDictionary& objectTypeDictionary) const;
};

NX_VMS_COMMON_API void serializeToParams(const Filter& filter, nx::network::rest::Params* params);

struct Region
{
    int channelNumber = 0;
    /** Coordinates in range [0;1]. */
    QRectF boundingBox;
};

#define Region_Fields (channelNumber)(boundingBox)
QN_FUSION_DECLARE_FUNCTIONS(Region, (json), NX_VMS_COMMON_API);

struct NX_VMS_COMMON_API MotionFilter
{
    /** If empty, any device is matched. */
    std::set<QnUuid> deviceIds;

    QnTimePeriod timePeriod;

    /** Coordinates in range [0;1]. */
    std::vector<Region> regions;

    /** Zero value is treated as no limit. */
    int limit = 0;

    /** Found tracks are sorted by the minimum track time using this order. */
    Qt::SortOrder sortOrder = Qt::SortOrder::DescendingOrder;
};

#define MotionFilter_Fields (deviceIds)(timePeriod)(regions)(limit)(sortOrder)
QN_FUSION_DECLARE_FUNCTIONS(MotionFilter, (json), NX_VMS_COMMON_API);
/*
 * Create search filter from URL query parameters.
 * @param resourcePool convert logical device id to Uuid if parameter is not null
 * @param taxonomyStateWatcher auto assign all parent types to the search request if parameter is not null
 */
NX_VMS_COMMON_API bool deserializeFromParams(
    const nx::network::rest::Params& params,
    Filter* filter,
    const QnResourcePool* resourcePool,
    const nx::analytics::taxonomy::AbstractStateWatcher* taxonomyStateWatcher);

NX_VMS_COMMON_API bool loadFromUrlQuery(
    const QUrlQuery& urlParams,
    Filter* filter,
    const QnResourcePool* resourcePool,
    const nx::analytics::taxonomy::AbstractStateWatcher* taxonomyStateWatcher);

NX_VMS_COMMON_API void serializeToUrlQuery(const Filter& filter, nx::utils::UrlQuery& urlQuery);

NX_VMS_COMMON_API ::std::ostream& operator<<(::std::ostream& os, const Filter& filter);
NX_VMS_COMMON_API QString toString(const Filter& filter);

NX_VMS_COMMON_API std::set<QString> addDerivedTypeIds(
    const nx::analytics::taxonomy::AbstractStateWatcher* stateWatcher,
    const QList<QString>& objectTypeIdsFromUser);

#define Filter_analytics_storage_Fields \
    (deviceIds)(objectTypeId)(objectTrackId)(timePeriod)(boundingBox)(freeText)\
    (maxObjectTracksToSelect)(needFullTrack)(sortOrder)(analyticsEngineId)
QN_FUSION_DECLARE_FUNCTIONS(Filter, (json), NX_VMS_COMMON_API);

NX_REFLECTION_INSTRUMENT(Filter, Filter_analytics_storage_Fields)

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

QN_FUSION_DECLARE_FUNCTIONS(TimePeriodsLookupOptions, (json), NX_VMS_COMMON_API);

//-------------------------------------------------------------------------------------------------

NX_REFLECTION_ENUM_CLASS(ResultCode,
    ok,
    retryLater,
    error
)

NX_VMS_COMMON_API nx::network::http::StatusCode::Value toHttpStatusCode(ResultCode resultCode);
NX_VMS_COMMON_API ResultCode fromHttpStatusCode(nx::network::http::StatusCode::Value statusCode);

} // namespace nx::analytics::db
