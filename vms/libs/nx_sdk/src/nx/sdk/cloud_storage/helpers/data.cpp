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

// string or int64_t are supported so far
template<typename T>
std::vector<T> getBasicListValue(const nx::kit::Json& json, const std::string& key)
{
    const auto value = json[key];
    if (!value.is_array())
        throw std::logic_error("Value '" + key + "' is not an array");

    std::vector<T> result;
    for (const auto& entry: value.array_items())
    {
        if constexpr (std::is_same_v<std::string, T>)
        {
            if (!entry.is_string())
                throw std::logic_error("Value '" + key + "' is not an array of strings");
            result.push_back(entry.string_value());
        }
        else if constexpr (std::is_same_v<int64_t, T>)
        {
            if (!entry.is_number())
                throw std::logic_error("Value '" + key + "' is not an array of numbers");
            result.push_back(entry.number_value());
        }
        else
        {
            throw std::logic_error("Unsupported array value");
        }
    }

    return result;
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
        case sdk::cloud_storage::MetadataType::analytics: return "analytics";
        case sdk::cloud_storage::MetadataType::motion: return "motion";
        case sdk::cloud_storage::MetadataType::bookmark: return "bookmark";
    }

    throw std::logic_error("Unknown MetadataType");
}

Bookmark::Bookmark(const char* jsonData): Bookmark(parseJson(jsonData))
{}

Bookmark::Bookmark(const nx::kit::Json& json)
{
    id = getStringValue(json, "id");
    creatorId = getStringValue(json, "creatorId");
    creationTimestamp = milliseconds(getIntValue(json, "creationTimestamp"));
    name = getStringValue(json, "name");
    description = getStringValue(json, "description");
    timeout = milliseconds(getIntValue(json, "timeout"));
    startTimestamp = milliseconds(getIntValue(json, "startTimestamp"));
    duration = milliseconds(getIntValue(json, "duration"));
    tags = getBasicListValue<std::string>(json, "tags");
    deviceId = getStringValue(json, "deviceId");
}

ValueOrError<Bookmark> Bookmark::fromJson(const char* jsonStr) noexcept
{
    try
    {
        return ValueOrError<Bookmark>::makeValue(Bookmark(jsonStr));
    }
    catch (const std::exception& e)
    {
        return ValueOrError<Bookmark>::makeError(e.what());
    }
}

ValueOrError<Bookmark> Bookmark::fromJson(const nx::kit::Json& json) noexcept
{
    try
    {
        return ValueOrError<Bookmark>::makeValue(Bookmark(json));
    }
    catch (const std::exception& e)
    {
        return ValueOrError<Bookmark>::makeError(e.what());
    }
}

bool Bookmark::operator==(const Bookmark& other) const
{
    return other.creationTimestamp == creationTimestamp && other.creatorId == creatorId
        && other.description == description && other.deviceId == deviceId && other.duration == duration
        && other.id == id && other.name == name && other.startTimestamp == startTimestamp
        && other.tags == tags && other.timeout == timeout;
}

nx::kit::Json Bookmark::to_json() const
{
    return nx::kit::Json::object({
        {"id", id},
        {"creatorId", creatorId},
        {"creationTimestamp", (double) creationTimestamp.count()},
        {"name", name},
        {"description", description},
        {"timeout", (double) timeout.count()},
        {"startTimestamp", (double) startTimestamp.count()},
        {"duration", (double) duration.count()},
        {"tags", tags},
        {"deviceId", deviceId}
        });
}

BookmarkList bookmarkListFromJson(const char* data)
{
    const auto json = parseJson(data);
    if (!json.is_array())
        throw std::logic_error("Bookmark list is not an array");

    BookmarkList result;
    for (const auto& bookmarkJson: json.array_items())
    {
        if (!bookmarkJson.is_object())
            throw std::logic_error("Bookmark list is not an object");

        result.push_back(Bookmark(bookmarkJson));
    }

    return result;
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

std::string BookmarkFilter::sortColumnToString(SortColumn column)
{
    switch (column)
    {
        case SortColumn::creationTime:
            return "creationTime";
        case SortColumn::description:
            return "description";
        case SortColumn::duration:
            return "duration";
        case SortColumn::name:
            return "name";
        case SortColumn::startTime:
            return "startTime";
        case SortColumn::tags:
            return "tags";
    }

    throw std::logic_error("Unexpected value of the SortColumn");
}

BookmarkFilter::SortColumn BookmarkFilter::sortColumnFromString(const std::string& s)
{
    if (s == "creationTime")
        return SortColumn::creationTime;
    if (s == "description")
        return SortColumn::description;
    if (s == "duration")
        return SortColumn::duration;
    if (s == "name")
        return SortColumn::name;
    if (s == "startTime")
        return SortColumn::startTime;
    if (s == "tags")
        return SortColumn::tags;

    throw std::logic_error("Unexpected value of the SortColumn string: " + s);
}


BookmarkFilter::BookmarkFilter(const char* urlParams)
{
    std::vector<std::string> params = split(urlParams, "&");
    for (int i = 0; i < params.size(); i++) {
        std::vector<std::string> param = split(params[i], "=");
        if (param.size() != 2)
            continue;

        if (param[0].compare("id") == 0)
            id = param[1];
        else if (param[0].compare("startTimestamp") == 0)
            startTimestamp = std::optional<std::chrono::milliseconds>(std::stoi(param[1]));
        else if (param[0].compare("endTimestamp") == 0)
            endTimestamp = std::optional<std::chrono::milliseconds>(std::stoi(param[1]));
        else if (param[0].compare("text") == 0)
            text = param[1];
        else if (param[0].compare("limit") == 0)
            limit = std::optional<int>(std::stoi(param[1]));
        else if (param[0].compare("order") == 0)
            order = sortOrderFromString(param[1]);
        else if (param[0].compare("column") == 0)
            column = sortColumnFromString(param[1]);
        else if (param[0].compare("minVisibleLength") == 0)
            minVisibleLength = std::optional<std::chrono::milliseconds>(std::stoi(param[1]));
        else if (param[0].compare("deviceIds") == 0)
            deviceIds = split(param[1], ",");
        else if (param[0].compare("creationStartTimestamp") == 0)
            creationStartTimestamp = std::optional<std::chrono::milliseconds>(std::stoi(param[1]));
        else if (param[0].compare("creationEndTimestamp") == 0)
            creationEndTimestamp = std::optional<std::chrono::milliseconds>(std::stoi(param[1]));
    }
}

BookmarkFilter::BookmarkFilter(const nx::kit::Json& json)
{
    id = getOptionalStringValue(json, "id");
    startTimestamp = getOptionalDurationValue<milliseconds>(json, "startTimestamp");
    endTimestamp = getOptionalDurationValue<milliseconds>(json, "endTimestamp");
    text = getOptionalStringValue(json, "text");
    const auto optionalLimit = getOptionalIntValue(json, "limit");
    limit = optionalLimit ? std::optional<int>((int) *optionalLimit) : std::nullopt;
    order = sortOrderFromString(getStringValue(json, "order"));
    column = sortColumnFromString(getStringValue(json, "column"));
    minVisibleLength = getOptionalDurationValue<milliseconds>(json, "minVisibleLength");
    deviceIds = getBasicListValue<std::string>(json, "deviceIds");
    creationStartTimestamp = getOptionalDurationValue<milliseconds>(json, "creationStartTimestamp");
    creationEndTimestamp = getOptionalDurationValue<milliseconds>(json, "creationEndTimestamp");
}

ValueOrError<BookmarkFilter> BookmarkFilter::fromUrlParams(const char* urlParams) noexcept
{
    try
    {
        return ValueOrError<BookmarkFilter>::makeValue(BookmarkFilter(urlParams));
    }
    catch (const std::exception& e)
    {
        return ValueOrError<BookmarkFilter>::makeError(e.what());
    }
}

ValueOrError<BookmarkFilter> BookmarkFilter::fromJson(const nx::kit::Json& json) noexcept
{
    try
    {
        return ValueOrError<BookmarkFilter>::makeValue(BookmarkFilter(json));
    }
    catch (const std::exception& e)
    {
        return ValueOrError<BookmarkFilter>::makeError(e.what());
    }
}

bool BookmarkFilter::operator==(const BookmarkFilter& other) const
{
    return other.column == column && other.creationEndTimestamp == creationEndTimestamp
        && other.creationStartTimestamp == creationStartTimestamp && other.deviceIds == deviceIds
        && other.endTimestamp == endTimestamp && other.id == id && other.limit == limit
        && other.minVisibleLength == minVisibleLength && other.order == order
        && other.startTimestamp == startTimestamp && other.text == text;
}

nx::kit::Json BookmarkFilter::to_json() const
{
    auto result =  nx::kit::Json::object({
        {"order", sortOrderToString(order)},
        {"column", sortColumnToString(column)},
        });

    if (id)
        result["id"] = *id;

    if (startTimestamp)
        result["startTimestamp"] = (double) startTimestamp->count();

    if (endTimestamp)
        result["endTimestamp"] = (double) endTimestamp->count();

    if (text)
        result["text"] = *text;

    if (limit)
        result["limit"] = *limit;

    if (minVisibleLength)
        result["minVisibleLength"] = (double) minVisibleLength->count();

    if (creationStartTimestamp)
        result["creationStartTimestamp"] = (double) creationStartTimestamp->count();

    if (creationEndTimestamp)
        result["creationEndTimestamp"] = (double) creationEndTimestamp->count();

    result["deviceIds"] = objectListToJson(deviceIds);
    return result;
}

std::string BookmarkFilter::toUrlParams() const
{
    std::stringstream ss;
    ss << "order=" << sortOrderToString(order);
    ss << "&column=" << sortColumnToString(column);

    if (id)
        ss << "&id=" << *id;

    if (startTimestamp)
        ss << "&startTimestamp=" << startTimestamp->count();

    if (endTimestamp)
        ss << "&endTimestamp=" << endTimestamp->count();

    if (text)
        ss << "&text=" << *text;

    if (limit)
        ss << "&limit=" << *limit;

    if (minVisibleLength)
        ss << "&minVisibleLength=" << minVisibleLength->count();

    if (creationStartTimestamp)
        ss << "&creationStartTimestamp=" << creationStartTimestamp->count();

    if (creationEndTimestamp)
        ss << "&creationEndTimestamp=" << creationEndTimestamp->count();

    if (!deviceIds.empty()) {
        ss << "&deviceIds=";
        for (int i = 0; i < deviceIds.size(); i++) {
            if (i > 0) ss << ',';
            ss << deviceIds[i];
        }
    }

    return ss.str();
}

Motion::Motion(const char* jsonData): Motion(parseJson(jsonData))
{
}

Motion::Motion(const nx::kit::Json& json)
{
    channel = getIntValue(json, "channel");
    startTimestamp = milliseconds(getIntValue(json, "startTimestamp"));
    duration = milliseconds(getIntValue(json, "duration"));
    deviceId = getStringValue(json, "deviceId");
    dataBase64 = getStringValue(json, "dataBase64");
}

ValueOrError<Motion> Motion::fromJson(const char* jsonStr) noexcept
{
    try
    {
        return ValueOrError<Motion>::makeValue(Motion(jsonStr));
    }
    catch (const std::exception& e)
    {
        return ValueOrError<Motion>::makeError(e.what());
    }
}

ValueOrError<Motion> Motion::fromJson(const nx::kit::Json& json) noexcept
{
    try
    {
        return ValueOrError<Motion>::makeValue(Motion(json));
    }
    catch (const std::exception& e)
    {
        return ValueOrError<Motion>::makeError(e.what());
    }
}

bool Motion::operator==(const Motion& other) const
{
    return other.channel == channel && other.dataBase64 == dataBase64 && other.deviceId == deviceId
        && other.duration == duration && other.startTimestamp == startTimestamp;
}

nx::kit::Json Motion::to_json() const
{
    return nx::kit::Json::object({
        {"channel", channel},
        {"startTimestamp", (double) startTimestamp.count()},
        {"duration", (double) duration.count()},
        {"deviceId", deviceId},
        {"dataBase64", dataBase64},
        });
}

Rect::Rect(const char* jsonData): Rect(parseJson(jsonData))
{
}

Rect::Rect(const nx::kit::Json& json)
{
    x = getDoubleValue(json, "x");
    y = getDoubleValue(json, "y");
    w = getDoubleValue(json, "w");
    h = getDoubleValue(json, "h");
}

ValueOrError<Rect> Rect::fromJson(const char* jsonStr) noexcept
{
    try
    {
        return ValueOrError<Rect>::makeValue(Rect(jsonStr));
    }
    catch (const std::exception& e)
    {
        return ValueOrError<Rect>::makeError(e.what());
    }
}

ValueOrError<Rect> Rect::fromJson(const nx::kit::Json& json) noexcept
{
    try
    {
        return ValueOrError<Rect>::makeValue(Rect(json));
    }
    catch (const std::exception& e)
    {
        return ValueOrError<Rect>::makeError(e.what());
    }
}

Rect::Rect(double x, double y, double w, double h):
    x(x), y(y), w(w), h(h)
{}

bool Rect::operator==(const Rect& other) const
{
    auto comparator = [](double l, double r)
    {
        return abs(l - r) < 1.0e-06;
    };
    return comparator(other.h, h) && comparator(other.w, w) && comparator(other.x, x)
        && comparator(other.y, y);
}

bool Rect::isInside(double x, double y) const
{
    return x >= this->x && x <= this->x + this->w && y <= this->y && y >= this->y - this->w;
}

bool Rect::isEmpty() const
{
    return w <= 0 || h <= 0;
}

bool Rect::intersectsWith(const Rect& other) const
{
    return isInside(other.left(), other.top()) || isInside(other.left(), other.bottom())
        || isInside(other.right(), other.top()) || isInside(other.right(), other.bottom());
}

double Rect::left() const
{
    return x;
}

double Rect::top() const
{
    return y;
}

double Rect::right() const
{
    return x + w;
}

double Rect::bottom() const
{
    return y + h;
}

nx::kit::Json Rect::to_json() const
{
    return nx::kit::Json::object({
        {"x", x},
        {"y", y},
        {"w", w},
        {"h", h},
        });
}

TimePeriod::TimePeriod(const char* jsonStr): TimePeriod(parseJson(jsonStr))
{
}

TimePeriod::TimePeriod(const nx::kit::Json& json)
{
    startTimestamp = milliseconds(getIntValue(json, "startTimestamp"));
    duration = milliseconds(getIntValue(json, "duration"));
}

ValueOrError<TimePeriod> TimePeriod::fromJson(const char* jsonStr) noexcept
{
    try
    {
        return ValueOrError<TimePeriod>::makeValue(TimePeriod(jsonStr));
    }
    catch (const std::exception& e)
    {
        return ValueOrError<TimePeriod>::makeError(e.what());
    }
}

ValueOrError<TimePeriod> TimePeriod::fromJson(const nx::kit::Json& json) noexcept
{
    try
    {
        return ValueOrError<TimePeriod>::makeValue(TimePeriod(json));
    }
    catch (const std::exception& e)
    {
        return ValueOrError<TimePeriod>::makeError(e.what());
    }
}

TimePeriod::TimePeriod(
    std::chrono::milliseconds startTimestamp,
    std::chrono::milliseconds duration)
    :
    startTimestamp(startTimestamp),
    duration(duration)
{
}

TimePeriodList::TimePeriodList(const char* jsonStr): TimePeriodList(parseJson(jsonStr))
{
}

TimePeriodList::TimePeriodList(SortOrder order): m_order(order)
{
}

TimePeriodList::TimePeriodList(const nx::kit::Json& json) noexcept(false)
{
    m_order = sortOrderFromString(getStringValue(json, "order"));
    m_periods = getBasicListValue<int64_t>(json, "periods");
    recalculateLastTimestamp();
}

ValueOrError<TimePeriodList> TimePeriodList::fromJson(const char* jsonStr) noexcept
{
    try
    {
        return ValueOrError<TimePeriodList>::makeValue(TimePeriodList(jsonStr));
    }
    catch (const std::exception& e)
    {
        return ValueOrError<TimePeriodList>::makeError(e.what());
    }
}

ValueOrError<TimePeriodList> TimePeriodList::fromJson(const nx::kit::Json& json) noexcept
{
    try
    {
        return ValueOrError<TimePeriodList>::makeValue(TimePeriodList(json));
    }
    catch (const std::exception& e)
    {
        return ValueOrError<TimePeriodList>::makeError(e.what());
    }
}

void TimePeriodList::forEach(std::function<void(const TimePeriod&)> func) const
{
    if (m_periods.size() < 2)
        return;

    auto p = TimePeriod(milliseconds(m_periods[0]), milliseconds(m_periods[1]));
    func(p);

    for (size_t i = 3; i < m_periods.size(); i += 2)
    {
        if (m_order == SortOrder::ascending)
        {
            p = TimePeriod(
                milliseconds(m_periods[i - 1]) + p.startTimestamp,
                milliseconds(m_periods[i]));
        }
        else
        {
            p = TimePeriod(
                p.startTimestamp - milliseconds(m_periods[i - 1]),
                milliseconds(m_periods[i]));
        }

        func(p);
    }
}

SortOrder TimePeriodList::sortOrder() const
{
    return m_order;
}

void TimePeriodList::reverse()
{
    if (!m_periods.empty())
    {
        std::vector<TimePeriod> reversed;
        forEach([&reversed](const auto& p) { reversed.push_back(p); });
        std::reverse(reversed.begin(), reversed.end());
        m_periods.clear();
        m_periods.push_back(reversed[0].startTimestamp.count());
        m_periods.push_back(reversed[0].duration.count());
        for (size_t i = 1; i < reversed.size(); i++)
        {
            if (m_order == SortOrder::ascending)
            {
                m_periods.push_back(
                    reversed[i - 1].startTimestamp.count() - reversed[i].startTimestamp.count());
                m_periods.push_back(reversed[i].duration.count());
            }
            else
            {
                m_periods.push_back(
                    reversed[i].startTimestamp.count() - reversed[i - 1].startTimestamp.count());
                m_periods.push_back(reversed[i].duration.count());
            }
        }
    }

    if (m_order == SortOrder::ascending)
        m_order = SortOrder::descending;
    else
        m_order = SortOrder::ascending;

    recalculateLastTimestamp();
}

void TimePeriodList::recalculateLastTimestamp()
{
    m_lastStartTimestmapMs = std::nullopt;
    const auto lastPeriod = last();
    if (lastPeriod)
        m_lastStartTimestmapMs = lastPeriod->startTimestamp;
}

void TimePeriodList::append(const TimePeriod& period)
{
    if (m_periods.empty())
    {
        m_periods.push_back(period.startTimestamp.count());
    }
    else
    {
        if (m_order == SortOrder::ascending)
        {
            m_periods.push_back(
                period.startTimestamp.count() - m_lastStartTimestmapMs->count());
        }
        else
        {
            m_periods.push_back(
                m_lastStartTimestmapMs->count() - period.startTimestamp.count());
        }
    }
    m_lastStartTimestmapMs = period.startTimestamp;

    m_periods.push_back(period.duration.count());
}

bool TimePeriodList::empty() const
{
    return m_periods.empty();
}

std::optional<TimePeriod> TimePeriodList::last() const
{
    if (m_periods.empty())
        return std::nullopt;

    auto startTimestamp = m_periods[0];
    for (size_t i = 2; i < m_periods.size(); i += 2)
    {
        if (m_order == SortOrder::ascending)
            startTimestamp += m_periods[i];
        else
            startTimestamp -= m_periods[i];
    }

    return TimePeriod{milliseconds(startTimestamp), milliseconds(m_periods.back())};
}

void TimePeriodList::setLastDuration(std::chrono::milliseconds duration)
{
    if (m_periods.empty())
        return;

    m_periods[m_periods.size() - 1] = duration.count();
}

size_t TimePeriodList::size() const
{
    return m_periods.size() / 2;
}

void TimePeriodList::shrink(size_t size)
{
    if (size < this->size())
        m_periods.resize(size * 2);
    recalculateLastTimestamp();
}

nx::kit::Json TimePeriodList::TimePeriodList::to_json() const
{
    auto result = nx::kit::Json::object();
    result["order"] = sortOrderToString(m_order);
    auto periodArray = nx::kit::Json::array();
    for (const int64_t& v: m_periods)
        periodArray.push_back((double) v);

    result["periods"] = periodArray;
    return result;
}

MotionFilter::MotionFilter(const char* jsonData): MotionFilter(parseJson(jsonData))
{
}

MotionFilter::MotionFilter(const nx::kit::Json& json)
{
    deviceIds = getBasicListValue<std::string>(json, "deviceIds");
    timePeriod = getObjectValue<TimePeriod>(json, "timePeriod");
    regions = getObjectListValue<Rect>(json, "regions");
    const auto optionalLimit = getOptionalIntValue(json, "limit");
    limit = optionalLimit ? std::optional<int>((int)*optionalLimit) : std::nullopt;
    detailLevel = milliseconds(getIntValue(json, "detailLevel"));
}

ValueOrError<MotionFilter> MotionFilter::fromJson(const char* jsonStr) noexcept
{
    try
    {
        return ValueOrError<MotionFilter>::makeValue(MotionFilter(jsonStr));
    }
    catch (const std::exception& e)
    {
        return ValueOrError<MotionFilter>::makeError(e.what());
    }
}

ValueOrError<MotionFilter> MotionFilter::fromJson(const nx::kit::Json& json) noexcept
{
    try
    {
        return ValueOrError<MotionFilter>::makeValue(MotionFilter(json));
    }
    catch (const std::exception& e)
    {
        return ValueOrError<MotionFilter>::makeError(e.what());
    }
}

bool MotionFilter::operator==(const MotionFilter& other) const
{
    return other.detailLevel == detailLevel && other.deviceIds == deviceIds && other.limit == limit
        && other.regions == regions && other.timePeriod == timePeriod;
}

nx::kit::Json MotionFilter::to_json() const
{
    auto result = nx::kit::Json::object({
        {"timePeriod", timePeriod},
        {"order", sortOrderToString(order)},
        {"detailLevel", (double) detailLevel.count() },
        });

    auto deviceIdJson = nx::kit::Json::array();
    for (const auto& id: deviceIds)
        deviceIdJson.push_back(nx::kit::Json(id));

    result["deviceIds"] = objectListToJson(deviceIds);
    result["regions"] = objectListToJson(regions);
    if (limit)
        result["limit"] = *limit;

    return result;
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

std::string AnalyticsFilter::optionsToString(int options)
{
    if (options == 0)
        return "none";

    std::string result;
    if (options & Option::ignoreBoundingBox)
        result = "ignoreBoundingBox";

    if (options & Option::ignoreTextFilter)
    {
        if (!result.empty())
            result += "|";

        result += "ignoreTextFilter";
    }

    if (options & Option::ignoreTimePeriod)
    {
        if (!result.empty())
            result += "|";

        result += "ignoreTimePeriod";
    }

    return result;
}

int AnalyticsFilter::optionsFromString(const std::string& s)
{
    if (s.empty())
        throw std::logic_error("Options string is unexpectedly empty");

    if (s == "none")
        return 0;

    int result = 0;
    if (s.find("ignoreBoundingBox") != std::string::npos)
        result |= Option::ignoreBoundingBox;

    if (s.find("ignoreTextFilter") != std::string::npos)
        result |= Option::ignoreTextFilter;

    if (s.find("ignoreTimePeriod") != std::string::npos)
        result |= Option::ignoreTimePeriod;

    if (result == 0)
        throw std::logic_error("Unexpected value of the options string: " + s);

    return result;
}

RangePoint::RangePoint(const nx::kit::Json& json)
{
    value = getDoubleValue(json, "value");
    inclusive = getBoolValue(json, "inclusive");
}

RangePoint::RangePoint(const char* jsonData): RangePoint(parseJson(jsonData))
{
}

ValueOrError<RangePoint> RangePoint::fromJson(const char* jsonStr) noexcept
{
    try
    {
        return ValueOrError<RangePoint>::makeValue(RangePoint(jsonStr));
    }
    catch (const std::exception& e)
    {
        return ValueOrError<RangePoint>::makeError(e.what());
    }
}

ValueOrError<RangePoint> RangePoint::fromJson(const nx::kit::Json& json) noexcept
{
    try
    {
        return ValueOrError<RangePoint>::makeValue(RangePoint(json));
    }
    catch (const std::exception& e)
    {
        return ValueOrError<RangePoint>::makeError(e.what());
    }
}

RangePoint::RangePoint(float value, bool inclusive): value(value), inclusive(inclusive)
{
}

bool RangePoint::operator==(const RangePoint& other) const
{
    return other.inclusive == inclusive && other.value == value;
}

nx::kit::Json RangePoint::to_json() const
{
    return nx::kit::Json::object({{"value", value}, {"inclusive", inclusive}});
}

NumericRange::NumericRange(const nx::kit::Json& json)
{
    from = getOptionalObjectValue<RangePoint>(json, "from");
    to = getOptionalObjectValue<RangePoint>(json, "to");
}

nx::kit::Json NumericRange::to_json() const
{
    auto result = nx::kit::Json::object();
    if (from)
        result["from"] = *from;

    if (to)
        result["to"] = *to;

    return result;
}

ValueOrError<NumericRange> NumericRange::fromJson(const char* jsonStr) noexcept
{
    try
    {
        return ValueOrError<NumericRange>::makeValue(NumericRange(jsonStr));
    }
    catch (const std::exception& e)
    {
        return ValueOrError<NumericRange>::makeError(e.what());
    }
}

ValueOrError<NumericRange> NumericRange::fromJson(const nx::kit::Json& json) noexcept
{
    try
    {
        return ValueOrError<NumericRange>::makeValue(NumericRange(json));
    }
    catch (const std::exception& e)
    {
        return ValueOrError<NumericRange>::makeError(e.what());
    }
}

NumericRange::NumericRange(const char* jsonData): NumericRange(parseJson(jsonData))
{
}

bool NumericRange::operator==(const NumericRange& other) const
{
    return other.from == from && other.to == to;
}

std::optional<NumericRange> NumericRange::fromString(const std::string& s)
{
    try
    {
        float floatValue = std::stof(s);
        return NumericRange(floatValue);
    }
    catch (...)
    {
    }

    bool isValidRange = s.find("...") != std::string::npos;
    if (!isValidRange)
        return std::nullopt;

    auto unquotedValue = s;
    if (unquotedValue[0] == '(' || unquotedValue[0] == '[')
        unquotedValue = unquotedValue.substr(1);
    if (unquotedValue[unquotedValue.size() - 1] == ')'
        || unquotedValue[unquotedValue.size() - 1] == ']')
    {
        unquotedValue = unquotedValue.substr(0, unquotedValue.size() - 1);
    }

    auto params = split(unquotedValue, "...");
    if (params.size() != 2)
        return std::nullopt;

    std::optional<RangePoint> left, right;
    if (params[0] != "-inf")
    {
        left = RangePoint();
        try
        {
            left->value = std::stof(params[0]);
        }
        catch (...)
        {
            return std::nullopt;
        }

        left->inclusive = s[0] != '(';
    }

    if (params[1] != "inf")
    {
        right = RangePoint();
        try
        {
            right->value = std::stof(params[1]);
        }
        catch (...)
        {
            return std::nullopt;
        }

        right->inclusive = s[s.size() - 1] != ')';
    }

    return NumericRange(left, right);
}

bool NumericRange::intersects(const NumericRange& range) const
{
    RangePoint ownLeft = from ? *from : RangePoint{std::numeric_limits<float>::min(), false};
    RangePoint ownRight = to ? *to : RangePoint{std::numeric_limits<float>::max(), false};

    RangePoint rangeLeft =
        range.from ? *range.from : RangePoint{std::numeric_limits<float>::min(), false};
    RangePoint rangeRight = range.to ? *range.to : RangePoint{std::numeric_limits<float>::max(), false};

    RangePoint maxLeft = (ownLeft.value > rangeLeft.value) ? ownLeft : rangeLeft;
    RangePoint minRight = (ownRight.value < rangeRight.value) ? ownRight : rangeRight;

    if (maxLeft.inclusive && minRight.inclusive)
        return maxLeft.value <= minRight.value;
    return maxLeft.value < minRight.value;
}

bool NumericRange::hasRange(const NumericRange& range) const
{
    RangePoint ownLeft = from ? *from : RangePoint{std::numeric_limits<float>::min(), false};
    RangePoint ownRight = to ? *to : RangePoint{std::numeric_limits<float>::max(), false};

    RangePoint rangeLeft =
        range.from ? *range.from : RangePoint{std::numeric_limits<float>::min(), false};
    RangePoint rangeRight = range.to ? *range.to : RangePoint{std::numeric_limits<float>::max(), false};

    if (ownLeft.inclusive || !rangeLeft.inclusive)
    {
        if (ownLeft.value > rangeLeft.value)
            return false;
    }
    else
    {
        if (ownLeft.value >= rangeLeft.value)
            return false;
    }

    if (ownRight.inclusive || !rangeRight.inclusive)
    {
        if (ownRight.value < rangeRight.value)
            return false;
    }
    else
    {
        if (ownRight.value <= rangeRight.value)
            return false;
    }
    return true;
}

AttributeSearchCondition::AttributeSearchCondition(const nx::kit::Json& json)
{
    type = typeFromString(getStringValue(json, "type"));
    name = getStringValue(json, "name");
    value = getStringValue(json, "value");
    text = getStringValue(json, "text");
    range = getObjectValue<NumericRange>(json, "range");
    isNegative = getBoolValue(json, "isNegative");
    matchesFromStart = getBoolValue(json, "matchesFromStart");
    matchesTillEnd = getBoolValue(json, "matchesTillEnd");
}

AttributeSearchCondition::AttributeSearchCondition(const char* jsonData):
    AttributeSearchCondition(parseJson(jsonData))
{
}

ValueOrError<AttributeSearchCondition> AttributeSearchCondition::fromJson(const char* jsonStr) noexcept
{
    try
    {
        return ValueOrError<AttributeSearchCondition>::makeValue(AttributeSearchCondition(jsonStr));
    }
    catch (const std::exception& e)
    {
        return ValueOrError<AttributeSearchCondition>::makeError(e.what());
    }
}

ValueOrError<AttributeSearchCondition> AttributeSearchCondition::fromJson(const nx::kit::Json& json) noexcept
{
    try
    {
        return ValueOrError<AttributeSearchCondition>::makeValue(AttributeSearchCondition(json));
    }
    catch (const std::exception& e)
    {
        return ValueOrError<AttributeSearchCondition>::makeError(e.what());
    }
}

bool AttributeSearchCondition::operator==(const AttributeSearchCondition& other) const
{
    return other.isNegative == isNegative && other.name == name && other.range == range
        && other.text == text && other.type == type && other.value == value
        && other.matchesFromStart == matchesFromStart && other.matchesTillEnd == matchesTillEnd;
}

std::string AttributeSearchCondition::typeToString(Type type)
{
    switch (type)
    {
        case Type::attributePresenceCheck:
            return "attributePresenceCheck";
        case Type::attributeValueMatch:
            return "attributeValueMatch";
        case Type::numericRangeMatch:
            return "numericRangeMatch";
        case Type::textMatch:
            return "textMatch";
    }

    throw std::logic_error("Unknown AttributeSearchCondition::type " + std::to_string((int) type));
}

AttributeSearchCondition::Type AttributeSearchCondition::typeFromString(const std::string& s)
{
    if (s == "attributePresenceCheck")
        return Type::attributePresenceCheck;
    if (s == "attributeValueMatch")
        return Type::attributeValueMatch;
    if (s == "numericRangeMatch")
        return Type::numericRangeMatch;
    if (s == "textMatch")
        return Type::textMatch;

    throw std::logic_error("Unknown AttributeSearchCondition::type string " + s);
}

nx::kit::Json AttributeSearchCondition::to_json() const
{
    return nx::kit::Json::object(
        {{"type", typeToString(type)},
        {"name", name},
        {"value", value},
        {"text", text},
        {"range", range},
        {"isNegative", isNegative},
        {"matchesFromStart", matchesFromStart},
        {"matchesTillEnd", matchesTillEnd},
    });
}

AnalyticsFilter::AnalyticsFilter(const char* jsonData): AnalyticsFilter(parseJson(jsonData))
{
}

AnalyticsFilter::AnalyticsFilter(const nx::kit::Json& json)
{
    deviceIds = getBasicListValue<std::string>(json, "deviceIds");
    objectTypeIds = getBasicListValue<std::string>(json, "objectTypeIds");
    objectTrackId = getOptionalStringValue(json, "objectTrackId");
    timePeriod = getObjectValue<TimePeriod>(json, "timePeriod");
    boundingBox = getOptionalObjectValue<Rect>(json, "boundingBox");
    const auto maybeMaxObjectTracksToSelect = getOptionalIntValue(json, "maxObjectTracksToSelect");
    if (maybeMaxObjectTracksToSelect)
        maxObjectTracksToSelect = (int) *maybeMaxObjectTracksToSelect;

    order = sortOrderFromString(getStringValue(json, "order"));
    analyticsEngineId = getOptionalStringValue(json, "analyticsEngineId");
    options = optionsFromString(getStringValue(json, "options"));
    detailLevel = getDurationValue<milliseconds>(json, "detailLevel");
    attributeSearchConditions =
        getObjectListValue<AttributeSearchCondition>(json, "attributeSearchConditions");
}

ValueOrError<AnalyticsFilter> AnalyticsFilter::fromJson(const char* jsonStr) noexcept
{
    try
    {
        return ValueOrError<AnalyticsFilter>::makeValue(AnalyticsFilter(jsonStr));
    }
    catch (const std::exception& e)
    {
        return ValueOrError<AnalyticsFilter>::makeError(e.what());
    }
}

ValueOrError<AnalyticsFilter> AnalyticsFilter::fromJson(const nx::kit::Json& json) noexcept
{
    try
    {
        return ValueOrError<AnalyticsFilter>::makeValue(AnalyticsFilter(json));
    }
    catch (const std::exception& e)
    {
        return ValueOrError<AnalyticsFilter>::makeError(e.what());
    }
}

bool AnalyticsFilter::operator==(const AnalyticsFilter& other) const
{
    return other.analyticsEngineId == analyticsEngineId
        && other.attributeSearchConditions == attributeSearchConditions
        && other.boundingBox == boundingBox && other.detailLevel == detailLevel
        && other.deviceIds == deviceIds && other.maxObjectTracksToSelect == maxObjectTracksToSelect
        && other.objectTrackId == objectTrackId && other.objectTypeIds == objectTypeIds
        && other.options == options && other.order == order && other.timePeriod == timePeriod;
}

nx::kit::Json AnalyticsFilter::to_json() const
{
    auto result = nx::kit::Json::object();
    result["deviceIds"] = objectListToJson(deviceIds);
    result["objectTypeIds"] = objectListToJson(objectTypeIds);
    if (objectTrackId)
        result["objectTrackId"] = *objectTrackId;

    result["timePeriod"] = timePeriod;
    if (boundingBox)
        result["boundingBox"] = *boundingBox;

    if (maxObjectTracksToSelect)
        result["maxObjectTracksToSelect"] = *maxObjectTracksToSelect;

    result["order"] = sortOrderToString(order);
    if (analyticsEngineId)
        result["analyticsEngineId"] = *analyticsEngineId;

    result["options"] = optionsToString(options);
    result["detailLevel"] = (double) detailLevel.count();
    result["attributeSearchConditions"] = attributeSearchConditions;

    return result;
}

ObjectRegion::ObjectRegion(const char* jsonData): ObjectRegion(parseJson(jsonData))
{
}

bool ObjectRegion::operator==(const ObjectRegion& other) const
{
    return other.boundingBoxGridBase64 == boundingBoxGridBase64;
}

ObjectRegion::ObjectRegion(const nx::kit::Json& json)
{
    boundingBoxGridBase64 = getStringValue(json, "boundingBoxGridBase64");
}

nx::kit::Json ObjectRegion::to_json() const
{
    return nx::kit::Json::object({{"boundingBoxGridBase64", boundingBoxGridBase64}});
}

ValueOrError<ObjectRegion> ObjectRegion::fromJson(const char* jsonStr) noexcept
{
    try
    {
        return ValueOrError<ObjectRegion>::makeValue(ObjectRegion(jsonStr));
    }
    catch (const std::exception& e)
    {
        return ValueOrError<ObjectRegion>::makeError(e.what());
    }
}

ValueOrError<ObjectRegion> ObjectRegion::fromJson(const nx::kit::Json& json) noexcept
{
    try
    {
        return ValueOrError<ObjectRegion>::makeValue(ObjectRegion(json));
    }
    catch (const std::exception& e)
    {
        return ValueOrError<ObjectRegion>::makeError(e.what());
    }
}

BaseImage::BaseImage(const nx::kit::Json& json)
{
    timestamp = microseconds(getIntValue(json, "timestamp"));
    rect = getObjectValue<Rect>(json, "rect");
    streamIndex = getIntValue(json, "streamIndex");
    image = getOptionalObjectValue<Image>(json, "image");
}

BaseImage::BaseImage(const char* jsonData): BaseImage(parseJson(jsonData))
{
}

bool BaseImage::operator==(const BaseImage& other) const
{
    return timestamp == other.timestamp
        && rect == other.rect
        && streamIndex == other.streamIndex
        && image == other.image;
}

nx::kit::Json BaseImage::to_json() const
{
    return toBaseObject();
}

std::map<std::string, nx::kit::detail::json11::Json> BaseImage::toBaseObject() const
{
    auto result = nx::kit::Json::object({
        {"timestamp", (double) timestamp.count()},
        {"rect", rect},
        {"streamIndex", streamIndex}
        });

    if (image)
        result["image"] = *image;

    return result;
}

//------------- Title ----------------------

bool Title::operator==(const Title& other) const
{
    return
        static_cast<const BaseImage&>(*this) == static_cast<const BaseImage&>(other)
        && text == other.text;
}

Title::Title(const nx::kit::Json& json): BaseImage(json)
{
    text = getStringValue(json, "text");
}

Title::Title(const char* jsonStr): Title(parseJson(jsonStr))
{
}

ValueOrError<Title> Title::fromJson(const char* jsonStr) noexcept
{
    try
    {
        return ValueOrError<Title>::makeValue(Title(jsonStr));
    }
    catch (const std::exception& e)
    {
        return ValueOrError<Title>::makeError(e.what());
    }
}

ValueOrError<Title> Title::fromJson(const nx::kit::Json& json) noexcept
{
    try
    {
        return ValueOrError<Title>::makeValue(Title(json));
    }
    catch (const std::exception& e)
    {
        return ValueOrError<Title>::makeError(e.what());
    }
}

nx::kit::Json Title::to_json() const
{
    auto result = toBaseObject();
    result["text"] = text;
    return result;
}

// ------------------------- BestShot ------------------------------------

BestShot::BestShot(const nx::kit::Json& json): BaseImage(json)
{
}

BestShot::BestShot(const char* jsonStr) : BestShot(parseJson(jsonStr))
{
}

bool BestShot::operator==(const BestShot& other) const
{
    return BaseImage::operator==(other);
}

ValueOrError<BestShot> BestShot::fromJson(const char* jsonStr) noexcept
{
    try
    {
        return ValueOrError<BestShot>::makeValue(BestShot(jsonStr));
    }
    catch (const std::exception& e)
    {
        return ValueOrError<BestShot>::makeError(e.what());
    }
}

ValueOrError<BestShot> BestShot::fromJson(const nx::kit::Json& json) noexcept
{
    try
    {
        return ValueOrError<BestShot>::makeValue(BestShot(json));
    }
    catch (const std::exception& e)
    {
        return ValueOrError<BestShot>::makeError(e.what());
    }
}

nx::kit::Json BestShot::to_json() const
{
    return toBaseObject();
}

// ----------ObjectTrack ------------------------

ObjectTrack::ObjectTrack(const char* jsonData): ObjectTrack(parseJson(jsonData))
{
}

ObjectTrack::ObjectTrack(const nx::kit::Json & json)
{
    id = getStringValue(json, "id");
    deviceId = getStringValue(json, "deviceId");
    objectTypeId = getStringValue(json, "objectTypeId");
    attributes = getObjectListValue<KeyValuePair>(json, "attributes");
    firstAppearanceTimestamp = microseconds(getIntValue(json, "firstAppearanceTimestamp"));
    lastAppearanceTimestamp = microseconds(getIntValue(json, "lastAppearanceTimestamp"));
    objectPosition = getObjectValue<ObjectRegion>(json, "objectPosition");
    analyticsEngineId = getStringValue(json, "analyticsEngineId");

    title = getOptionalObjectValue<Title>(json, "title");
    bestShot = getOptionalObjectValue<BestShot>(json, "bestShot");
}

bool ObjectTrack::operator==(const ObjectTrack& other) const
{
    return other.analyticsEngineId == analyticsEngineId && other.attributes == attributes
        && other.bestShot == bestShot
        && other.title == title
        && other.deviceId == deviceId
        && other.firstAppearanceTimestamp == firstAppearanceTimestamp && other.id == id
        && other.lastAppearanceTimestamp == lastAppearanceTimestamp
        && other.objectPosition == objectPosition && other.objectTypeId == objectTypeId;
}

ValueOrError<ObjectTrack> ObjectTrack::fromJson(const char* jsonStr) noexcept
{
    try
    {
        return ValueOrError<ObjectTrack>::makeValue(ObjectTrack(jsonStr));
    }
    catch (const std::exception& e)
    {
        return ValueOrError<ObjectTrack>::makeError(e.what());
    }
}

ValueOrError<ObjectTrack> ObjectTrack::fromJson(const nx::kit::Json& json) noexcept
{
    try
    {
        return ValueOrError<ObjectTrack>::makeValue(ObjectTrack(json));
    }
    catch (const std::exception& e)
    {
        return ValueOrError<ObjectTrack>::makeError(e.what());
    }
}

nx::kit::Json ObjectTrack::to_json() const
{
    nx::kit::Json::object result =
    nx::kit::Json::object({
        {"id", id},
        {"deviceId", deviceId},
        {"objectTypeId", objectTypeId},
        {"attributes", attributes},
        {"firstAppearanceTimestamp", (double) firstAppearanceTimestamp.count()},
        {"lastAppearanceTimestamp", (double) lastAppearanceTimestamp.count()},
        {"objectPosition", objectPosition},
        {"analyticsEngineId", analyticsEngineId},
        });

    if (bestShot)
        result["bestShot"] = nx::kit::Json(*bestShot);
    if (title)
        result["title"] = nx::kit::Json(*title);

    return result;
}

Image::Image(const nx::kit::Json& json)
{
    objectTrackId = getStringValue(json, "objectTrackId");
    format = getStringValue(json, "format");
    data64 = getStringValue(json, "data64");
}

Image::Image(const char* jsonData): Image(parseJson(jsonData))
{
}

nx::kit::Json Image::to_json() const
{
    return nx::kit::Json::object({
        {"objectTrackId", objectTrackId},
        {"format", format},
        {"data64", data64}
    });
}

ValueOrError<Image> Image::fromJson(const char* jsonStr) noexcept
{
    try
    {
        return ValueOrError<Image>::makeValue(Image(jsonStr));
    }
    catch (const std::exception& e)
    {
        return ValueOrError<Image>::makeError(e.what());
    }
}

ValueOrError<Image> Image::fromJson(const nx::kit::Json& json) noexcept
{
    try
    {
        return ValueOrError<Image>::makeValue(Image(json));
    }
    catch (const std::exception& e)
    {
        return ValueOrError<Image>::makeError(e.what());
    }
}

bool Image::operator==(const Image& other) const
{
    return objectTrackId == other.objectTrackId
        && format == other.format
        && data64 == other.data64;
}

AnalyticsLookupResult analyticsLookupResultFromJson(const char* data)
{
    return objectListFromJson<ObjectTrack>(data);
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
