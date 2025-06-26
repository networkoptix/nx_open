// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include <camera/camera_plugin.h>
#include <nx/kit/json.h>
#include <nx/sdk/cloud_storage/i_media_data_packet.h>
#include <nx/sdk/cloud_storage/i_integration.h>
#include <nx/sdk/i_device_info.h>

namespace nx::sdk::cloud_storage {

template<typename T>
class ValueOrError
{
public:
    static ValueOrError<T> makeValue(T t)
    {
        ValueOrError<T> result;
        result.v = std::move(t);
        return result;
    }

    static ValueOrError<T> makeError(std::string s)
    {
        ValueOrError<T> result;
        result.e = std::move(s);
        return result;
    }

    operator bool() const
    {
        return (bool) v;
    }

    T value() const
    {
        return *v;
    }

    std::string error() const
    {
        return *e;
    }

private:
    std::optional<T> v;
    std::optional<std::string> e;

    ValueOrError() = default;
};

/**
 * Every data structure here has two alternative ways of constructing from the string and json:
 * 1) Constructors which throw exception on invalid data.
 * 2) static `fromJson` functions which never throw and return error if parsing fails.
 */

enum class SortOrder
{
    ascending,
    descending,
};

std::string sortOrderToString(SortOrder order);
SortOrder sortOrderFromString(const std::string& s);

struct IntegrationManifest
{
    /**
     * Plugin identifier. This is what will be displayed int the storage type field in the Storage
     * dialog.
     */
    std::string id;

    /** Full plugin name, e.g. "Stub Cloud Storage plugin". */
    std::string name;

    /** Brief plugin description. */
    std::string description;

    std::string version;
    std::string vendor;

public:
    IntegrationManifest(const char* jsonStr) noexcept(false);
    IntegrationManifest(const nx::kit::Json& json) noexcept(false);
    IntegrationManifest(
        const std::string& id,
        const std::string& name,
        const std::string& description,
        const std::string& version,
        const std::string& vendor);

    static ValueOrError<IntegrationManifest> fromJson(const char* jsonStr) noexcept;
    static ValueOrError<IntegrationManifest> fromJson(const nx::kit::Json& json) noexcept;

    IntegrationManifest() = default;

    template<typename T>
    IntegrationManifest(const T&) = delete;

    bool operator==(const IntegrationManifest&) const;

    nx::kit::Json to_json() const;
};

struct TimePeriod
{
    std::chrono::milliseconds startTimestamp{-1};
    std::chrono::milliseconds duration{-1};

public:
    TimePeriod(const char* jsonStr) noexcept(false);
    TimePeriod(const nx::kit::Json& json) noexcept(false);
    TimePeriod(std::chrono::milliseconds startTimestamp, std::chrono::milliseconds duration);
    TimePeriod() = default;

    static ValueOrError<TimePeriod> fromJson(const char* jsonStr) noexcept;
    static ValueOrError<TimePeriod> fromJson(const nx::kit::Json& json) noexcept;

    template<typename T>
    TimePeriod(const T&) = delete;

    bool isInfinite() const;
    bool contains(std::chrono::milliseconds timestamp) const;
    bool isNull() const;
    bool intersects(const TimePeriod& other) const;
    std::optional<std::chrono::milliseconds> endTimestamp() const;

    nx::kit::Json to_json() const;
    bool operator<(const TimePeriod& other) const;
    bool operator==(const TimePeriod& other) const;
    bool operator!=(const TimePeriod& other) const;
};

class TimePeriodList
{
public:
    TimePeriodList(const char* jsonStr) noexcept(false);
    TimePeriodList(const nx::kit::Json& json) noexcept(false);
    TimePeriodList() = default;

    static ValueOrError<TimePeriodList> fromJson(const char* jsonStr) noexcept;
    static ValueOrError<TimePeriodList> fromJson(const nx::kit::Json& json) noexcept;

    TimePeriodList(SortOrder order);

    template<typename T>
    TimePeriodList(const T&) = delete;

    void forEach(std::function<void(const TimePeriod&)> func) const;
    nx::kit::Json to_json() const;
    void reverse();
    void shrink(size_t size);
    void append(const TimePeriod& period);
    void setLastDuration(std::chrono::milliseconds duration);

    SortOrder sortOrder() const;
    size_t size() const;
    bool empty() const;
    std::optional<TimePeriod> last() const;

private:
    SortOrder m_order = SortOrder::ascending;
    std::vector<int64_t> m_periods;
    std::optional<std::chrono::milliseconds> m_lastStartTimestmapMs;

private:
    void recalculateLastTimestamp();
};

struct KeyValuePair
{
    std::string name;
    std::string value;

public:
    KeyValuePair(const char* jsonStr) noexcept(false);
    KeyValuePair(const nx::kit::Json& json) noexcept(false);
    KeyValuePair() = default;
    KeyValuePair(const std::string& name, const std::string& value);

    static ValueOrError<KeyValuePair> fromJson(const char* jsonStr) noexcept;
    static ValueOrError<KeyValuePair> fromJson(const nx::kit::Json& json) noexcept;

    template<typename T>
    KeyValuePair(const T&) = delete;

    bool operator==(const KeyValuePair&) const;

    nx::kit::Json to_json() const;
};

nx::kit::Json keyValuePairsToJson(const std::vector<KeyValuePair>& keyValuePairList);

using DeviceParameter = KeyValuePair;

/**
 * Device description object. Consists of key-value pairs of the device attributes.
 */
struct DeviceDescription
{
    std::vector<DeviceParameter> parameters;

public:
    DeviceDescription() = default;
    DeviceDescription(const char* jsonData) noexcept(false);
    DeviceDescription(const nx::kit::Json& json) noexcept(false);
    DeviceDescription(const nx::sdk::IDeviceInfo* info);

    static ValueOrError<DeviceDescription> fromJson(const char* jsonStr) noexcept;
    static ValueOrError<DeviceDescription> fromJson(const nx::kit::Json& json) noexcept;

    template<typename T>
    DeviceDescription(const T&) = delete;

    bool operator==(const DeviceDescription&) const;

    const char* getParamValue(const std::string& key) const;
    nx::kit::Json to_json() const;
    std::optional<std::string> deviceId() const;
};

std::string deviceId(nx::sdk::cloud_storage::IDeviceAgent* deviceAgent);

int toStreamIndex(nxcip::MediaStreamQuality quality);

std::string toString(sdk::cloud_storage::MetadataType metadataType);

/**
 * Bookmark object passed to the plugin when a bookmark is saved. And the same object should be
 * returned to the Server when queried back if it matches the filter. The plugin should keep
 * bookmarks as is, without any changes.
 */
struct Bookmark
{
    /** Bookmark unique id. */
    std::string id;

    /** User that created this bookmark id. */
    std::string creatorId;

    std::chrono::milliseconds creationTimestamp{};
    std::string name;
    std::string description;
    std::chrono::milliseconds timeout = std::chrono::milliseconds(-1);
    std::chrono::milliseconds startTimestamp{};
    std::chrono::milliseconds duration{};
    std::vector<std::string> tags;
    std::string deviceId;

public:
    Bookmark(const char* jsonData) noexcept(false);
    Bookmark(const nx::kit::Json& json) noexcept(false);
    Bookmark() = default;

    static ValueOrError<Bookmark> fromJson(const char* jsonStr) noexcept;
    static ValueOrError<Bookmark> fromJson(const nx::kit::Json& json) noexcept;

    template<typename T>
    Bookmark(const T&) = delete;

    bool operator==(const Bookmark&) const;

    nx::kit::Json to_json() const;
};

using BookmarkList = std::vector<Bookmark>;

BookmarkList bookmarkListFromJson(const char* data);

/**
 * This filter will be passed to the plugin when bookmarks are queried by the Server. For
 * processing example, refer to nx/sdk/cloud_storage/algorithm.cpp.
 */
struct BookmarkFilter
{
    enum class SortColumn
    {
        name,
        startTime,
        duration,
        creationTime,
        tags,
        description,
    };

    std::optional<std::string> id;
    std::optional<std::chrono::milliseconds> startTimestamp;
    std::optional<std::chrono::milliseconds> endTimestamp;

    /** Arbitrary text to search for within bookmark name or tag. */
    std::optional<std::string> text;

    std::optional<int> limit;
    SortOrder order = SortOrder::ascending;
    SortColumn column = SortColumn::startTime;

    /** Minimum bookmark duration time. */
    std::optional<std::chrono::milliseconds> minVisibleLength;

    std::vector<std::string> deviceIds;
    std::optional<std::chrono::milliseconds> creationStartTimestamp;
    std::optional<std::chrono::milliseconds> creationEndTimestamp;

public:
    BookmarkFilter(const char* urlParams) noexcept(false);
    BookmarkFilter(const nx::kit::Json& json) noexcept(false);
    BookmarkFilter() = default;

    static ValueOrError<BookmarkFilter> fromUrlParams(const char* urlParams) noexcept;
    static ValueOrError<BookmarkFilter> fromJson(const nx::kit::Json& json) noexcept;

    template<typename T>
    BookmarkFilter(const T&) = delete;

    bool operator==(const BookmarkFilter&) const;

    nx::kit::Json to_json() const;
    std::string toUrlParams() const;

    static std::string sortColumnToString(SortColumn column);
    static SortColumn sortColumnFromString(const std::string& s);
};

/**
 * The Motion object passed to the plugin when bookmark is saved. And the same object should be
 * returned to the Server when queried back if it matches the filter. Plugin should keep motion as
 * is, without any changes.
 */
struct Motion
{
    int channel = 0;
    std::chrono::milliseconds startTimestamp;
    std::chrono::milliseconds duration{};
    std::string deviceId;

    /**
     * Binary motion mask data. Encoded in base64.
     *
     * This mask covers the frame as a 44x32 cells grid. Every non zero bit in the mask means that
     * motion was detected in that cell. So, the bit mask size is 44 * 32 = 1408 bits = 176 bytes
     * before encoding to base64. The mask is rotated by 90 degree. The very first bit of the mask
     * is the top-left corner bit. The next bit is for 1st column, 2nd row, etc.
     */
    std::string dataBase64;

public:
    Motion() = default;
    Motion(const char* jsonData) noexcept(false);
    Motion(const nx::kit::Json& json) noexcept(false);

    static ValueOrError<Motion> fromJson(const char* jsonStr) noexcept;
    static ValueOrError<Motion> fromJson(const nx::kit::Json& json) noexcept;

    template<typename T>
    Motion(const T&) = delete;

    bool operator==(const Motion&) const;

    nx::kit::Json to_json() const;
};

struct Rect
{
    double x = 0;
    double y = 0;
    double w = 0;
    double h = 0;

public:
    Rect(const char* jsonData) noexcept(false);
    Rect(const nx::kit::Json& json) noexcept(false);
    Rect(double x, double y, double w, double h);
    Rect() = default;

    static ValueOrError<Rect> fromJson(const char* jsonStr) noexcept;
    static ValueOrError<Rect> fromJson(const nx::kit::Json& json) noexcept;

    template<typename T>
    Rect(const T&) = delete;

    bool operator==(const Rect&) const;

    nx::kit::Json to_json() const;

    bool isEmpty() const;
    bool intersectsWith(const Rect& other) const;
    double left() const;
    double top() const;
    double right() const;
    double bottom() const;

private:
    bool isInside(double x, double y) const;
};

/**
 * This filter will be passed to the Plugin when motion is queried by the Server. For processing
 * example, refer to nx/sdk/cloud_storage/algorithm.cpp.
 */
struct MotionFilter
{
    std::vector<std::string> deviceIds;
    TimePeriod timePeriod;

    /**
     * Regions of screen which should be matched against the motion data.
     *
     * Motion matches if motion.data intersected with any of rectangles in regions contains at
     * least one cell of motion i.e. the corresponding bit in the motion.data is set to 1.
     */
    std::vector<Rect> regions;

    /** Maximum number of objects to return. */
    std::optional<int> limit;

    /** Found periods are sorted by the start timestamp using this order. */
    SortOrder order = SortOrder::ascending;

    /**
     * If distance between two time periods less than this value, then those periods must be merged
     * ignoring the gap.
     */
    std::chrono::milliseconds detailLevel;

public:
    MotionFilter() = default;
    MotionFilter(const char* jsonData) noexcept(false);
    MotionFilter(const nx::kit::Json& json) noexcept(false);

    static ValueOrError<MotionFilter> fromJson(const char* jsonStr) noexcept;
    static ValueOrError<MotionFilter> fromJson(const nx::kit::Json& json) noexcept;

    template<typename T>
    MotionFilter(const T&) = delete;

    bool operator==(const MotionFilter&) const;

    nx::kit::Json to_json() const;
};

using Attribute = KeyValuePair;
using Attributes = std::vector<KeyValuePair>;

struct ObjectRegion
{
    /** Binary mask of the bounding box grid. It has the same binary structure as motion data. */
    std::string boundingBoxGridBase64;

public:
    ObjectRegion() = default;
    ObjectRegion(const nx::kit::Json & json) noexcept(false);
    ObjectRegion(const char* jsonData) noexcept(false);

    static ValueOrError<ObjectRegion> fromJson(const char* jsonStr) noexcept;
    static ValueOrError<ObjectRegion> fromJson(const nx::kit::Json& json) noexcept;

    template<typename T>
    ObjectRegion(const T&) = delete;

    bool operator==(const ObjectRegion&) const;

    nx::kit::Json to_json() const;
};

std::string trackImageTypeToString(TrackImageType type);
TrackImageType trackImageTypeFromString(const std::string& s);

/**
 * Image data and Track id the image is associated with.
 */
struct Image
{
    std::string objectTrackId;

    /** Human-readable image format name. */
    std::string format;

    /** Base64 encoded image data. */
    std::string data64;

public:
    Image() = default;
    Image(const nx::kit::Json& json) noexcept(false);
    Image(const char* jsonData) noexcept(false);

    static ValueOrError<Image> fromJson(const char* jsonStr) noexcept;
    static ValueOrError<Image> fromJson(const nx::kit::Json& json) noexcept;

    template<typename T>
    Image(const T&) = delete;

    nx::kit::Json to_json() const;

    bool operator==(const Image&) const;
};

/**
 * The image/thumbnail + related metadata.
 */
struct BaseImage
{
    std::chrono::microseconds timestamp{};
    Rect rect;
    int streamIndex = -1;
    std::optional<Image> image;

public:
    BaseImage() = default;
    BaseImage(const nx::kit::Json& json) noexcept(false);
    BaseImage(const char* jsonData) noexcept(false);

    bool operator==(const BaseImage&) const;

    template<typename T>
    BaseImage(const T&) = delete;

    nx::kit::Json to_json() const;
    std::map<std::string, nx::kit::detail::json11::Json> toBaseObject() const;

    bool isNull() const;
};

struct BestShot: public BaseImage
{
    BestShot() = default;
    BestShot(const nx::kit::Json& json) noexcept(false);
    BestShot(const BaseImage& data) : BaseImage(data) {}
    BestShot(const char* jsonStr) noexcept(false);

    bool operator==(const BestShot&) const;

    static ValueOrError<BestShot> fromJson(const char* jsonStr) noexcept;
    static ValueOrError<BestShot> fromJson(const nx::kit::Json& json) noexcept;

    nx::kit::Json to_json() const;
};

struct Title: public BaseImage
{
    /** Optional text associated with the title. */
    std::string text;

public:
    Title() = default;
    Title(const nx::kit::Json& json) noexcept(false);
    Title(const BaseImage& data): BaseImage(data) {}
    Title(const char* jsonStr) noexcept(false);

    bool operator==(const Title&) const;

    static ValueOrError<Title> fromJson(const char* jsonStr) noexcept;
    static ValueOrError<Title> fromJson(const nx::kit::Json& json) noexcept;

    nx::kit::Json to_json() const;
};

using TrackImage = BaseImage;

struct ObjectTrack
{
    std::string id;

    /** Device that this object track was collected from id. */
    std::string deviceId;

    /**
     * Object category (vehicle, person, e.t.c.). The string of the type must be compared starting
     * from the beginning. For example, object track may have a 'human.head' type. This Object
     * Track should be found if filter.text contains "human" Object Type.
     */
    std::string objectTypeId;

    /**
     * A list of the Object Track Attributes. Each Attribute in the list consists of a name and a
     * value. For example `color=red`,`hasBag=true`, etc. An Attribute value may also contain a
     * numeric range, for example: `speed=[10..50]`.
     */
    Attributes attributes;

    /** Object Track start time. */
    std::chrono::microseconds firstAppearanceTimestamp{};

    /** Object Track end time. */
    std::chrono::microseconds lastAppearanceTimestamp{};

    /** Object Track coordinates. See ObjectRegion description for more details. */
    ObjectRegion objectPosition;

    /** An analytics plugin id that provided data. */
    std::string analyticsEngineId;

    std::optional<BestShot> bestShot;
    std::optional<Title> title;

public:
    ObjectTrack() = default;
    ObjectTrack(const nx::kit::Json & json) noexcept(false);
    ObjectTrack(const char* jsonData) noexcept(false);

    static ValueOrError<ObjectTrack> fromJson(const char* jsonStr) noexcept;
    static ValueOrError<ObjectTrack> fromJson(const nx::kit::Json& json) noexcept;

    template<typename T>
    ObjectTrack(const T&) = delete;

    bool operator==(const ObjectTrack&) const;

    nx::kit::Json to_json() const;
};

using AnalyticsLookupResult = std::vector<ObjectTrack>;

AnalyticsLookupResult analyticsLookupResultFromJson(const char* data);

/**
 * Helper struct representing a range border point.
 */
struct RangePoint
{
    RangePoint() = default;
    RangePoint(const nx::kit::Json& json) noexcept(false);
    RangePoint(const char* jsonData) noexcept(false);

    static ValueOrError<RangePoint> fromJson(const char* jsonStr) noexcept;
    static ValueOrError<RangePoint> fromJson(const nx::kit::Json& json) noexcept;

    template<typename T>
    RangePoint(const T&) = delete;

    RangePoint(float value, bool inclusive);

    bool operator==(const RangePoint&) const;

    nx::kit::Json to_json() const;

    float value = 0.0;
    bool inclusive = false;
};

/**
 * Numeric range representation. Used as one of the possible search conditions while querying
 * Object Tracks filtered by Attributes.
 */
struct NumericRange
{
    std::optional<RangePoint> from;
    std::optional<RangePoint> to;

public:
    NumericRange() = default;
    NumericRange(const nx::kit::Json& json) noexcept(false);
    NumericRange(const char* jsonData) noexcept(false);

    static ValueOrError<NumericRange> fromJson(const char* jsonStr) noexcept;
    static ValueOrError<NumericRange> fromJson(const nx::kit::Json& json) noexcept;

    template<typename T>
    NumericRange(const T&) = delete;

    bool operator==(const NumericRange&) const;

    NumericRange(float value): from(RangePoint{value, true}), to(RangePoint{value, true}) {}

    NumericRange(std::optional<RangePoint> from, std::optional<RangePoint> to):
        from(std::move(from)), to(std::move(to))
    {
    }

    static std::optional<NumericRange> fromString(const std::string& s);

    nx::kit::Json to_json() const;

    bool intersects(const NumericRange& range) const;
    bool hasRange(const NumericRange& range) const;
};

/**
 * A search condition representation specifying how exactly to filter queried object tracks by
 * their attributes.
 *
 * Multiple condition match results are joined using AND logic. So for object track to be included
 * in the result its attributes (any of them) should match every condition present in the Filter.
 * I.e., bool objectTrackMatch = atLeastOneAttrMatch(condition_1) && ... && atLeastOneAttrMatch(condition_N)
 */
struct AttributeSearchCondition
{
    /**
     * All search types except 'textMatch' assume that ObjectTrack attribute name matches the
     * condition name only if the match starts from the beginning and lasts till the end or the
     * point separator.
     *
     * For example, let one of the ObjectTrack Attribute equals to `car.color=blue`. Then the
     * Search conditions with the following names should match: `car`, `car.color`. But not the
     * `ca`, `car.co`, `carxx` or `car.colov`.
     * In case of the `textMatch` for the Attribute to match search condition it's enough that
     * either Attribute name or value contain AttributeSearchCondition::text. Condition name and
     * value should be ignored in this case.
     *
     * `numericRangeMatch` assumes that the Attribute value contains a valid NumericRange, and it
     * it intersects with AttributeSearchCondition::range.
     */
    enum class Type
    {
        attributePresenceCheck,
        attributeValueMatch,
        textMatch,
        numericRangeMatch,
    };

    Type type;
    std::string name;
    std::string value;
    std::string text;

    /**
     * If it is set to true than the ObjectTrack should be included in the result only if it does
     * not match the given condition.
     */
    bool isNegative = false;

    /** If it is set to true, then the value should be matched from the beginning. */
    bool matchesFromStart = false;

    /** If it is set to true, then the value should be matched till the end. */
    bool matchesTillEnd = false;

    /**
     * Used when type == numericRangeMatch. Object track attribute value should contain numeric
     * range value and it should intersect the given condition value for the Object track to be
     * included in the result.
     */
    NumericRange range;

public:
    AttributeSearchCondition() = default;
    AttributeSearchCondition(const nx::kit::Json& json) noexcept(false);
    AttributeSearchCondition(const char* jsonData) noexcept(false);

    static ValueOrError<AttributeSearchCondition> fromJson(const char* jsonStr) noexcept;
    static ValueOrError<AttributeSearchCondition> fromJson(const nx::kit::Json& json) noexcept;

    template<typename T>
    AttributeSearchCondition(const T&) = delete;

    bool operator==(const AttributeSearchCondition&) const;

    nx::kit::Json to_json() const;

    static std::string typeToString(Type type);
    static Type typeFromString(const std::string& s);
};

/**
 * Filter used to search for Object Tracks.
 */
struct AnalyticsFilter
{
    enum Option
    {
        none = 0x0,
        ignoreTextFilter = 0x1,
        ignoreBoundingBox = 0x2,
        ignoreTimePeriod = 0x4,
    };

    /** If empty, any device is a match. */
    std::vector<std::string> deviceIds;

    /**
     * See ObjectTrack.objectTypeId for the explanation. If empty, any objectTrack.objectTypeId
     * is a match.
     */
    std::vector<std::string> objectTypeIds;

    /** If empty, any objectTrackId is a match. */
    std::optional<std::string> objectTrackId;

    /**
     * If not null, only Object Tracks with firstAppearanceTimestamp within this period should
     * match.
     */
    TimePeriod timePeriod;

    /** Bounding box area to search within. Search should be done similarly to motion data. */
    std::optional<Rect> boundingBox;

    std::optional<int> maxObjectTracksToSelect;
    std::vector<AttributeSearchCondition> attributeSearchConditions;

    /** Found tracks are sorted by the minimum track time using this order. */
    SortOrder order = SortOrder::descending;

    std::optional<std::string> analyticsEngineId;
    int options = Option::none;
    std::chrono::milliseconds detailLevel{};

public:
    AnalyticsFilter() = default;
    AnalyticsFilter(const char* jsonData) noexcept(false);
    AnalyticsFilter(const nx::kit::Json& json) noexcept(false);

    static ValueOrError<AnalyticsFilter> fromJson(const char* jsonStr) noexcept;
    static ValueOrError<AnalyticsFilter> fromJson(const nx::kit::Json& json) noexcept;

    template<typename T>
    AnalyticsFilter(const T&) = delete;

    bool operator==(const AnalyticsFilter&) const;

    nx::kit::Json to_json() const;

    static std::string optionsToString(int options);
    static int optionsFromString(const std::string& s);
};

struct CodecInfoData
{
    nxcip::CompressionType compressionType = nxcip::CompressionType::AV_CODEC_ID_NONE;
    nxcip::PixelFormat pixelFormat = nxcip::PixelFormat::AV_PIX_FMT_YUV420P;
    nxcip::MediaType mediaType = nxcip::MediaType::AVMEDIA_TYPE_UNKNOWN;
    int width = -1;
    int height = -1;
    int64_t codecTag = -1;
    int64_t bitRate = -1;
    int channels = -1;
    int frameSize = -1;
    int blockAlign = -1;
    int sampleRate = -1;
    nxcip::SampleFormat sampleFormat = nxcip::SampleFormat::AV_SAMPLE_FMT_NONE;
    int bitsPerCodedSample = -1;
    int64_t channelLayout = -1;
    std::string extradataBase64;
    int channelNumber = -1;

public:
    CodecInfoData();
    CodecInfoData(const char* jsonData) noexcept(false);
    CodecInfoData(const nx::kit::Json& json) noexcept(false);
    CodecInfoData(const nx::sdk::cloud_storage::ICodecInfo* codecInfo);

    static ValueOrError<CodecInfoData> fromJson(const char* jsonStr) noexcept;
    static ValueOrError<CodecInfoData> fromJson(const nx::kit::Json& json) noexcept;

    template<typename T>
    CodecInfoData(const T&) = delete;

    bool operator==(const CodecInfoData&) const;

    nx::kit::Json to_json() const;
};

IDeviceInfo* deviceInfo(const DeviceDescription& deviceDescription);

class MediaPacket;

struct MediaPacketData
{
    std::vector<uint8_t> data;
    int dataSize = 0;
    nxcip::CompressionType compressionType = nxcip::CompressionType::AV_CODEC_ID_NONE;
    nxcip::UsecUTCTimestamp timestampUs = 0;
    IMediaDataPacket::Type type = IMediaDataPacket::Type::unknown;
    int channelNumber = -1;
    bool isKeyFrame = false;
    std::vector<uint8_t> encryptionData;

public:
    MediaPacketData(const IMediaDataPacket* mediaPacket, std::chrono::milliseconds startTime);
    MediaPacketData() = default;
};

struct CloudDeviceReportEntry
{
    std::string id;
    std::string serviceId;

public:
    CloudDeviceReportEntry() = default;
    CloudDeviceReportEntry(const char* jsonData) noexcept(false);
    CloudDeviceReportEntry(const nx::kit::Json& json) noexcept(false);

    static ValueOrError<CloudDeviceReportEntry> fromJson(const char* jsonStr) noexcept;
    static ValueOrError<CloudDeviceReportEntry> fromJson(const nx::kit::Json& json) noexcept;

    bool operator==(const CloudDeviceReportEntry& other) const;
    nx::kit::Json to_json() const;
};

using CloudDeviceEntryList = std::vector<CloudDeviceReportEntry>;

struct CloudDeviceReport
{
    std::string cloudSystemId;
    CloudDeviceEntryList devices;

public:
    CloudDeviceReport() = default;
    CloudDeviceReport(const char* jsonData) noexcept(false);
    CloudDeviceReport(const nx::kit::Json& json) noexcept(false);

    static ValueOrError<CloudDeviceReport> fromJson(const char* jsonStr) noexcept;
    static ValueOrError<CloudDeviceReport> fromJson(const nx::kit::Json& json) noexcept;

    bool operator==(const CloudDeviceReport& other) const;
    nx::kit::Json to_json() const;
};

} // nx::sdk::namespace cloud_storage
