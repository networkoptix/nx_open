#include "settings.h"

#include <cstdlib>
#include <charconv>
#include <string>
#include <sstream>
#include <optional>

#include <nx/utils/log/log.h>

using namespace std::literals;

namespace nx::vms_server_plugins::analytics::hanwha {

//-------------------------------------------------------------------------------------------------

/**
 * Parse the sunapi reply from camera and extract (as a json object) information about the desired
 * event on the desired channel. If nothing found, empty json object returned.
 */
nx::kit::Json getChannelInfoOrThrow(
    const std::string& cameraReply, const char* eventName, int channelNumber)
{
    std::string err;
    nx::kit::Json json = nx::kit::Json::parse(cameraReply, err);
    if (!json.is_object())
        throw DeviceValueError{};

    const nx::kit::Json& jsonChannels = json[eventName];
    if (!jsonChannels.is_array())
        throw DeviceValueError{};

    for (const auto& channel: jsonChannels.array_items())
    {
        if (const auto& j = channel["Channel"]; j.is_number() && j.int_value() == channelNumber)
            return channel;
    }
    throw DeviceValueError{};
}

/**
 * Extract information about min and max object size (as a json object) of a desired type from
 * the json object (that corresponds to some event and channel)
 */
nx::kit::Json getObjectSizeInfo(const nx::kit::Json& channelInfo,
    const std::string& detectionTypeValue)
{
    nx::kit::Json result;
    const nx::kit::Json& objectSizeList = channelInfo["ObjectSizeByDetectionTypes"];
    if (!objectSizeList.is_array())
        return result;

    for (const nx::kit::Json& objectSize : objectSizeList.array_items())
    {
        const nx::kit::Json& detectionTypeParameter = objectSize["DetectionType"];
        if (detectionTypeParameter.string_value() == detectionTypeValue)
        {
            result = objectSize;
            return result;
        }
    }
    return result;
}

// TODO: Unite getMdRoiInfo, getIvaLineInfo, getIvaRoiInfo, getOdRoiInfo into one function

/**
 * Extract information about motion detection ROI (as a json object) of a desired type from
 * the json object (that corresponds to some event and channel)
 */
nx::kit::Json getMdRoiInfo(nx::kit::Json channelInfo, int sunapiIndex)
{
    nx::kit::Json result;
    const nx::kit::Json& Lines = channelInfo["ROIs"];
    if (!Lines.is_array())
        return result;

    for (const nx::kit::Json& Line : Lines.array_items())
    {
        const nx::kit::Json& roiIndex = Line["ROI"];
        if (roiIndex.is_number() && roiIndex.int_value() == sunapiIndex)
        {
            result = Line;
            return result;
        }
    }
    return result;;
}

/**
 * Extract information about IVA line (as a json object) of a desired type from
 * the json object (that corresponds to some event and channel)
 */
nx::kit::Json getIvaLineInfo(nx::kit::Json channelInfo, int sunapiIndex)
{
    nx::kit::Json result;
    const nx::kit::Json& Lines = channelInfo["Lines"];
    if (!Lines.is_array())
        return result;

    for (const nx::kit::Json& Line : Lines.array_items())
    {
        const nx::kit::Json& roiIndex = Line["Line"];
        if (roiIndex.is_number() && roiIndex.int_value() == sunapiIndex)
        {
            result = Line;
            return result;
        }
    }
    return result;;
}

/**
 * Extract information about IVA ROI (as a json object) of a desired type from
 * the json object (that corresponds to some event and channel)
 */
nx::kit::Json getIvaRoiInfo(nx::kit::Json channelInfo, int sunapiIndex)
{
    nx::kit::Json result;
    const nx::kit::Json& Lines = channelInfo["DefinedAreas"];
    if (!Lines.is_array())
        return result;

    for (const nx::kit::Json& Line : Lines.array_items())
    {
        const nx::kit::Json& roiIndex = Line["DefinedArea"];
        if (roiIndex.is_number() && roiIndex.int_value() == sunapiIndex)
        {
            result = Line;
            return result;
        }
    }
    return result;;
}

/**
 * Extract information about object detection ROI (as a json object) of a desired type from
 * the json object (that corresponds to some event and channel)
 */
nx::kit::Json getOdRoiInfo(nx::kit::Json channelInfo, int sunapiIndex)
{
    nx::kit::Json result;
    const nx::kit::Json& Areas = channelInfo["ExcludeAreas"];
    if (!Areas.is_array())
        return result;

    for (const nx::kit::Json& Area : Areas.array_items())
    {
        const nx::kit::Json& roiIndex = Area["ExcludeArea"];
        if (roiIndex.is_number() && roiIndex.int_value() == sunapiIndex)
        {
            result = Area;
            return result;
        }
    }
    return result;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

std::string SettingGroup::serverKey(int keyIndex) const
{
    NX_ASSERT(keyIndex < serverKeyCount);

    std::string result(serverKeys[keyIndex]);
    if (serverIndex() >= 0)
        result.replace(result.find("#"), 1, std::to_string(serverIndex()));

    return result;
}

void SettingGroup::replanishErrorMap(
    nx::sdk::Ptr<nx::sdk::StringMap>& errorMap, const std::string& reason) const
{
    for ( int i = 0; i < serverKeyCount; ++i)
        errorMap->setItem(serverKeys[i], reason);
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

void serverReadOrThrow(const char* source, bool* destination)
{
    if(!source)
        throw ServerValueError();

    if (strcmp(source, "true") == 0)
    {
        *destination = true;
        return;
    }
    if (strcmp(source, "false") == 0)
    {
        *destination = false;
        return;
    }

    throw ServerValueError();
}

std::string serverWrite(bool value)
{
    if (value)
        return "true"s;
    else
        return "false"s;
}

void serverReadOrThrow(const char* source, int* destination)
{
    if (!source)
        throw ServerValueError();

    const char* end = source + strlen(source);
    std::from_chars_result conversionResult = std::from_chars(source, end, *destination);
    if (conversionResult.ptr == end)
        return;

    throw ServerValueError();
}

std::string serverWrite(int value)
{
    return std::to_string(value);
}

void serverReadOrThrow(const char* source, std::string* destination)
{
    if (!source)
        throw ServerValueError();

    *destination = source;
    if (destination->size() >= 2 && destination->front() == '"' && destination->back() == '"')
    {
        *destination = destination->substr(1, destination->size() - 2);
        return;
    }

    throw ServerValueError();
}

std::string serverWrite(std::string value)
{
    return "\"" + value + "\"";
}

void serverReadOrThrow(const char* source, std::vector<PluginPoint>* destination)
{
    if (!source)
        throw ServerValueError();

    std::optional<std::vector<PluginPoint>> tmp = ServerStringToPluginPoints(source);
    if (tmp)
    {
        *destination = *tmp;
        return;
    }

    throw ServerValueError();
}

void serverReadOrThrow(const char* source, Direction* destination)
{
    if (!source)
        throw ServerValueError();

    std::optional<Direction> tmp = ServerStringToDirection(source);
    if (tmp)
    {
        *destination = *tmp;
        return;
    }

    throw ServerValueError();
}

void serverReadOrThrow(const char* source, Width* destination)
{
    if (!source)
        throw ServerValueError();

    std::optional<Width> tmp = ServerStringToWidth(source);
    if (tmp)
    {
        *destination = *tmp;
        return;
    }

    throw ServerValueError();
}

std::string serverWrite(std::pair<Width, Height> pair)
{
    std::optional<std::vector<PluginPoint>> points = WidthHeightToPluginPoints(
        pair.first, pair.second);
    std::string result = pluginPointsToServerJson(*points);
    return result;
}

void serverReadOrThrow(const char* source, Height* destination)
{
    if (!source)
        throw ServerValueError();

    std::optional<Height> tmp = ServerStringToHeight(source);
    if (tmp)
    {
        *destination = *tmp;
        return;
    }

    throw ServerValueError();
}
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void sunapiReadOrThrow(const nx::kit::Json& json, const char* key, FrameSize /*frameSize*/, bool* result)
{
    NX_ASSERT(key);

    if (const auto& param = json[key]; param.is_bool())
        *result = param.bool_value();
    else
        throw DeviceValueError{};
}

void sunapiReadOrThrow(const nx::kit::Json& json, const char* key, FrameSize /*frameSize*/, int* result)
{
    NX_ASSERT(key);

    if (const auto& param = json[key]; param.is_number())
        *result = param.int_value();
    else
        throw DeviceValueError{};
}

void sunapiReadOrThrow(const nx::kit::Json& json, const char* key, FrameSize /*frameSize*/, std::string* result)
{
    NX_ASSERT(key);

    if (const auto& param = json[key]; param.is_string())
        *result = param.string_value();
    else
        throw DeviceValueError{};
}

void sunapiReadOrThrow(const nx::kit::Json& json, const char* key, FrameSize frameSize, PluginPoint* result)
{
    NX_ASSERT(key);

    const auto& param = json[key];
    if (!param.is_string())
        throw DeviceValueError{};

    const std::string value = param.string_value();

    if (!result->fromSunapiString(value, frameSize))
        throw DeviceValueError();
}

void sunapiReadOrThrow(const nx::kit::Json& json, const char* key, FrameSize frameSize, Width* result)
{
    PluginPoint point;
    sunapiReadOrThrow(json, key, frameSize, &point);
    *result = point.x;
}

void sunapiReadOrThrow(const nx::kit::Json& json, const char* key, FrameSize frameSize, Height* result)
{
    PluginPoint point;
    sunapiReadOrThrow(json, key, frameSize, &point);
    *result = point.y;
}

void sunapiReadOrThrow(const nx::kit::Json& json, const char* key, FrameSize frameSize, std::vector<PluginPoint>* result)
{
    NX_ASSERT(key);

    const auto& points = json[key];
    if (!points.is_array())
        throw DeviceValueError{};

    result->reserve(points.array_items().size());
    for (const nx::kit::Json& point: points.array_items())
    {
        if (!point["x"].is_number() || !point["y"].is_number())
            throw DeviceValueError{};

        const int ix = point["x"].int_value();
        const int iy = point["y"].int_value();
        result->emplace_back(frameSize.xAbsoluteToRelative(ix), frameSize.yAbsoluteToRelative(iy));
    }
}

void sunapiReadOrThrow(const nx::kit::Json& json, const char* key, FrameSize frameSize, Direction* result)
{
    NX_ASSERT(key);

    if (const auto& param = json[key]; param.is_string())
    {
        if (param.string_value() == "RightSide")
            *result = Direction::Right;
        else if (param.string_value() == "LeftSide")
            *result = Direction::Left;
        else if (param.string_value() == "BothDirections")
            *result = Direction::Both;
        else if (param.string_value() != "Off")
            throw DeviceValueError{}; //< unknown direction
    }
    else
        throw DeviceValueError{};
}

void sunapiReadOrThrow(const nx::kit::Json& json, const char* key, FrameSize /*frameSize*/, bool* result, const char* desired)
{
    NX_ASSERT(key);

    if (const auto& param = json[key]; param.is_array())
    {
        *result = false;
        for (const nx::kit::Json& item : param.array_items())
        {
            if (item.string_value() == desired)
            {
                *result = true;
                break;
            }
        }
    }
    else
        throw DeviceValueError{};
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

std::string buildBool(bool value)
{
    if (value)
        return "True";
    else
        return "False";
}

std::string concat(std::vector<const char*> items, char c = ',')
{
    std::string result;
    constexpr size_t kMaxExpectedSize = 64;
    result.reserve(kMaxExpectedSize);
    if (items.size())
        result.append(items[0]);
    for (size_t i = 1; i < items.size(); ++i)
    {
        result.push_back(c);
        result.append(items[i]);
    }
    return result;
}

//-------------------------------------------------------------------------------------------------

#define NX_READ_FROM_SERVER_OR_THROW(SOURCE_MAP, FIELD_NAME) serverReadOrThrow( \
    SOURCE_MAP->value(serverKey((int)ServerParamIndex::FIELD_NAME).c_str()), \
    &FIELD_NAME)

#define NX_WRITE_TO_SERVER(DESTINATION_MAP, FIELD_NAME) \
    DESTINATION_MAP->setValue(serverKey((int)ServerParamIndex::FIELD_NAME).c_str(), \
    serverWrite(FIELD_NAME));

//-------------------------------------------------------------------------------------------------

bool ShockDetection::operator==(const ShockDetection& rhs) const
{
    return initialized == rhs.initialized
        && enabled == rhs.enabled
        && thresholdLevel == rhs.thresholdLevel
        && sensitivityLevel == rhs.sensitivityLevel
        ;
}

void ShockDetection::readFromServerOrThrow(
    const nx::sdk::IStringMap* settingsSource, int /*roiIndex*/)
{
    NX_READ_FROM_SERVER_OR_THROW(settingsSource, enabled);
    NX_READ_FROM_SERVER_OR_THROW(settingsSource, thresholdLevel);
    NX_READ_FROM_SERVER_OR_THROW(settingsSource, sensitivityLevel);
    initialized = true;
}

void ShockDetection::writeToServer(
    nx::sdk::SettingsResponse* settingsDestination, int /*roiIndex*/) const
{
    NX_WRITE_TO_SERVER(settingsDestination, enabled);
    NX_WRITE_TO_SERVER(settingsDestination, thresholdLevel);
    NX_WRITE_TO_SERVER(settingsDestination, sensitivityLevel);
}

void ShockDetection::readFromCameraOrThrow(const nx::kit::Json& channelInfo, FrameSize frameSize)
{
    sunapiReadOrThrow(channelInfo, "Enable", frameSize, &enabled);
    sunapiReadOrThrow(channelInfo, "ThresholdLevel", frameSize, &thresholdLevel);
    sunapiReadOrThrow(channelInfo, "Sensitivity", frameSize, &sensitivityLevel);
    initialized = true;
}

std::string ShockDetection::buildCameraWritingQuery(FrameSize /*frameSize*/, int channelNumber) const
{
    std::ostringstream query;
    if (initialized)
    {
        query
            << "msubmenu=" << "shockdetection"
            << "&action=" << "set"
            << "&Channel=" << channelNumber
            << "&Enable=" << buildBool(enabled)
            << "&ThresholdLevel=" << thresholdLevel
            << "&Sensitivity=" << sensitivityLevel
            ;
    }
    return query.str();
}

//-------------------------------------------------------------------------------------------------

bool Motion::operator==(const Motion& rhs) const
{
    return initialized == rhs.initialized
        && detectionType == rhs.detectionType
        ;
}

void Motion::readFromServerOrThrow(const nx::sdk::IStringMap* settingsSource, int /*roiIndex*/)
{
    NX_READ_FROM_SERVER_OR_THROW(settingsSource, detectionType);
    initialized = true;
}

void Motion::writeToServer(nx::sdk::SettingsResponse* settingsDestination, int /*roiIndex*/) const
{
    NX_WRITE_TO_SERVER(settingsDestination, detectionType);
}

void Motion::readFromCameraOrThrow(const nx::kit::Json& channelInfo, FrameSize frameSize)
{
    sunapiReadOrThrow(channelInfo, "DetectionType", frameSize, &detectionType);
    initialized = true;
}

std::string Motion::buildCameraWritingQuery(FrameSize /*frameSize*/, int channelNumber) const
{
    std::ostringstream query;
    if (initialized)
    {
        query
            << "msubmenu=" << "videoanalysis2"
            << "&action=" << "set"
            << "&Channel=" << channelNumber
            << "&DetectionType=" << detectionType
            ;
    }
    return query.str();
}

//-------------------------------------------------------------------------------------------------

bool MotionDetectionObjectSize::operator==(const MotionDetectionObjectSize& rhs) const
{
    return initialized == rhs.initialized
        && minWidth == rhs.minWidth
        && minHeight == rhs.minHeight
        && maxWidth == rhs.maxWidth
        && maxHeight == rhs.maxHeight
        ;
}

void MotionDetectionObjectSize::readFromServerOrThrow(const nx::sdk::IStringMap* settingsSource, int /*roiIndex*/)
{
    NX_READ_FROM_SERVER_OR_THROW(settingsSource, minWidth);
    NX_READ_FROM_SERVER_OR_THROW(settingsSource, minHeight);
    NX_READ_FROM_SERVER_OR_THROW(settingsSource, maxWidth);
    NX_READ_FROM_SERVER_OR_THROW(settingsSource, maxHeight);
    initialized = true;
}

void MotionDetectionObjectSize::writeToServer(
    nx::sdk::SettingsResponse* settingsDestination, int /*roiIndex*/) const
{
    settingsDestination->setValue("MotionDetection.MinObjectSize.Points",
        serverWrite(std::pair(minWidth, minHeight)));

}

void MotionDetectionObjectSize::readFromCameraOrThrow(const nx::kit::Json& channelInfo, FrameSize frameSize)
{
    nx::kit::Json objectSizeInfo = getObjectSizeInfo(channelInfo, "MotionDetection");
    sunapiReadOrThrow(objectSizeInfo, "MinimumObjectSizeInPixels", frameSize, &minWidth);
    sunapiReadOrThrow(objectSizeInfo, "MinimumObjectSizeInPixels", frameSize, &minHeight);
    sunapiReadOrThrow(objectSizeInfo, "MaximumObjectSizeInPixels", frameSize, &maxWidth);
    sunapiReadOrThrow(objectSizeInfo, "MaximumObjectSizeInPixels", frameSize, &maxHeight);
    initialized = true;
}

std::string MotionDetectionObjectSize::buildMinObjectSize(FrameSize frameSize) const
{
    std::stringstream stream;
    stream
        << frameSize.xRelativeToAbsolute(minWidth)
        << ','
        << frameSize.yRelativeToAbsolute(minHeight);
    return stream.str();
}

std::string MotionDetectionObjectSize::buildMaxObjectSize(FrameSize frameSize) const
{
    std::stringstream stream;
    stream
        << frameSize.xRelativeToAbsolute(maxWidth)
        << ','
        << frameSize.yRelativeToAbsolute(maxHeight);
    return stream.str();
}

std::string MotionDetectionObjectSize::buildCameraWritingQuery(FrameSize frameSize, int channelNumber) const
{
    std::ostringstream query;
    if (initialized)
    {
        query
            << "msubmenu=" << "videoanalysis2"
            << "&action=" << "set"
            << "&Channel=" << channelNumber
            << "&DetectionType.MotionDetection.MinimumObjectSizeInPixels="
                << buildMinObjectSize(frameSize)
            << "&DetectionType.MotionDetection.MaximumObjectSizeInPixels="
                << buildMaxObjectSize(frameSize)
            ;
    }
    return query.str();
}

//-------------------------------------------------------------------------------------------------

bool IvaObjectSize::operator==(const IvaObjectSize& rhs) const
{
    return initialized == rhs.initialized
        && minWidth == rhs.minWidth
        && minHeight == rhs.minHeight
        && maxWidth == rhs.maxWidth
        && maxHeight == rhs.maxHeight
        ;
}

void IvaObjectSize::readFromServerOrThrow(const nx::sdk::IStringMap* settingsSource, int /*roiIndex*/)
{
    NX_READ_FROM_SERVER_OR_THROW(settingsSource, minWidth);
    NX_READ_FROM_SERVER_OR_THROW(settingsSource, minHeight);
    NX_READ_FROM_SERVER_OR_THROW(settingsSource, maxWidth);
    NX_READ_FROM_SERVER_OR_THROW(settingsSource, maxHeight);
    initialized = true;
}

void IvaObjectSize::readFromCameraOrThrow(const nx::kit::Json& channelInfo, FrameSize frameSize)
{
    nx::kit::Json objectSizeInfo = getObjectSizeInfo(channelInfo, "IntelligentVideo");
    sunapiReadOrThrow(objectSizeInfo, "MinimumObjectSizeInPixels", frameSize, &minWidth);
    sunapiReadOrThrow(objectSizeInfo, "MinimumObjectSizeInPixels", frameSize, &minHeight);
    sunapiReadOrThrow(objectSizeInfo, "MaximumObjectSizeInPixels", frameSize, &maxWidth);
    sunapiReadOrThrow(objectSizeInfo, "MaximumObjectSizeInPixels", frameSize, &maxHeight);
    initialized = true;
}

std::string IvaObjectSize::buildMinObjectSize(FrameSize frameSize) const
{
    std::stringstream stream;
    stream
        << frameSize.xRelativeToAbsolute(minWidth)
        << ','
        << frameSize.yRelativeToAbsolute(minHeight);
    return stream.str();
}

std::string IvaObjectSize::buildMaxObjectSize(FrameSize frameSize) const
{
    std::stringstream stream;
    stream
        << frameSize.xRelativeToAbsolute(maxWidth)
        << ','
        << frameSize.yRelativeToAbsolute(maxHeight);
    return stream.str();
}

std::string IvaObjectSize::buildCameraWritingQuery(FrameSize frameSize, int channelNumber) const
{
    std::ostringstream query;
    if (initialized)
    {
        query
            << "msubmenu=" << "videoanalysis2"
            << "&action=" << "set"
            << "&Channel=" << channelNumber
            << "&DetectionType.IntelligentVideo.MinimumObjectSizeInPixels="
                << buildMinObjectSize(frameSize)
            << "&DetectionType.IntelligentVideo.MaximumObjectSizeInPixels="
                << buildMaxObjectSize(frameSize)
            ;
    }
    return query.str();
}

//-------------------------------------------------------------------------------------------------

bool MotionDetectionIncludeArea::operator==(const MotionDetectionIncludeArea& rhs) const
{
    return initialized == rhs.initialized
        && points == rhs.points
        && thresholdLevel == rhs.thresholdLevel
        && sensitivityLevel == rhs.sensitivityLevel
        && minimumDuration == rhs.minimumDuration
        ;
}

void MotionDetectionIncludeArea::readFromServerOrThrow(const nx::sdk::IStringMap* settingsSource, int roiIndex)
{
    NX_READ_FROM_SERVER_OR_THROW(settingsSource, points);
    NX_READ_FROM_SERVER_OR_THROW(settingsSource, thresholdLevel);
    NX_READ_FROM_SERVER_OR_THROW(settingsSource, sensitivityLevel);
    NX_READ_FROM_SERVER_OR_THROW(settingsSource, minimumDuration);
    initialized = true;
}

void MotionDetectionIncludeArea::readFromCameraOrThrow(const nx::kit::Json& channelInfo, FrameSize frameSize)
{
        nx::kit::Json roiInfo = getMdRoiInfo(channelInfo, this->deviceIndex());
        if (roiInfo == nx::kit::Json())
        {
            // No roi info found for current channel => *this should be set into default state.
            // Current SettingGroup is considered to be uninitialized.
            *this = MotionDetectionIncludeArea(this->nativeIndex());
            return;
        }

        sunapiReadOrThrow(roiInfo, "Coordinates", frameSize, &points);
        sunapiReadOrThrow(roiInfo, "ThresholdLevel", frameSize, &thresholdLevel);
        sunapiReadOrThrow(roiInfo, "SensitivityLevel", frameSize, &sensitivityLevel);
        sunapiReadOrThrow(roiInfo, "Duration", frameSize, &minimumDuration);
        initialized = true;
}

std::string MotionDetectionIncludeArea::buildCameraWritingQuery(FrameSize frameSize, int channelNumber) const
{
    std::ostringstream query;
    if (initialized)
    {
        if (!points.empty())
        {
            const std::string prefix = "&ROI."s + std::to_string(deviceIndex());
            query
                << "msubmenu=" << "videoanalysis2"
                << "&action=" << "set"
                << "&Channel=" << channelNumber
                << prefix << ".Coordinate=" << pluginPointsToSunapiString(points, frameSize)
                << prefix << ".ThresholdLevel=" << thresholdLevel
                << prefix << ".SensitivityLevel=" << sensitivityLevel
                << prefix << ".Duration=" << minimumDuration
                ;
        }
        else
        {
            query
                << "msubmenu=" << "videoanalysis2"
                << "&action=" << "remove"
                << "&Channel=" << channelNumber
                << "&ROIIndex=" << deviceIndex();
        }
    }
    return query.str();

}
//-------------------------------------------------------------------------------------------------
bool MotionDetectionExcludeArea::operator==(const MotionDetectionExcludeArea& rhs) const
{
    return initialized == rhs.initialized
        && points == rhs.points
        ;
}

void MotionDetectionExcludeArea::readFromServerOrThrow(const nx::sdk::IStringMap* settingsSource, int roiIndex)
{
    NX_READ_FROM_SERVER_OR_THROW(settingsSource, points);
    initialized = true;
}

void MotionDetectionExcludeArea::readFromCameraOrThrow(const nx::kit::Json& channelInfo, FrameSize frameSize)
{
    nx::kit::Json roiInfo = getMdRoiInfo(channelInfo, this->deviceIndex());
    if (roiInfo == nx::kit::Json(this->deviceIndex()))
    {
        *this = MotionDetectionExcludeArea(this->nativeIndex());
        return;
    }
    sunapiReadOrThrow(roiInfo, "Coordinates", frameSize, &points);
    initialized = true;
}

std::string MotionDetectionExcludeArea::buildCameraWritingQuery(FrameSize frameSize, int channelNumber) const
{
    std::ostringstream query;
    if (initialized)
    {
        if (!points.empty())
        {
            const std::string prefix = "&ROI."s + std::to_string(deviceIndex());
            query
                << "msubmenu=" << "videoanalysis2"
                << "&action=" << "set"
                << "&Channel=" << channelNumber
                << prefix << ".Coordinate=" << pluginPointsToSunapiString(points, frameSize)
                ;
        }
        else
        {
            query
                << "msubmenu=" << "videoanalysis2"
                << "&action=" << "remove"
                << "&Channel=" << channelNumber
                << "&ROIIndex=" << deviceIndex()
                ;
        }
    }
    return query.str();
}

//-------------------------------------------------------------------------------------------------

bool TamperingDetection::operator==(const TamperingDetection& rhs) const
{
    return initialized == rhs.initialized
        && enabled == rhs.enabled
        && thresholdLevel == rhs.thresholdLevel
        && sensitivityLevel == rhs.sensitivityLevel
        && minimumDuration == rhs.minimumDuration
        && exceptDarkImages == rhs.exceptDarkImages
        ;
}

void TamperingDetection::readFromServerOrThrow(const nx::sdk::IStringMap* settingsSource, int roiIndex)
{
    NX_READ_FROM_SERVER_OR_THROW(settingsSource, enabled);
    NX_READ_FROM_SERVER_OR_THROW(settingsSource, thresholdLevel);
    NX_READ_FROM_SERVER_OR_THROW(settingsSource, sensitivityLevel);
    NX_READ_FROM_SERVER_OR_THROW(settingsSource, minimumDuration);
    NX_READ_FROM_SERVER_OR_THROW(settingsSource, exceptDarkImages);
    initialized = true;
}

void TamperingDetection::readFromCameraOrThrow(const nx::kit::Json& channelInfo, FrameSize frameSize)
{
    sunapiReadOrThrow(channelInfo, "Enable", frameSize, &enabled);
    sunapiReadOrThrow(channelInfo, "ThresholdLevel", frameSize, &thresholdLevel);
    sunapiReadOrThrow(channelInfo, "SensitivityLevel", frameSize, &sensitivityLevel);
    sunapiReadOrThrow(channelInfo, "Duration", frameSize, &minimumDuration);
    sunapiReadOrThrow(channelInfo, "DarknessDetection", frameSize, &exceptDarkImages);
    initialized = true;
}

std::string TamperingDetection::buildCameraWritingQuery(FrameSize /*frameSize*/, int channelNumber) const
{
    std::ostringstream query;
    if (initialized)
    {
        query
            << "msubmenu=" << "tamperingdetection"
            << "&action=" << "set"
            << "&Channel=" << channelNumber
            << "&Enable=" << buildBool(enabled)
            << "&ThresholdLevel=" << thresholdLevel
            << "&SensitivityLevel=" << sensitivityLevel
            << "&Duration=" << minimumDuration
            << "&DarknessDetection=" << buildBool(exceptDarkImages)
            ;
    }
    return query.str();
}

//-------------------------------------------------------------------------------------------------
bool DefocusDetection::operator==(const DefocusDetection& rhs) const
{
    return initialized == rhs.initialized
        && enabled == rhs.enabled
        && thresholdLevel == rhs.thresholdLevel
        && sensitivityLevel == rhs.sensitivityLevel
        && minimumDuration == rhs.minimumDuration
        ;
}

void DefocusDetection::readFromServerOrThrow(const nx::sdk::IStringMap* settingsSource, int /*roiIndex*/)
{
    NX_READ_FROM_SERVER_OR_THROW(settingsSource, enabled);
    NX_READ_FROM_SERVER_OR_THROW(settingsSource, thresholdLevel);
    NX_READ_FROM_SERVER_OR_THROW(settingsSource, sensitivityLevel);
    NX_READ_FROM_SERVER_OR_THROW(settingsSource, minimumDuration);
    initialized = true;
}

void DefocusDetection::readFromCameraOrThrow(const nx::kit::Json& channelInfo, FrameSize frameSize)
{
    sunapiReadOrThrow(channelInfo, "Enable", frameSize, &enabled);
    sunapiReadOrThrow(channelInfo, "ThresholdLevel", frameSize, &thresholdLevel);
    sunapiReadOrThrow(channelInfo, "Sensitivity", frameSize, &sensitivityLevel);
    sunapiReadOrThrow(channelInfo, "Duration", frameSize, &minimumDuration);
    initialized = true;
}

std::string DefocusDetection::buildCameraWritingQuery(FrameSize /*frameSize*/, int channelNumber) const
{
    std::ostringstream query;
    if (initialized)
    {
        query
            << "msubmenu=" << "defocusdetection"
            << "&action=" << "set"
            << "&Channel=" << channelNumber
            << "&Enable=" << buildBool(enabled)
            << "&ThresholdLevel=" << thresholdLevel
            << "&Sensitivity=" << sensitivityLevel
            << "&Duration=" << minimumDuration
            ;
    }
    return query.str();
}

//-------------------------------------------------------------------------------------------------
bool FogDetection::operator==(const FogDetection& rhs) const
{
    return initialized == rhs.initialized
        && enabled == rhs.enabled
        && thresholdLevel == rhs.thresholdLevel
        && sensitivityLevel == rhs.sensitivityLevel
        && minimumDuration == rhs.minimumDuration
        ;
}

void FogDetection::readFromServerOrThrow(const nx::sdk::IStringMap* settingsSource, int /*roiIndex*/)
{
    NX_READ_FROM_SERVER_OR_THROW(settingsSource, enabled);
    NX_READ_FROM_SERVER_OR_THROW(settingsSource, thresholdLevel);
    NX_READ_FROM_SERVER_OR_THROW(settingsSource, sensitivityLevel);
    NX_READ_FROM_SERVER_OR_THROW(settingsSource, minimumDuration);
    initialized = true;
}

void FogDetection::readFromCameraOrThrow(const nx::kit::Json& channelInfo, FrameSize frameSize)
{
    sunapiReadOrThrow(channelInfo, "Enable", frameSize, &enabled);
    sunapiReadOrThrow(channelInfo, "ThresholdLevel", frameSize, &thresholdLevel);
    sunapiReadOrThrow(channelInfo, "Sensitivity", frameSize, &sensitivityLevel);
    sunapiReadOrThrow(channelInfo, "Duration", frameSize, &minimumDuration);
    initialized = true;
}

std::string FogDetection::buildCameraWritingQuery(FrameSize /*frameSize*/, int channelNumber) const
{
    std::ostringstream query;
    if (initialized)
    {
        query
            << "msubmenu=" << "fogdetection"
            << "&action=" << "set"
            << "&Channel=" << channelNumber
            << "&Enable=" << buildBool(enabled)
            << "&ThresholdLevel=" << thresholdLevel
            << "&Sensitivity=" << sensitivityLevel
            << "&Duration=" << minimumDuration
            ;
    }
    return query.str();
}

//-------------------------------------------------------------------------------------------------

bool FaceDetectionGeneral::operator==(const FaceDetectionGeneral& rhs) const
{
    return initialized == rhs.initialized
        && enabled == rhs.enabled
        && sensitivityLevel == rhs.sensitivityLevel
        ;
}

void FaceDetectionGeneral::readFromServerOrThrow(const nx::sdk::IStringMap* settingsSource, int /*roiIndex*/)
{
    NX_READ_FROM_SERVER_OR_THROW(settingsSource, enabled);
    NX_READ_FROM_SERVER_OR_THROW(settingsSource, sensitivityLevel);
    initialized = true;
}

void FaceDetectionGeneral::readFromCameraOrThrow(const nx::kit::Json& channelInfo, FrameSize frameSize)
{
    sunapiReadOrThrow(channelInfo, "Enable", frameSize, &enabled);
    sunapiReadOrThrow(channelInfo, "Sensitivity", frameSize, &sensitivityLevel);
    initialized = true;
}

std::string FaceDetectionGeneral::buildCameraWritingQuery(FrameSize /*frameSize*/, int channelNumber) const
{
    std::ostringstream query;
    if (initialized)
    {
        query
            << "msubmenu=" << "facedetection"
            << "&action=" << "set"
            << "&Channel=" << channelNumber
            << "&Enable=" << buildBool(enabled)
            << "&Sensitivity=" << sensitivityLevel
            ;
    }
    return query.str();
}

//-------------------------------------------------------------------------------------------------

bool ObjectDetectionGeneral::operator==(const ObjectDetectionGeneral& rhs) const
{
    return initialized == rhs.initialized
        && enabled == rhs.enabled
        && person == rhs.person
        && vehicle == rhs.vehicle
        && face == rhs.face
        && licensePlate == rhs.licensePlate
        && minimumDuration == rhs.minimumDuration
        ;
}

void ObjectDetectionGeneral::readFromServerOrThrow(const nx::sdk::IStringMap* settingsSource, int /*roiIndex*/)
{
    NX_READ_FROM_SERVER_OR_THROW(settingsSource, enabled);
    NX_READ_FROM_SERVER_OR_THROW(settingsSource, person);
    NX_READ_FROM_SERVER_OR_THROW(settingsSource, vehicle);
    NX_READ_FROM_SERVER_OR_THROW(settingsSource, face);
    NX_READ_FROM_SERVER_OR_THROW(settingsSource, licensePlate);
    NX_READ_FROM_SERVER_OR_THROW(settingsSource, minimumDuration);
    initialized = true;
}

void ObjectDetectionGeneral::readFromCameraOrThrow(const nx::kit::Json& channelInfo, FrameSize frameSize)
{
    sunapiReadOrThrow(channelInfo, "Enable", frameSize, &enabled);
    sunapiReadOrThrow(channelInfo, "ObjectTypes", frameSize, &person, "Person");
    sunapiReadOrThrow(channelInfo, "ObjectTypes", frameSize, &vehicle, "Vehicle");
    sunapiReadOrThrow(channelInfo, "ObjectTypes", frameSize, &face, "Face");
    sunapiReadOrThrow(channelInfo, "ObjectTypes", frameSize, &licensePlate, "LicensePlate");
    sunapiReadOrThrow(channelInfo, "Duration", frameSize, &minimumDuration);
    initialized = true;
}

std::string ObjectDetectionGeneral::buildObjectTypes() const
{
    std::vector<const char*> mode;

    if (person)
        mode.push_back("Person");

    if (vehicle)
        mode.push_back("Vehicle");

    if (face)
        mode.push_back("Face");

    if (licensePlate)
        mode.push_back("LicensePlate");

    return concat(mode);
}

std::string ObjectDetectionGeneral::buildCameraWritingQuery(FrameSize /*frameSize*/, int channelNumber) const
{
    std::ostringstream query;
    if (initialized)
    {
        query
            << "msubmenu=" << "objectdetection"
            << "&action=" << "set"
            << "&Channel=" << channelNumber
            << "&Enable=" << buildBool(enabled)
            << "&ObjectTypes=" << buildObjectTypes()
            << "&Duration=" << minimumDuration
            ;
    }
    return query.str();
}

//-------------------------------------------------------------------------------------------------

bool ObjectDetectionBestShot::operator==(const ObjectDetectionBestShot& rhs) const
{
    return initialized == rhs.initialized
        && person == rhs.person
        && vehicle == rhs.vehicle
        && face == rhs.face
        && licensePlate == rhs.licensePlate
        ;
}

void ObjectDetectionBestShot::readFromServerOrThrow(const nx::sdk::IStringMap* settingsSource, int /*roiIndex*/)
{
    NX_READ_FROM_SERVER_OR_THROW(settingsSource, person);
    NX_READ_FROM_SERVER_OR_THROW(settingsSource, vehicle);
    NX_READ_FROM_SERVER_OR_THROW(settingsSource, face);
    NX_READ_FROM_SERVER_OR_THROW(settingsSource, licensePlate);
    initialized = true;
}

void ObjectDetectionBestShot::readFromCameraOrThrow(const nx::kit::Json& channelInfo, FrameSize frameSize)
{
    // This doesn't work in the PNO-A0981R now (bug in the camera).
    sunapiReadOrThrow(channelInfo, "BestShot", frameSize, &person, "Person");
    sunapiReadOrThrow(channelInfo, "BestShot", frameSize, &vehicle, "Vehicle");
    sunapiReadOrThrow(channelInfo, "BestShot", frameSize, &face, "Face");
    sunapiReadOrThrow(channelInfo, "BestShot", frameSize, &licensePlate, "LicensePlate");
    initialized = true;
}

std::string ObjectDetectionBestShot::buildObjectTypes() const
{
    std::vector<const char*> mode;

    if (person)
        mode.push_back("Person");

    if (vehicle)
        mode.push_back("Vehicle");

    if (face)
        mode.push_back("Face");

    if (licensePlate)
        mode.push_back("LicensePlate");

    return concat(mode);
}

std::string ObjectDetectionBestShot::buildCameraWritingQuery(FrameSize /*frameSize*/, int channelNumber) const
{
    std::ostringstream query;
    if (initialized)
    {
        query
            << "msubmenu=" << "metaimagetransfer"
            << "&action=" << "set"
            << "&Channel=" << channelNumber
            << "&ObjectTypes=" << buildObjectTypes()
            ;
    }
    return query.str();
}

//-------------------------------------------------------------------------------------------------

bool ObjectDetectionExcludeArea::operator==(const ObjectDetectionExcludeArea& rhs) const
{
    return initialized == rhs.initialized
        && points == rhs.points
        ;
}
void ObjectDetectionExcludeArea::readFromServerOrThrow(const nx::sdk::IStringMap* settingsSource, int roiIndex)
{
    NX_READ_FROM_SERVER_OR_THROW(settingsSource, points);
    initialized = true;
}

void ObjectDetectionExcludeArea::readFromCameraOrThrow(const nx::kit::Json& channelInfo, FrameSize frameSize)
{
    nx::kit::Json roiInfo = getOdRoiInfo(channelInfo, this->deviceIndex());
    if (roiInfo == nx::kit::Json())
    {
        *this = ObjectDetectionExcludeArea(this->nativeIndex());
        return;
    }
    sunapiReadOrThrow(roiInfo, "Coordinates", frameSize, &points);
    initialized = true;
}

std::string ObjectDetectionExcludeArea::buildCameraWritingQuery(FrameSize frameSize, int channelNumber) const
{
    std::ostringstream query;
    if (initialized)
    {
        if (!points.empty())
        {
            const std::string prefix = "&ExcludeArea."s + std::to_string(deviceIndex());
            query
                << "msubmenu=" << "objectdetection"
                << "&action=" << "set"
                << "&Channel=" << channelNumber
                << prefix << ".Coordinate=" << pluginPointsToSunapiString(points, frameSize)
                ;
        }
        else
        {
            query
                << "msubmenu=" << "objectdetection"
                << "&action=" << "remove"
                << "&Channel=" << channelNumber
                << "&ExcludeAreaIndex=" << deviceIndex()
                ;
        }
    }
    return query.str();
}

//-------------------------------------------------------------------------------------------------

bool IvaLine::operator==(const IvaLine& rhs) const
{
    return initialized == rhs.initialized
        && points == rhs.points
        && name == rhs.name
        && person == rhs.person
        && vehicle == rhs.vehicle
        && crossing == rhs.crossing
        && direction == rhs.direction
        ;
}

void IvaLine::readFromServerOrThrow(const nx::sdk::IStringMap* settingsSource, int roiIndex)
{
    NX_READ_FROM_SERVER_OR_THROW(settingsSource, points);
    if (!points.empty())
        NX_READ_FROM_SERVER_OR_THROW(settingsSource, direction);
    NX_READ_FROM_SERVER_OR_THROW(settingsSource, name);
    NX_READ_FROM_SERVER_OR_THROW(settingsSource, person);
    NX_READ_FROM_SERVER_OR_THROW(settingsSource, vehicle);
    NX_READ_FROM_SERVER_OR_THROW(settingsSource, crossing);
    initialized = true;
}

void IvaLine::readFromCameraOrThrow(const nx::kit::Json& channelInfo, FrameSize frameSize)
{
    nx::kit::Json lineInfo = getIvaLineInfo(channelInfo, this->deviceIndex());
    if (lineInfo == nx::kit::Json())
    {
        *this = IvaLine(this->nativeIndex()); // reset value;
        return;
    }

    sunapiReadOrThrow(lineInfo, "Coordinates", frameSize, &points);
    sunapiReadOrThrow(lineInfo, "Mode", frameSize, &direction);
    sunapiReadOrThrow(lineInfo, "RuleName", frameSize, &name);
    sunapiReadOrThrow(lineInfo, "ObjectTypeFilter", frameSize, &person, "Person");
    sunapiReadOrThrow(lineInfo, "ObjectTypeFilter", frameSize, &vehicle, "Vehicle");

    std::string Crossing;
    sunapiReadOrThrow(lineInfo, "Mode", frameSize, &Crossing);
    this->crossing = Crossing != "Off";
    initialized = true;
}

std::string IvaLine::buildFilter() const
{
    std::vector<const char*> mode;

    if (person)
        mode.push_back("Person");

    if (vehicle)
        mode.push_back("Vehicle");

    return concat(mode);
}

std::string IvaLine::buildMode() const
{
    if (!crossing)
        return "Off";
    if (direction == Direction::Right)
        return "Right";
    if (direction == Direction::Left)
        return "Left";
    if (direction == Direction::Both)
        return "BothDirections";

    NX_ASSERT(false);
    return {};
}

std::string IvaLine::buildCameraWritingQuery(FrameSize frameSize, int channelNumber) const
{
    std::ostringstream query;
    if (initialized)
    {
        if (!points.empty())
        {
            const std::string prefix = "&Line."s + std::to_string(deviceIndex());
            query
                << "msubmenu=" << "videoanalysis2"
                << "&action=" << "set"
                << "&Channel=" << channelNumber
                << prefix << ".Coordinate=" << pluginPointsToSunapiString(points, frameSize)
                << prefix << ".RuleName=" << name
                << prefix << ".ObjectTypeFilter=" << buildFilter()
                << prefix << ".Mode=" << buildMode()
                ;
        }
        else
        {
            query
                << "msubmenu=" << "videoanalysis2"
                << "&action=" << "remove"
                << "&Channel=" << channelNumber
                << "&LineIndex=" << deviceIndex();
        }
    }
    return query.str();
}

//-------------------------------------------------------------------------------------------------

bool IvaIncludeArea::operator==(const IvaIncludeArea& rhs) const
{
    return initialized == rhs.initialized
        && points == rhs.points
        && name == rhs.name
        && person == rhs.person
        && vehicle == rhs.vehicle
        && intrusion == rhs.intrusion
        && enter == rhs.enter
        && exit == rhs.exit
        && appear == rhs.appear
        && loitering == rhs.loitering
        && intrusionDuration == rhs.intrusionDuration
        && appearDuration == rhs.appearDuration
        && loiteringDuration == rhs.loiteringDuration
        ;
}

void IvaIncludeArea::readFromServerOrThrow(const nx::sdk::IStringMap* settingsSource, int roiIndex)
{
    NX_READ_FROM_SERVER_OR_THROW(settingsSource, points);
    NX_READ_FROM_SERVER_OR_THROW(settingsSource, name);
    NX_READ_FROM_SERVER_OR_THROW(settingsSource, person);
    NX_READ_FROM_SERVER_OR_THROW(settingsSource, vehicle);
    NX_READ_FROM_SERVER_OR_THROW(settingsSource, intrusion);
    NX_READ_FROM_SERVER_OR_THROW(settingsSource, enter);
    NX_READ_FROM_SERVER_OR_THROW(settingsSource, exit);
    NX_READ_FROM_SERVER_OR_THROW(settingsSource, appear);
    NX_READ_FROM_SERVER_OR_THROW(settingsSource, loitering);
    NX_READ_FROM_SERVER_OR_THROW(settingsSource, intrusionDuration);
    NX_READ_FROM_SERVER_OR_THROW(settingsSource, appearDuration);
    NX_READ_FROM_SERVER_OR_THROW(settingsSource, loiteringDuration);
    initialized = true;
}

void IvaIncludeArea::readFromCameraOrThrow(const nx::kit::Json& channelInfo, FrameSize frameSize)
{

    nx::kit::Json roiInfo = getIvaRoiInfo(channelInfo, this->deviceIndex());
    if (roiInfo == nx::kit::Json())
    {
        *this = IvaIncludeArea(this->nativeIndex()); // reset value;
        return ;
    }

    sunapiReadOrThrow(roiInfo, "Coordinates", frameSize, &points);
    sunapiReadOrThrow(roiInfo, "RuleName", frameSize, &name);
    sunapiReadOrThrow(roiInfo, "ObjectTypeFilter", frameSize, &person, "Person");
    sunapiReadOrThrow(roiInfo, "ObjectTypeFilter", frameSize, &vehicle, "Vehicle");

    sunapiReadOrThrow(roiInfo, "Mode", frameSize, &intrusion, "Intrusion");
    sunapiReadOrThrow(roiInfo, "Mode", frameSize, &enter, "Entering");
    sunapiReadOrThrow(roiInfo, "Mode", frameSize, &exit, "Exiting");
    sunapiReadOrThrow(roiInfo, "Mode", frameSize, &appear, "AppearDisappear");
    sunapiReadOrThrow(roiInfo, "Mode", frameSize, &loitering, "Loitering");

    sunapiReadOrThrow(roiInfo, "IntrusionDuration", frameSize, &intrusionDuration);
    sunapiReadOrThrow(roiInfo, "AppearanceDuration", frameSize, &appearDuration);
    sunapiReadOrThrow(roiInfo, "LoiteringDuration", frameSize, &loiteringDuration);
    initialized = true;
}

std::string IvaIncludeArea::buildMode() const
{
    std::vector<const char*> mode;

    if (intrusion)
        mode.push_back("Intrusion");

    if (enter)
        mode.push_back("Entering");

    if (exit)
        mode.push_back("Exiting");

    if (appear)
        mode.push_back("AppearDisappear");

    if (loitering)
        mode.push_back("Loitering");

    return concat(mode);
}

std::string IvaIncludeArea::buildFilter() const
{
    std::vector<const char*> mode;

    if (person)
        mode.push_back("Person");

    if (vehicle)
        mode.push_back("Vehicle");

    return concat(mode);
}

std::string IvaIncludeArea::buildCameraWritingQuery(FrameSize frameSize, int channelNumber) const
{
    std::ostringstream query;
    if (initialized)
    {
        if (!points.empty())
        {
            const std::string prefix = "&DefinedArea."s + std::to_string(deviceIndex());
            query
                << "msubmenu=" << "videoanalysis2"
                << "&action=" << "set"
                << "&Channel=" << channelNumber
                << prefix << ".Coordinate=" << pluginPointsToSunapiString(points, frameSize)
                << prefix << ".Type=" << "Inside"
                << prefix << ".RuleName=" << name
                << prefix << ".ObjectTypeFilter=" << buildFilter()
                << prefix << ".Mode=" << buildMode()
                << prefix << ".IntrusionDuration=" << intrusionDuration
                << prefix << ".AppearanceDuration=" << appearDuration
                << prefix << ".LoiteringDuration=" << loiteringDuration
                ;
        }
        else
        {
                query
                << "msubmenu=" << "videoanalysis2"
                << "&action=" << "remove"
                << "&Channel=" << channelNumber
                << "&DefinedAreaIndex=" << deviceIndex();
        }
    }
    return query.str();
}

//-------------------------------------------------------------------------------------------------

bool IvaExcludeArea::operator==(const IvaExcludeArea& rhs) const
{
    return initialized == rhs.initialized
        && points == rhs.points
        ;
}

void IvaExcludeArea::readFromServerOrThrow(const nx::sdk::IStringMap* settingsSource, int roiIndex)
{
    NX_READ_FROM_SERVER_OR_THROW(settingsSource, points);
    initialized = true;
}

void IvaExcludeArea::readFromCameraOrThrow(const nx::kit::Json& channelInfo, FrameSize frameSize)
{
    nx::kit::Json roiInfo = getIvaRoiInfo(channelInfo, this->deviceIndex());
    if (roiInfo == nx::kit::Json())
    {
        *this = IvaExcludeArea(this->nativeIndex());
        return;
    }
    sunapiReadOrThrow(roiInfo, "Coordinates", frameSize, &points);
    initialized = true;
}

std::string IvaExcludeArea::buildCameraWritingQuery(FrameSize frameSize, int channelNumber) const
{
    std::ostringstream query;
    if (initialized)
    {
        if (!points.empty())
        {
            const std::string prefix = "&DefinedArea."s + std::to_string(deviceIndex());
            query
                << "msubmenu=" << "videoanalysis2"
                << "&action=" << "set"
                << "&Channel=" << channelNumber
                << prefix << ".Coordinate=" << pluginPointsToSunapiString(points, frameSize)
                << prefix << ".Type=" << "Outside"
                ;
        }
        else
        {
            query
                << "msubmenu=" << "videoanalysis2"
                << "&action=" << "remove"
                << "&Channel=" << channelNumber
                << "&DefinedAreaIndex=" << deviceIndex()
                ;
        }
    }
    return query.str();
}

//-------------------------------------------------------------------------------------------------

bool AudioDetection::operator==(const AudioDetection& rhs) const
{
    return initialized == rhs.initialized
        && enabled == rhs.enabled
        && thresholdLevel == rhs.thresholdLevel
        ;
}

void AudioDetection::readFromServerOrThrow(const nx::sdk::IStringMap* settingsSource, int /*roiIndex*/)
{
    NX_READ_FROM_SERVER_OR_THROW(settingsSource, enabled);
    NX_READ_FROM_SERVER_OR_THROW(settingsSource, thresholdLevel);
    initialized = true;
}

void AudioDetection::writeToServer(nx::sdk::SettingsResponse* settingsDestination, int /*roiIndex*/) const
{
    NX_WRITE_TO_SERVER(settingsDestination, enabled);
    NX_WRITE_TO_SERVER(settingsDestination, thresholdLevel);
}

void AudioDetection::readFromCameraOrThrow(const nx::kit::Json& channelInfo, FrameSize frameSize)
{
    sunapiReadOrThrow(channelInfo, "Enable", frameSize, &enabled);
    sunapiReadOrThrow(channelInfo, "InputThresholdLevel", frameSize, &thresholdLevel);
    initialized = true;
}

std::string AudioDetection::buildCameraWritingQuery(FrameSize /*frameSize*/, int channelNumber) const
{
    std::ostringstream query;
    if (initialized)
    {
        query
            << "msubmenu=" << "audiodetection"
            << "&action=" << "set"
            << "&Channel=" << channelNumber
            << "&Enable=" << buildBool(enabled)
            << "&InputThresholdLevel=" << thresholdLevel
            ;
    }
    return query.str();
}

//-------------------------------------------------------------------------------------------------

bool SoundClassification::operator==(const SoundClassification& rhs) const
{
    return initialized == rhs.initialized
        && enabled == rhs.enabled
        && noisefilter == rhs.noisefilter
        && thresholdLevel == rhs.thresholdLevel
        && scream == rhs.scream
        && gunshot == rhs.gunshot
        && explosion == rhs.explosion
        && crashingGlass == rhs.crashingGlass
        ;
}

void SoundClassification::readFromServerOrThrow(const nx::sdk::IStringMap* settingsSource, int /*roiIndex*/)
{
    NX_READ_FROM_SERVER_OR_THROW(settingsSource, enabled);
    NX_READ_FROM_SERVER_OR_THROW(settingsSource, noisefilter);
    NX_READ_FROM_SERVER_OR_THROW(settingsSource, thresholdLevel);
    NX_READ_FROM_SERVER_OR_THROW(settingsSource, scream);
    NX_READ_FROM_SERVER_OR_THROW(settingsSource, gunshot);
    NX_READ_FROM_SERVER_OR_THROW(settingsSource, explosion);
    NX_READ_FROM_SERVER_OR_THROW(settingsSource, crashingGlass);
    initialized = true;
}

void SoundClassification::writeToServer(nx::sdk::SettingsResponse* settingsDestination, int /*roiIndex*/) const
{
    NX_WRITE_TO_SERVER(settingsDestination, enabled);
    NX_WRITE_TO_SERVER(settingsDestination, noisefilter);
    NX_WRITE_TO_SERVER(settingsDestination, thresholdLevel);
    NX_WRITE_TO_SERVER(settingsDestination, scream);
    NX_WRITE_TO_SERVER(settingsDestination, gunshot);
    NX_WRITE_TO_SERVER(settingsDestination, explosion);
    NX_WRITE_TO_SERVER(settingsDestination, crashingGlass);
}

void SoundClassification::readFromCameraOrThrow(
    const nx::kit::Json& channelInfo, FrameSize frameSize)
{
    sunapiReadOrThrow(channelInfo, "Enable", frameSize, &enabled);
    sunapiReadOrThrow(channelInfo, "NoiseReduction", frameSize, &noisefilter);
    sunapiReadOrThrow(channelInfo, "ThresholdLevel", frameSize, &thresholdLevel);
    sunapiReadOrThrow(channelInfo, "SoundType", frameSize, &scream, "Scream");
    sunapiReadOrThrow(channelInfo, "SoundType", frameSize, &gunshot, "Gunshot");
    sunapiReadOrThrow(channelInfo, "SoundType", frameSize, &explosion, "Explosion");
    sunapiReadOrThrow(channelInfo, "SoundType", frameSize, &crashingGlass, "GlassBreak");
    initialized = true;
}

std::string SoundClassification::buildSoundType() const
{
    std::vector<const char*> mode;

    if (scream)
        mode.push_back("Scream");

    if (gunshot)
        mode.push_back("Gunshot");

    if (explosion)
        mode.push_back("Explosion");

    if (crashingGlass)
        mode.push_back("GlassBreak");

    return concat(mode);
}

std::string SoundClassification::buildCameraWritingQuery(FrameSize /*frameSize*/, int channelNumber) const
{
    std::ostringstream query;
    if (initialized)
    {
        query
            << "msubmenu=" << "audioanalysis"
            << "&action=" << "set"
            << "&Channel=" << channelNumber
            << "&Enable=" << buildBool(enabled)
            << "&NoiseReduction=" << buildBool(noisefilter)
            << "&ThresholdLevel=" << thresholdLevel
            << "&SoundType=" << buildSoundType()
            ;
    }
    return query.str();
}

//-------------------------------------------------------------------------------------------------

} // namespace nx::vms_server_plugins::analytics::hanwha
