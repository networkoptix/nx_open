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
    TimePeriod(const int64_t startTimeMs, const int64_t durationMs);
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

} // namespace nx::sdk::cloud_storage
