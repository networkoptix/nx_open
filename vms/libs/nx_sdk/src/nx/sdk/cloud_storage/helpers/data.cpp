// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "data.h"

#include <algorithm>
#include <sstream>

#include <nx/sdk/cloud_storage/helpers/algorithm.h>
#include <nx/sdk/helpers/device_info.h>

#include "algorithm.h"

namespace nx::sdk::cloud_storage {

using namespace std::chrono;

namespace {

template<typename Data>
Data getObjectValue(const nx::kit::Json& json, const std::string& key)
{
    const auto value = json[key];
    if (!value.is_object())
        throw std::logic_error("Value '" + key + "' is not an object");

    return Data(value);
}

template<typename Data>
std::optional<Data> getOptionalObjectValue(
    const nx::kit::Json& json, const std::string& key)
{
    const auto value = json[key];
    if (!value.is_object())
        return std::nullopt;

    return std::optional<Data>(Data(value));
}

std::string getStringValue(const nx::kit::Json& json, const std::string& key)
{
    const auto value = json[key];
    if (!value.is_string())
        throw std::logic_error("Value '" + key + "' is not a string");

    return value.string_value();
}

std::optional<std::string> getOptionalStringValue(
    const nx::kit::Json& json, const std::string& key)
{
    const auto value = json[key];
    if (!value.is_string())
        return std::nullopt;

    return value.string_value();
}

int64_t getIntValue(const nx::kit::Json& json, const std::string& key)
{
    const auto value = json[key];
    if (!value.is_number())
        throw std::logic_error("Value '" + key + "' is not a number");

    return (int64_t) value.number_value();
}

double getDoubleValue(const nx::kit::Json& json, const std::string& key)
{
    const auto value = json[key];
    if (!value.is_number())
        throw std::logic_error("Value '" + key + "' is not a number");

    return value.number_value();
}

bool getBoolValue(const nx::kit::Json& json, const std::string& key)
{
    const auto value = json[key];
    if (!value.is_bool())
        throw std::logic_error("Value '" + key + "' is not a boolean");

    return value.bool_value();
}

std::optional<int64_t> getOptionalIntValue(const nx::kit::Json& json, const std::string& key)
{
    const auto value = json[key];
    if (!value.is_number())
        return std::nullopt;

    return (int64_t) value.number_value();
}

template<typename Duration>
Duration getDurationValue(const nx::kit::Json& json, const std::string& key)
{
    const auto value = json[key];
    if (!value.is_number())
        throw std::logic_error("Value '" + key + "' is not a number");

    return Duration((int64_t) value.number_value());
}

template<typename Duration>
std::optional<Duration> getOptionalDurationValue(
    const nx::kit::Json& json, const std::string& key)
{
    const auto value = json[key];
    if (!value.is_number())
        return std::nullopt;

    return Duration((int64_t) value.number_value());
}

template<typename T>
std::vector<T> getBasicListValue(const nx::kit::Json& value)
{
    std::vector<T> result;
    for (const auto& entry : value.array_items())
    {
        if constexpr (std::is_same_v<std::string, T>)
        {
            if (!entry.is_string())
                throw std::logic_error("Value is not an array of strings");
            result.push_back(entry.string_value());
        }
        else if constexpr (std::is_same_v<int64_t, T>)
        {
            if (!entry.is_number())
                throw std::logic_error("Value  is not an array of numbers");
            result.push_back(entry.number_value());
        }
        else
        {
            throw std::logic_error("Unsupported array value");
        }
    }

    return result;
}

// string or int64_t are supported so far
template<typename T>
std::vector<T> getBasicListValue(const nx::kit::Json& json, const std::string& key)
{
    const nx::kit::Json value = json[key];
    if (!value.is_array())
        throw std::logic_error("Value '" + key + "' is not an array");
    return getBasicListValue<T>(value);
}

template<typename Data>
std::vector<Data> objectListFromJson(nx::kit::Json json)
{
    if (!json.is_array())
        throw std::logic_error("Object list is not an array");

    std::vector<Data> result;
    for (const auto& oJson : json.array_items())
    {
        if (!oJson.is_object())
            throw std::logic_error("Object is not a json object");

        result.push_back(Data(oJson));
    }

    return result;
}

template<typename Data>
std::vector<Data> getObjectListValue(const nx::kit::Json& json, const std::string& key)
{
    const auto value = json[key];
    return objectListFromJson<Data>(value);
}

nx::kit::Json parseJson(const std::string& jsonStr)
{
    std::string parseError;
    const auto json = nx::kit::Json::parse(jsonStr, parseError);
    if (!parseError.empty())
    {
        throw std::logic_error(
            "Failed to parse json. Error: " + parseError + ". Original string: " + jsonStr);
    }

    return json;
}

template<typename Data>
std::vector<Data> objectListFromJson(const char* data)
{
    return objectListFromJson<Data>(parseJson(data));
}

template<typename T>
nx::kit::Json objectListToJson(const std::vector<T>& objectList)
{
    auto result = nx::kit::Json::array();
    for (const auto& o: objectList)
        result.push_back(nx::kit::Json(o));

    return result;
}

} // namespace

IntegrationManifest::IntegrationManifest(const char* jsonStr): IntegrationManifest(parseJson(jsonStr))
{
}

IntegrationManifest::IntegrationManifest(const nx::kit::Json& json)
{
    id = getStringValue(json, "id");
    name = getStringValue(json, "name");
    description = getStringValue(json, "description");
    version = getStringValue(json, "version");
    vendor = getStringValue(json, "vendor");
}

ValueOrError<IntegrationManifest> IntegrationManifest::fromJson(const char* jsonStr) noexcept
{
    try
    {
        return ValueOrError<IntegrationManifest>::makeValue(IntegrationManifest(jsonStr));
    }
    catch (const std::exception& e)
    {
        return ValueOrError<IntegrationManifest>::makeError(e.what());
    }
}

ValueOrError<IntegrationManifest> IntegrationManifest::fromJson(const nx::kit::Json& json) noexcept
{
    try
    {
        return ValueOrError<IntegrationManifest>::makeValue(IntegrationManifest(json));
    }
    catch (const std::exception& e)
    {
        return ValueOrError<IntegrationManifest>::makeError(e.what());
    }
}

IntegrationManifest::IntegrationManifest(
    const std::string& id,
    const std::string& name,
    const std::string& description,
    const std::string& version,
    const std::string& vendor)
    :
    id(id),
    name(name),
    description(description),
    version(version),
    vendor(vendor)
{
}

nx::kit::Json IntegrationManifest::to_json() const
{
    return nx::kit::Json::object({
        {"id", id},
        {"name", name},
        {"description", description},
        {"version", version},
        {"vendor", vendor},
        });
}

bool IntegrationManifest::operator==(const IntegrationManifest& other) const
{
    return other.description == description && other.id == id && other.name == name
        && other.vendor == vendor && other.version == version;
}

bool TimePeriod::contains(std::chrono::milliseconds timestamp) const
{
    return (timestamp >= startTimestamp)
        && (isInfinite() || timestamp < startTimestamp + duration);
}

bool TimePeriod::isInfinite() const
{
    return duration == -1ms;
}

bool TimePeriod::isNull() const
{
    return startTimestamp == -1ms;
}

bool TimePeriod::intersects(const TimePeriod& other) const
{
    if (isNull() || other.isNull())
        return false;

    if (startTimestamp == other.startTimestamp)
        return true;

    if (startTimestamp > other.startTimestamp)
    {
        if (other.isInfinite() || other.endTimestamp() > startTimestamp)
            return true;
    }

    if (other.startTimestamp > startTimestamp)
    {
        if (isInfinite() || endTimestamp() > other.startTimestamp)
            return true;
    }

    return false;
}

std::optional<std::chrono::milliseconds> TimePeriod::endTimestamp() const
{
    if (isInfinite())
        return std::nullopt;

    return startTimestamp + duration;
}

nx::kit::Json TimePeriod::to_json() const
{
    return nx::kit::Json::object(
        {{"startTimestamp", nx::kit::Json((double) startTimestamp.count())},
        {"duration", (double) duration.count()} });
}

bool TimePeriod::operator<(const TimePeriod& other) const
{
    return this->startTimestamp < other.startTimestamp;
}

bool TimePeriod::operator==(const TimePeriod& other) const
{
    return startTimestamp == other.startTimestamp && duration == other.duration;
}

bool TimePeriod::operator!=(const TimePeriod& other) const
{
    return !operator==(other);
}

KeyValuePair::KeyValuePair(const char* jsonStr): KeyValuePair(parseJson(jsonStr))
{
}

KeyValuePair::KeyValuePair(const nx::kit::Json& json)
{
    if (!json.is_object())
        throw std::logic_error("KeyValuePair is not an object");

    name = getStringValue(json, "name");
    value = getStringValue(json, "value");
}

ValueOrError<KeyValuePair> KeyValuePair::fromJson(const char* jsonStr) noexcept
{
    try
    {
        return ValueOrError<KeyValuePair>::makeValue(KeyValuePair(jsonStr));
    }
    catch (const std::exception& e)
    {
        return ValueOrError<KeyValuePair>::makeError(e.what());
    }
}

ValueOrError<KeyValuePair> KeyValuePair::fromJson(const nx::kit::Json& json) noexcept
{
    try
    {
        return ValueOrError<KeyValuePair>::makeValue(KeyValuePair(json));
    }
    catch (const std::exception& e)
    {
        return ValueOrError<KeyValuePair>::makeError(e.what());
    }
}

KeyValuePair::KeyValuePair(const std::string& name, const std::string& value):
    name(name), value(value)
{
}

nx::kit::Json KeyValuePair::to_json() const
{
    return nx::kit::Json::object({
        {"name", name},
        {"value", value},
        });
}

bool KeyValuePair::operator==(const KeyValuePair& other) const
{
    return other.name == name && other.value == value;
}

nx::kit::Json keyValuePairsToJson(const std::vector<KeyValuePair>& keyValuePairList)
{
    auto result = nx::kit::Json::array();
    for (const auto& p: keyValuePairList)
        result.push_back(nx::kit::Json(p));

    return result;
}

nx::kit::Json DeviceDescription::to_json() const
{
    return nx::kit::Json::object({
        {"parameters", keyValuePairsToJson(parameters)}
        });
}

DeviceDescription::DeviceDescription(const char* jsonData): DeviceDescription(parseJson(jsonData))
{
}

DeviceDescription::DeviceDescription(const nx::kit::Json& json)
{
    parameters = getObjectListValue<KeyValuePair>(json, "parameters");
}

ValueOrError<DeviceDescription> DeviceDescription::fromJson(const char* jsonStr) noexcept
{
    try
    {
        return ValueOrError<DeviceDescription>::makeValue(DeviceDescription(jsonStr));
    }
    catch (const std::exception& e)
    {
        return ValueOrError<DeviceDescription>::makeError(e.what());
    }
}

ValueOrError<DeviceDescription> DeviceDescription::fromJson(const nx::kit::Json& json) noexcept
{
    try
    {
        return ValueOrError<DeviceDescription>::makeValue(DeviceDescription(json));
    }
    catch (const std::exception& e)
    {
        return ValueOrError<DeviceDescription>::makeError(e.what());
    }
}

DeviceDescription::DeviceDescription(const nx::sdk::IDeviceInfo* info)
{
    parameters.push_back(DeviceParameter{ "id", info->id() });
    parameters.push_back(
        DeviceParameter{ "channelNumber", std::to_string(info->channelNumber()) });

    parameters.push_back(DeviceParameter{ "firmware", info->firmware() });
    parameters.push_back(DeviceParameter{ "logicalId", info->logicalId() });
    parameters.push_back(DeviceParameter{ "login", info->login() });
    parameters.push_back(DeviceParameter{ "model", info->model() });
    parameters.push_back(DeviceParameter{ "name", info->name() });
    parameters.push_back(DeviceParameter{ "password", info->password() });
    parameters.push_back(DeviceParameter{ "sharedId", info->sharedId() });
    parameters.push_back(DeviceParameter{ "url", info->url() });
    parameters.push_back(DeviceParameter{ "vendor", info->vendor() });
}

bool DeviceDescription::operator==(const DeviceDescription& other) const
{
    return other.parameters == parameters;
}

std::optional<std::string> DeviceDescription::deviceId() const
{
    const std::string result = getParamValue("id");
    if (result.empty())
        throw std::logic_error("No id in device description");

    return result.empty() ? std::nullopt : std::optional<std::string>(result);
}

std::string deviceId(nx::sdk::cloud_storage::IDeviceAgent* deviceAgent)
{
    return nx::sdk::Ptr(deviceAgent->deviceInfo().value())->id();
}

int toStreamIndex(nxcip::MediaStreamQuality quality)
{
    switch (quality)
    {
        case nxcip::msqDefault:
        case nxcip::msqHigh:
            return 0;
        case nxcip::msqLow:
            return 1;
    }

    throw std::logic_error("Unknown MediaStreamQuality");
}

std::string toString(sdk::cloud_storage::MetadataType metadataType)
{
    switch (metadataType)
    {
        case sdk::cloud_storage::MetadataType::motion: return "motion";
    }

    throw std::logic_error("Unknown MetadataType");
}

std::string sortOrderToString(SortOrder order)
{
    switch (order)
    {
        case SortOrder::ascending:
            return "ascending";
        case SortOrder::descending:
            return "descending";
    }

    throw std::logic_error("Unexpected value of the SortOrder");
}

SortOrder sortOrderFromString(const std::string& s)
{
    if (s == "ascending")
        return SortOrder::ascending;

    if (s == "descending")
        return SortOrder::descending;

    throw std::logic_error("Unexpected value of the SortOrder string: " + s);
}

std::string trackImageTypeToString(TrackImageType type)
{
    switch (type)
    {
        case TrackImageType::bestShot:
            return "bestShot";
        case TrackImageType::title:
            return "title";
    }

    std::stringstream ss;
    ss << "Unexpected value of the TrackeImageType: " << (int) type;
    throw std::logic_error(ss.str());
}

TrackImageType trackImageTypeFromString(const std::string& s)
{
    if (s == "bestShot")
        return TrackImageType::bestShot;

    if (s == "title")
        return TrackImageType::title;

    throw std::logic_error("Unexpected value of the TrackImageType string: " + s);
}

TimePeriod::TimePeriod(const char* jsonStr): TimePeriod(parseJson(jsonStr))
{
}

TimePeriod::TimePeriod(const nx::kit::Json& json)
{
    startTimestamp = milliseconds(getIntValue(json, "startTimestamp"));
    duration = milliseconds(getIntValue(json, "duration"));
}
TimePeriod::TimePeriod(
    std::chrono::milliseconds startTimestamp,
    std::chrono::milliseconds duration)
    :
    startTimestamp(startTimestamp),
    duration(duration)
{
}
TimePeriod::TimePeriod(
    const int64_t startTimeMs,
    const int64_t durationMs)
    :
    startTimestamp(milliseconds(startTimeMs)),
    duration(milliseconds(durationMs))
{
}

CodecInfoData::CodecInfoData()
{
}

const char* DeviceDescription::getParamValue(const std::string& key) const
{
    const auto it = std::find_if(
        parameters.cbegin(), parameters.cend(),
        [&key](const auto& p) { return p.name == key; });

    if (it == parameters.cend())
        return "";

    return it->value.data();
}

IDeviceInfo* deviceInfo(const DeviceDescription& deviceDescription)
{
    auto result = new nx::sdk::DeviceInfo();

    auto get = [&](const std::string& key) { return deviceDescription.getParamValue(key); };
    const std::string channelNumberStr = get("channelNumber");
    int channelNumber = channelNumberStr == "" ? -1 : std::stoi(channelNumberStr);

    result->setChannelNumber(channelNumber);
    result->setFirmware(get("firmware"));
    result->setId(get("id"));
    result->setLogicalId(get("logicalId"));
    result->setLogin(get("login"));
    result->setModel(get("model"));
    result->setName(get("name"));
    result->setPassword(get("password"));
    result->setSharedId(get("sharedId"));
    result->setUrl(get("url"));
    result->setVendor(get("vendor"));

    return result;
}

CodecInfoData::CodecInfoData(const char* jsonData): CodecInfoData(parseJson(jsonData))
{}

CodecInfoData::CodecInfoData(const nx::kit::Json& json)
{
    compressionType = (nxcip::CompressionType) getIntValue(json, "compressionType");
    pixelFormat = (nxcip::PixelFormat) getIntValue(json, "pixelFormat");
    mediaType = (nxcip::MediaType) getIntValue(json, "mediaType");
    width = getIntValue(json, "width");
    height = getIntValue(json, "height");
    codecTag = getIntValue(json, "codecTag");
    bitRate = getIntValue(json, "bitRate");
    channels = getIntValue(json, "channels");
    frameSize = getIntValue(json, "frameSize");
    blockAlign = getIntValue(json, "blockAlign");
    sampleRate = getIntValue(json, "sampleRate");
    sampleFormat = (nxcip::SampleFormat) getIntValue(json, "sampleFormat");
    bitsPerCodedSample = getIntValue(json, "bitsPerCodedSample");
    channelLayout = getIntValue(json, "channelLayout");
    channelNumber = getIntValue(json, "channelNumber");
    extradataBase64 = getStringValue(json, "extradataBase64");
}

ValueOrError<CodecInfoData> CodecInfoData::fromJson(const char* jsonStr) noexcept
{
    try
    {
        return ValueOrError<CodecInfoData>::makeValue(CodecInfoData(jsonStr));
    }
    catch (const std::exception& e)
    {
        return ValueOrError<CodecInfoData>::makeError(e.what());
    }
}

ValueOrError<CodecInfoData> CodecInfoData::fromJson(const nx::kit::Json& json) noexcept
{
    try
    {
        return ValueOrError<CodecInfoData>::makeValue(CodecInfoData(json));
    }
    catch (const std::exception& e)
    {
        return ValueOrError<CodecInfoData>::makeError(e.what());
    }
}

bool CodecInfoData::operator==(const CodecInfoData& other) const
{
    return other.bitRate == bitRate && other.bitsPerCodedSample == bitsPerCodedSample
        && other.blockAlign == blockAlign && other.channelLayout == channelLayout
        && other.channelNumber == channelNumber && other.channels == channels
        && other.codecTag == codecTag && other.compressionType == compressionType
        && other.extradataBase64 == extradataBase64 && other.frameSize == frameSize
        && other.height == height && other.mediaType == mediaType
        && other.pixelFormat == pixelFormat && other.sampleFormat == sampleFormat
        && other.sampleRate == sampleRate && other.width == width;
}

nx::kit::Json CodecInfoData::to_json() const
{
    return nx::kit::Json::object({
        {"compressionType", (int) compressionType},
        {"pixelFormat", (int) pixelFormat},
        {"mediaType", (int) mediaType},
        {"width", width},
        {"height", height},
        {"codecTag", (double) codecTag},
        {"bitRate", (double) bitRate},
        {"channels", channels},
        {"frameSize", frameSize},
        {"blockAlign", blockAlign},
        {"sampleRate", sampleRate},
        {"sampleFormat", (int) sampleFormat},
        {"bitsPerCodedSample", bitsPerCodedSample},
        {"channelLayout", (double) channelLayout},
        {"channelNumber", channelNumber},
        {"extradataBase64", extradataBase64}
        });
}

CodecInfoData::CodecInfoData(const ICodecInfo* info)
{
    compressionType = info->compressionType();
    pixelFormat = info->pixelFormat();
    mediaType = info->mediaType();
    width = info->width();
    height = info->height();
    codecTag = info->codecTag();
    bitRate = info->bitRate();
    channels = info->channels();
    frameSize = info->frameSize();
    blockAlign = info->blockAlign();
    sampleRate = info->sampleRate();
    sampleFormat = info->sampleFormat();
    bitsPerCodedSample = info->bitsPerCodedSample();
    channelLayout = info->channelLayout();
    if (info->extradataSize() > 0)
    {
        extradataBase64 = nx::sdk::cloud_storage::toBase64(
            info->extradata(), info->extradataSize());
    }

    channelNumber = info->channelNumber();
}

MediaPacketData::MediaPacketData(
    const IMediaDataPacket* mediaPacket, std::chrono::milliseconds startTime)
{
    data.assign(
        (const uint8_t*) mediaPacket->data(),
        (const uint8_t*) mediaPacket->data() + mediaPacket->dataSize());

    dataSize = mediaPacket->dataSize();
    compressionType = mediaPacket->codecType();
    timestampUs = mediaPacket->timestampUs() - startTime.count() * 1000;
    type = mediaPacket->type();
    channelNumber = mediaPacket->channelNumber();
    isKeyFrame = mediaPacket->isKeyFrame();

    if (mediaPacket->encryptionDataSize() > 0)
    {
        encryptionData.assign((const uint8_t*) mediaPacket->encryptionData(),
            (const uint8_t*) mediaPacket->encryptionData() + mediaPacket->encryptionDataSize());
    }
}

CloudDeviceReportEntry::CloudDeviceReportEntry(const char* jsonData): CloudDeviceReportEntry(parseJson(jsonData))
{}

CloudDeviceReportEntry::CloudDeviceReportEntry(const nx::kit::Json& json)
{
    id = getStringValue(json, "id");
    serviceId = getStringValue(json, "serviceId");
}

ValueOrError<CloudDeviceReportEntry> CloudDeviceReportEntry::fromJson(const char* jsonStr) noexcept
{
    try
    {
        return ValueOrError<CloudDeviceReportEntry>::makeValue(CloudDeviceReportEntry(jsonStr));
    }
    catch (const std::exception& e)
    {
        return ValueOrError<CloudDeviceReportEntry>::makeError(e.what());
    }
}

ValueOrError<CloudDeviceReportEntry> CloudDeviceReportEntry::fromJson(const nx::kit::Json& json) noexcept
{
    try
    {
        return ValueOrError<CloudDeviceReportEntry>::makeValue(CloudDeviceReportEntry(json));
    }
    catch (const std::exception& e)
    {
        return ValueOrError<CloudDeviceReportEntry>::makeError(e.what());
    }
}

nx::kit::Json CloudDeviceReportEntry::to_json() const
{
    return nx::kit::Json::object({
        {"id", id},
        {"serviceId", serviceId}
    });
}

bool CloudDeviceReportEntry::operator==(const CloudDeviceReportEntry& other) const
{
    return id == other.id && serviceId == other.serviceId;
}

CloudDeviceReport::CloudDeviceReport(const char* jsonData): CloudDeviceReport(parseJson(jsonData))
{}

CloudDeviceReport::CloudDeviceReport(const nx::kit::Json& json)
{
    cloudSystemId = getStringValue(json, "cloudSystemId");
    devices = getObjectListValue<CloudDeviceReportEntry>(json, "devices");
}

ValueOrError<CloudDeviceReport> CloudDeviceReport::fromJson(const char* jsonStr) noexcept
{
    try
    {
        return ValueOrError<CloudDeviceReport>::makeValue(CloudDeviceReport(jsonStr));
    }
    catch (const std::exception& e)
    {
        return ValueOrError<CloudDeviceReport>::makeError(e.what());
    }
}

ValueOrError<CloudDeviceReport> CloudDeviceReport::fromJson(const nx::kit::Json& json) noexcept
{
    try
    {
        return ValueOrError<CloudDeviceReport>::makeValue(CloudDeviceReport(json));
    }
    catch (const std::exception& e)
    {
        return ValueOrError<CloudDeviceReport>::makeError(e.what());
    }
}

nx::kit::Json CloudDeviceReport::to_json() const
{
    return nx::kit::Json::object({
        {"cloudSystemId", cloudSystemId},
        {"devices", devices},
    });
}

bool CloudDeviceReport::operator==(const CloudDeviceReport& other) const
{
    return cloudSystemId == other.cloudSystemId && devices == other.devices;
}

} // namespace nx::sdk::cloud_storage
