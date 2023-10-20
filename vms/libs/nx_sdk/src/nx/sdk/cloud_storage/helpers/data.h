// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include <camera/camera_plugin.h>
#include <nx/kit/json.h>
#include <nx/sdk/cloud_storage/i_media_data_packet.h>
#include <nx/sdk/cloud_storage/i_plugin.h>
#include <nx/sdk/i_device_info.h>

namespace nx::sdk::cloud_storage {

struct PluginManifest
{
    PluginManifest(const char* jsonStr);
    PluginManifest(const nx::kit::Json& json);
    PluginManifest(
        const std::string& id,
        const std::string& name,
        const std::string& description,
        const std::string& version,
        const std::string& vendor);

    PluginManifest() = default;

    template<typename T>
    PluginManifest(const T&) = delete;

    bool operator==(const PluginManifest&) const;

    // Plugin identificator. This is what will be displayed int the storage type field in
    // the storage dialog.
    std::string id;
    // Full plugin name ('Stub Cloud Storage plugin', for example)
    std::string name;
    // Brief plugin description.
    std::string description;
    std::string version;
    std::string vendor;

    nx::kit::Json to_json() const;
};

struct TimePeriod
{
    TimePeriod(const char* jsonStr);
    TimePeriod(const nx::kit::Json& json);
    TimePeriod(std::chrono::milliseconds startTimestamp, std::chrono::milliseconds duration);
    TimePeriod() = default;

    template<typename T>
    TimePeriod(const T&) = delete;

    std::chrono::milliseconds startTimestamp{-1};
    std::chrono::milliseconds duration{-1};

    bool isInfinite() const;
    bool contains(std::chrono::milliseconds timestamp) const;
    bool isNull() const;
    bool intersects(const TimePeriod& other) const;
    std::optional<std::chrono::milliseconds> endTimestamp() const;

    nx::kit::Json to_json() const;
    bool operator<(const TimePeriod& other) const;
    bool operator==(const TimePeriod& other) const;
};

using TimePeriodList = std::vector<TimePeriod>;
nx::kit::Json timePeriodListToJson(const TimePeriodList& timePeriods);
TimePeriodList timePeriodListFromJson(const char* data);

struct KeyValuePair
{
    KeyValuePair(const char* jsonStr);
    KeyValuePair(const nx::kit::Json& json);
    KeyValuePair() = default;
    KeyValuePair(const std::string& name, const std::string& value);

    template<typename T>
    KeyValuePair(const T&) = delete;

    bool operator==(const KeyValuePair&) const;

    nx::kit::Json to_json() const;

    std::string name;
    std::string value;
};

nx::kit::Json keyValuePairsToJson(const std::vector<KeyValuePair>& keyValuePairList);

using DeviceParameter = KeyValuePair;

// Device description object. Consists of key-value pairs of the device attributes.
struct DeviceDescription
{
    DeviceDescription() = default;
    DeviceDescription(const char* jsonData);
    DeviceDescription(const nx::kit::Json& json);
    DeviceDescription(const nx::sdk::IDeviceInfo* info);

    template<typename T>
    DeviceDescription(const T&) = delete;

    bool operator==(const DeviceDescription&) const;

    std::vector<DeviceParameter> parameters;

    const char* getParamValue(const std::string& key) const;
    nx::kit::Json to_json() const;
    std::optional<std::string> deviceId() const;
};

std::string deviceId(nx::sdk::cloud_storage::IDeviceAgent* deviceAgent);
int toStreamIndex(nxcip::MediaStreamQuality quality);
std::string toString(sdk::cloud_storage::MetadataType metadataType);

// Bookmark object passed to the plugin when bookmark is saved. And the same object
// should be returned to the Server when queried back if it matches against the filter.
// Plugin should keep bookmarks as is, without any changes.
struct Bookmark
{
    Bookmark(const char* jsonData);
    Bookmark(const nx::kit::Json& json);
    Bookmark() = default;

    template<typename T>
    Bookmark(const T&) = delete;

    bool operator==(const Bookmark&) const;

    nx::kit::Json to_json() const;

    // Bookmark unique id.
    std::string id;
    // User that created this bookmark id.
    std::string creatorId;
    std::chrono::milliseconds creationTimestamp{};
    std::string name;
    std::string description;
    std::chrono::milliseconds timeout = std::chrono::milliseconds(-1);
    std::chrono::milliseconds startTimestamp;
    std::chrono::milliseconds duration{};
    std::vector<std::string> tags;
    std::string deviceId;
};

using BookmarkList = std::vector<Bookmark>;

BookmarkList bookmarkListFromJson(const char* data);

enum class SortOrder
{
    ascending,
    descending,
};

std::string sortOrderToString(SortOrder order);
SortOrder sortOrderFromString(const std::string& s);

// This filter will be passed to the plugin when bookmarks are queried by Server.
// For processing example refer to the 'nx/sdk/cloud_storage/algorithm.cpp'
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

    BookmarkFilter(const char* jsonData);
    BookmarkFilter(const nx::kit::Json& json);
    BookmarkFilter() = default;

    template<typename T>
    BookmarkFilter(const T&) = delete;

    bool operator==(const BookmarkFilter&) const;

    nx::kit::Json to_json() const;

    static std::string sortColumnToString(SortColumn column);
    static SortColumn sortColumnFromString(const std::string& s);

    std::optional<std::string> id;
    std::optional<std::chrono::milliseconds> startTimestamp;
    std::optional<std::chrono::milliseconds> endTimestamp;
    // Arbitrary text to search for within bookmark name or tag.
    std::optional<std::string> text;
    std::optional<int> limit;
    SortOrder order = SortOrder::ascending;
    SortColumn column = SortColumn::startTime;
    // Minimum bookmark duration time.
    std::optional<std::chrono::milliseconds> minVisibleLength;
    std::vector<std::string> deviceIds;
    std::optional<std::chrono::milliseconds> creationStartTimestamp;
    std::optional<std::chrono::milliseconds> creationEndTimestamp;
};

// Motion object passed to the plugin when bookmark is saved. And the same object
// should be returned to the Server when queried back if it matches against the filter.
// Plugin should keep motion as is, without any changes.
struct Motion
{
    Motion() = default;
    Motion(const char* jsonData);
    Motion(const nx::kit::Json& json);

    template<typename T>
    Motion(const T&) = delete;

    bool operator==(const Motion&) const;

    int channel = 0;
    std::chrono::milliseconds startTimestamp;
    std::chrono::milliseconds duration{};
    std::string deviceId;
    // Binary motion mask data. Encoded in base64.
    // This mask covers the frame as a 44x32 cells grid. Every non zero bit in the mask means
    // that motion was detected in that cell. So, the bit mask size is 44*32=1408 bits = 176 bytes
    // before encoding to base64. The mask is rotated by 90 degree. The very first bit of the mask
    // is the top-left corner bit. The next bit is for 1-st column, 2-nd row e.t.c.
    std::string dataBase64;

    nx::kit::Json to_json() const;
};

struct Rect
{
    Rect(const char* jsonData);
    Rect(const nx::kit::Json& json);
    Rect(double x, double y, double w, double h);
    Rect() = default;

    template<typename T>
    Rect(const T&) = delete;

    bool operator==(const Rect&) const;

    nx::kit::Json to_json() const;

    double x = 0;
    double y = 0;
    double w = 0;
    double h = 0;

    bool isEmpty() const;
    bool intersectsWith(const Rect& other) const;
    double left() const;
    double top() const;
    double right() const;
    double bottom() const;

private:
    bool isInside(double x, double y) const;
};

// This filter will be passed to the plugin when motion is queried by Server.
// For processing example refer to the 'nx/sdk/cloud_storage/algorithm.cpp'
struct MotionFilter
{
    MotionFilter() = default;
    MotionFilter(const char* jsonData);
    MotionFilter(const nx::kit::Json& json);

    template<typename T>
    MotionFilter(const T&) = delete;

    bool operator==(const MotionFilter&) const;

    nx::kit::Json to_json() const;

    std::vector<std::string> deviceIds;
    TimePeriod timePeriod;

    // Regions of screen which should be matched against the motion data.
    // Motion matches if motion.data intersected with any of rectangles in regions contains at
    // least one cell of motion i.e. the corresponding bit in the motion.data is set to 1.
    std::vector<Rect> regions;
    // Maximum number of objects to return.
    std::optional<int> limit;

    /** Found periods are sorted by the start timestamp using this order. */
    SortOrder order = SortOrder::descending;

     // If distance between two time periods less than this value, then those periods must be merged
     // ignoring the gap.
     std::chrono::milliseconds detailLevel;
};

using Attribute = KeyValuePair;
using Attributes = std::vector<KeyValuePair>;

struct ObjectRegion
{
    ObjectRegion() = default;
    ObjectRegion(const nx::kit::Json & json);
    ObjectRegion(const char* jsonData);

    template<typename T>
    ObjectRegion(const T&) = delete;

    bool operator==(const ObjectRegion&) const;

    nx::kit::Json to_json() const;

    // Binary mask of the bounding box grid. It has the same binary structure as motion data.
    std::string boundingBoxGridBase64;
};

// The image/thumbnail that represent the object track metadata. This information describes the
// image from the media archive (a part of the video frame for track thumbnail). This information
// is used by Server in case if the best shot image for the track is not uploaded explicitly.
// BestShot is NULL if its rect is empty.
struct BestShot
{
    BestShot() = default;
    BestShot(const nx::kit::Json& json);
    BestShot(const char* jsonData);

    template<typename T>
    BestShot(const T&) = delete;

    bool operator==(const BestShot&) const;

    nx::kit::Json to_json() const;
    bool isNull() const;

    std::chrono::microseconds timestamp{};
    Rect rect;
    int streamIndex = -1;
};

struct ObjectTrack
{
    ObjectTrack() = default;
    ObjectTrack(const nx::kit::Json & json);
    ObjectTrack(const char* jsonData);

    template<typename T>
    ObjectTrack(const T&) = delete;

    bool operator==(const ObjectTrack&) const;

    nx::kit::Json to_json() const;

    std::string id;
    // Device that this object track was collected from id.
    std::string deviceId;
    // Object category (vehicle, person, e.t.c.). The string of the type must be compared
    // starting from the beginning. For example, object track may have a 'human.head' type.
    // This object track should be found if the filter.text contains 'human' object type.
    std::string objectTypeId;
    // A list of the object track attributes. Each attribute in the list consists of a name
    // and a value. For example 'color,red','hasBag,true' e.t.c. Attribute value may also contain a
    // numeric range. For example: 'speed, [10..50]'.
    Attributes attributes;
    // Object track start time.
    std::chrono::microseconds firstAppearanceTimestamp{};
    // Object track end time.
    std::chrono::microseconds lastAppearanceTimestamp{};
    // Object track coordinates. See ObjectRegion description for more details.
    ObjectRegion objectPosition;
    // An analytics plugin id that provided data.
    std::string analyticsEngineId;
    BestShot bestShot;
};

// BestShot image associated with the given ObjectTrack.
struct BestShotImage
{
    BestShotImage() = default;
    BestShotImage(const nx::kit::Json& json);
    BestShotImage(const char* jsonData);

    template<typename T>
    BestShotImage(const T&) = delete;

    bool operator==(const BestShotImage&) const;

    nx::kit::Json to_json() const;

    std::string objectTrackId;
    // Human-readable image format name.
    std::string format;
    // Base64 encoded image data.
    std::string data64;
};

using AnalyticsLookupResult = std::vector<ObjectTrack>;

AnalyticsLookupResult analyticsLookupResultFromJson(const char* data);

// Helper struct representing a range border point.
struct RangePoint
{
    RangePoint() = default;
    RangePoint(const nx::kit::Json& json);
    RangePoint(const char* jsonData);

    template<typename T>
    RangePoint(const T&) = delete;

    RangePoint(float value, bool inclusive);

    bool operator==(const RangePoint&) const;

    nx::kit::Json to_json() const;

    float value = 0.0;
    bool inclusive = false;
};

// Numeric range representation. Used as one of the possible search conditions while querying
// object tracks filtered by attributes.
struct NumericRange
{
    NumericRange() = default;
    NumericRange(const nx::kit::Json& json);
    NumericRange(const char* jsonData);

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

    std::optional<RangePoint> from;
    std::optional<RangePoint> to;
};

// A search condition representation specifying how exactly to filter queried object tracks
// by their attributes.
// Multiple condition match results are joined using AND logic. So for object track to be included
// in the result its attributes (any of them) should match every condition present in the Filter.
// I.e. bool objectTrackMatch = atLeastOneAttrMatch(condition_1) && ... && atLeastOneAttrMatch(condition_N)
struct AttributeSearchCondition
{
    AttributeSearchCondition() = default;
    AttributeSearchCondition(const nx::kit::Json& json);
    AttributeSearchCondition(const char* jsonData);

    template<typename T>
    AttributeSearchCondition(const T&) = delete;

    bool operator==(const AttributeSearchCondition&) const;

    nx::kit::Json to_json() const;

    // All search types except 'textMatch' assume that ObjectTrack attribute name matches
    // the condition name only if the match starts from the beginning and lasts till the end
    // or the point separator.
    // For example, let one of the ObjectTrack attribute equals to 'car.color=blue'. Then the
    // Search conditions with the following names should match: 'car', 'car.color'. But not the
    // 'ca', 'car.co', 'carxx' or 'car.colov'.
    // In case of the 'textMatch' for the attribute to match search condition it's enough that
    // either attribute name or value contain AttributeSearchCondition::text. Condition name and
    // value should be ignored in this case.
    // 'numericRangeMatch' assumes that the attribute value contains valid NumericRange and
    // it intersects AttributeSearchCondition::range.
    enum class Type
    {
        attributePresenceCheck,
        attributeValueMatch,
        textMatch,
        numericRangeMatch,
    };

    static std::string typeToString(Type type);
    static Type typeFromString(const std::string& s);

    Type type;
    std::string name;
    std::string value;
    std::string text;

    // If it is set to true than the ObjectTrack should be included in the result only if it does not
    // match the given condition.
    bool isNegative = false;

    // If it is set to true than value should be matched from begining.
    bool matchesFromStart = false;

    // If it is set to true than value should be matched till end.
    bool matchesTillEnd = false;

    // Used when type == numericRangeMatch. Object track attribute value should contain numeric range
    // value and it should intersect the given condition value for the Object track to be included in the result.
    NumericRange range;
};

// Filter used to search for object tracks.
struct AnalyticsFilter
{
    AnalyticsFilter() = default;
    AnalyticsFilter(const char* jsonData);
    AnalyticsFilter(const nx::kit::Json& json);

    template<typename T>
    AnalyticsFilter(const T&) = delete;

    bool operator==(const AnalyticsFilter&) const;

    nx::kit::Json to_json() const;

    enum Option
    {
        none = 0x0,
        ignoreTextFilter = 0x1,
        ignoreBoundingBox = 0x2,
        ignoreTimePeriod = 0x4,
    };

    static std::string optionsToString(int options);
    static int optionsFromString(const std::string& s);

    // If empty, any device is a match.
    std::vector<std::string> deviceIds;
    // See ObjectTrack.objectTypeId for explanation. If empty, any objectTrack.objectTypeId
    // is a match.
    std::vector<std::string> objectTypeIds;
    // If empty, any objectTrackId is a match.
    std::optional<std::string> objectTrackId;
    // If not null only object tracks with firstAppearanceTimestamp within this period should
    // match.
    TimePeriod timePeriod;
    // Bounding box area to search within. Search should be done similarly to motion data.
    std::optional<Rect> boundingBox;
    std::optional<int> maxObjectTracksToSelect;
    std::vector<AttributeSearchCondition> attributeSearchConditions;

    /** Found tracks are sorted by the minimum track time using this order. */
    SortOrder order = SortOrder::descending;

    std::optional<std::string> analyticsEngineId;
    int options = Option::none;
    std::chrono::milliseconds detailLevel{};
};

struct CodecInfoData
{
    CodecInfoData();
    CodecInfoData(const char* jsonData);
    CodecInfoData(const nx::kit::Json& json);
    CodecInfoData(const nx::sdk::cloud_storage::ICodecInfo* codecInfo);

    template<typename T>
    CodecInfoData(const T&) = delete;

    bool operator==(const CodecInfoData&) const;

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

    nx::kit::Json to_json() const;
};

IDeviceInfo* deviceInfo(const DeviceDescription& deviceDescription);

class MediaPacket;

struct MediaPacketData
{
    MediaPacketData(const IMediaDataPacket* mediaPacket, std::chrono::milliseconds startTime);
    MediaPacketData() = default;

    std::vector<uint8_t> data;
    int dataSize = 0;
    nxcip::CompressionType compressionType = nxcip::CompressionType::AV_CODEC_ID_NONE;
    nxcip::UsecUTCTimestamp timestampUs = 0;
    IMediaDataPacket::Type type = IMediaDataPacket::Type::unknown;
    int channelNumber = -1;
    bool isKeyFrame = false;
    std::vector<uint8_t> encryptionData;
};

struct CloudDeviceReportEntry
{
    CloudDeviceReportEntry() = default;
    CloudDeviceReportEntry(const char* jsonData);
    CloudDeviceReportEntry(const nx::kit::Json& json);

    std::string id;
    std::string serviceId;

    bool operator==(const CloudDeviceReportEntry& other) const;
    nx::kit::Json to_json() const;
};

using CloudDeviceEntryList = std::vector<CloudDeviceReportEntry>;

struct CloudDeviceReport
{
    CloudDeviceReport() = default;
    CloudDeviceReport(const char* jsonData);
    CloudDeviceReport(const nx::kit::Json& json);

    std::string cloudSystemId;
    CloudDeviceEntryList devices;

    bool operator==(const CloudDeviceReport& other) const;
    nx::kit::Json to_json() const;
};

} // nx::sdk::namespace cloud_storage
