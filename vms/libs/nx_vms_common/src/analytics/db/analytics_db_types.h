// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <optional>
#include <set>
#include <vector>

#include <analytics/common/object_metadata.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/network/http/http_types.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/byte_array.h>
#include <nx/utils/json/qt_core_types.h>
#include <nx/utils/json/qt_geometry_reflect.h>
#include <nx/utils/latin1_array.h>
#include <nx/utils/url_query.h>
#include <nx/vms/api/data/analytics_data.h>
#include <recording/time_period.h>

#include "abstract_object_type_dictionary.h"
#include "text_search_utils.h"

class QnResourcePool;

namespace nx::analytics::taxonomy { class AbstractStateWatcher; }
namespace nx::network::rest { class Params; }

namespace nx::analytics::db {

struct ObjectPosition
{
    /**%apidoc Id of a Device the Object has been detected on. */
    nx::Uuid deviceId;

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

struct NX_VMS_COMMON_API ObjectRegion: nx::vms::api::BaseObjectRegion
{
    void add(const QRectF& rect);
    void add(const ObjectRegion& region);
    bool intersect(const QRectF& rect) const;
    bool isEmpty() const;
    QRect boundingBox() const;
    bool isSimpleRect() const;
    void clear();

    bool operator==(const ObjectRegion& right) const;
};
#define ObjectRegion_analytics_storage_Fields BaseObjectRegion_Fields
QN_FUSION_DECLARE_FUNCTIONS(ObjectRegion, (json)(ubjson), NX_VMS_COMMON_API);
NX_REFLECTION_INSTRUMENT(ObjectRegion, ObjectRegion_analytics_storage_Fields)

struct Image
{
    QnLatin1Array imageDataFormat;

    /**%apidoc[unused]
     * %// In the Legacy API, the image is retrieved via /ec2/analyticsTrackBestShot instead.
     */
    QByteArray imageData;

    bool isEmpty() const { return imageData.isEmpty();  }
};
#define Image_analytics_storage_Fields (imageDataFormat)(imageData)
QN_FUSION_DECLARE_FUNCTIONS(Image, (json)(ubjson), NX_VMS_COMMON_API);
NX_REFLECTION_INSTRUMENT(Image, Image_analytics_storage_Fields)

NX_REFLECTION_ENUM_CLASS(ImageType,
    notDefined,
    bestShot,
    title
);

struct ImageEx: Image
{
    ImageType imageType = ImageType::notDefined;
    nx::utils::Url url;
};

struct BaseTrackImage
{
    qint64 timestampUs = -1;
    QRectF rect = QRectF{-1, -1, -1, -1};
    nx::vms::api::StreamIndex streamIndex = nx::vms::api::StreamIndex::undefined;
    std::optional<Image> image;
};
#define BaseTrackImage_analytics_storage_Fields (timestampUs)(rect)(streamIndex)(image)
QN_FUSION_DECLARE_FUNCTIONS(BaseTrackImage, (json)(ubjson), NX_VMS_COMMON_API);
NX_REFLECTION_INSTRUMENT(BaseTrackImage, BaseTrackImage_analytics_storage_Fields)

struct BestShot: public BaseTrackImage
{
    // Keep timestamp 0 by default for compatibility with previous version.
    BestShot(): BaseTrackImage() { timestampUs = 0; };
    BestShot(const BaseTrackImage& image): BaseTrackImage(image) { timestampUs = 0; }

    bool initialized() const { return timestampUs > 0; }
};

struct TrackImage: public BaseTrackImage
{
    nx::Uuid deviceId;

    TrackImage() = default;

    TrackImage(const BaseTrackImage& bestShot):
        BaseTrackImage(bestShot)
    {
    }
};
#define TrackImage_analytics_storage_Fields BaseTrackImage_analytics_storage_Fields(deviceId)
QN_FUSION_DECLARE_FUNCTIONS(TrackImage, (json)(ubjson));
NX_REFLECTION_INSTRUMENT(TrackImage, TrackImage_analytics_storage_Fields)

struct Title: public BaseTrackImage
{
    Title() = default;
    Title(const BaseTrackImage& image): BaseTrackImage(image) {}

    QString text;
    bool hasImage = false;
};
#define Title_analytics_storage_Fields BaseTrackImage_analytics_storage_Fields (text)(hasImage)
QN_FUSION_DECLARE_FUNCTIONS(Title, (json)(ubjson), NX_VMS_COMMON_API);
NX_REFLECTION_INSTRUMENT(Title, Title_analytics_storage_Fields)

struct ObjectTrack
{
    nx::Uuid id;

    /**%apidoc Id of a Device the Object has been detected on. */
    nx::Uuid deviceId;

    /**%apidoc Id of an Object Type of the last received Object Metadata in the Track. */
    QString objectTypeId;

    /**%apidoc
     * Aggregated Attributes of all Object Metadata items in the Track. The aggregation rules are:
     * <ul>
     * <li>For Attributes which values are detected to be numbers (either integer or fractional),
     *     all values are converted into a range represented as a string in the format:
     *     `<opening> <start_number> ... <end_number> <closing>`, where <opening>/<closing> are
     *     either `[`/`]` (inclusive) or `(`/`]` (exclusive).</li>
     * <li>For Attributes of other types, all different values are kept separately.</li>
     * </ul>
     */
    nx::common::metadata::Attributes attributes;

    qint64 firstAppearanceTimeUs = 0;

    qint64 lastAppearanceTimeUs = 0;

    ObjectRegion objectPosition;

    BestShot bestShot;
    std::optional<Title> title;

    nx::Uuid analyticsEngineId;

    /**%apidoc
     * Groups the data under the specified value. I.e., it allows to store multiple independent
     * sets of data in a single DB instead of having to create a separate DB instance for each
     * unique value of the storageId.
     */
    std::string storageId;
};
#define ObjectTrack_analytics_storage_Fields \
    (id)(deviceId)(objectTypeId)(attributes)(firstAppearanceTimeUs)(lastAppearanceTimeUs) \
    (objectPosition)(bestShot)(title)(analyticsEngineId)(storageId)
QN_FUSION_DECLARE_FUNCTIONS(ObjectTrack, (json)(ubjson), NX_VMS_COMMON_API);
NX_REFLECTION_INSTRUMENT(ObjectTrack, ObjectTrack_analytics_storage_Fields)

// TODO: #mshevchenko Rename the old structs to Legacy...

struct NX_VMS_COMMON_API ObjectTrackEx: public ObjectTrack
{
    std::vector<ObjectPosition> objectPositionSequence;

    ObjectTrackEx() = default;
    ObjectTrackEx(const ObjectTrack& data);
};
#define ObjectTrackEx_analytics_storage_Fields \
    ObjectTrack_analytics_storage_Fields(objectPositionSequence)
QN_FUSION_DECLARE_FUNCTIONS(ObjectTrackEx, (json)(ubjson), NX_VMS_COMMON_API);
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

    /**
     * Groups data under specified value. I.e., it allows to store multiple independent sets of
     * data in a single DB instead of having to create a separate DB instance for each unique
     * value of the storageId.
     */
    std::string storageId;

    /** If empty, any device is matched. */
    std::set<nx::Uuid> deviceIds;

    std::set<QString> objectTypeId;

    nx::Uuid objectTrackId;

    QnTimePeriod timePeriod;

    /** Coordinates in range [0;1]. */
    std::optional<QRectF> boundingBox;

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
    nx::Uuid analyticsEngineId;

    Options options{};

    std::chrono::milliseconds maxAnalyticsDetails{};

    Filter();

    bool empty() const;

    bool acceptsMetadata(
        const nx::Uuid& deviceId,
        const Uuid& analyticsEngineId,
        const nx::common::metadata::ObjectMetadata& metadata,
        const AbstractObjectTypeDictionary& objectTypeDictionary,
        bool checkBoundingBox = true) const;

    /*
     * This context is used for multiple 'acceptsTrackInternal' calls.
     * It allows to perform calculation only once to match multiple tracks.
     */
    struct AcceptTrackContext
    {
        AcceptTrackContext(const QString& text = QString()): textMatcher(text)
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
#define Filter_analytics_storage_Fields \
    (deviceIds)(objectTypeId)(objectTrackId)(timePeriod)(boundingBox)(freeText) \
    (maxObjectTracksToSelect)(needFullTrack)(sortOrder)(analyticsEngineId)(storageId)
QN_FUSION_DECLARE_FUNCTIONS(Filter, (json), NX_VMS_COMMON_API);
NX_REFLECTION_INSTRUMENT(Filter, Filter_analytics_storage_Fields)

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
    std::set<nx::Uuid> deviceIds;

    QnTimePeriod timePeriod;

    /** Coordinates in range [0..1]. */
    std::vector<Region> regions;

    /** Zero value is treated as no limit. */
    int limit = 0;

    /** Found tracks are sorted by the minimum track time using this order. */
    Qt::SortOrder sortOrder = Qt::SortOrder::DescendingOrder;
};
#define MotionFilter_Fields (deviceIds)(timePeriod)(regions)(limit)(sortOrder)
QN_FUSION_DECLARE_FUNCTIONS(MotionFilter, (json), NX_VMS_COMMON_API);

/**
 * Creates a search filter from the URL query parameters.
 * @param resourcePool Convert logical device id to Uuid if the parameter is not null.
 * @param taxonomyStateWatcher Automatically assign all parent types to the search request if the
 *     parameter is not null.
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
