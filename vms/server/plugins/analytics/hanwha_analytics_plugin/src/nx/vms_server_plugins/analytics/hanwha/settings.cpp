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
    return result;
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
    return result;
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
    return result;
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

template<class E>
std::string SettingGroup::key(E keyIndexE) const
{
    const int keyIndex = (int)keyIndexE;
    NX_ASSERT(keyIndex < serverKeyCount);

    std::string result(serverKeys[keyIndex]);
    if (serverIndex() >= 0)
        result.replace(result.find("#"), 1, std::to_string(serverIndex()));

    return result;
}

template<class E>
const char* SettingGroup::value(const nx::sdk::IStringMap* sourceMap, E keyIndexE)
{
    const std::string key = this->key(keyIndexE);
    return sourceMap->value(key.c_str());
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
}

std::string serverWrite(std::string value)
{
    return value;
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
    std::string result = pluginPointsToServerString(*points);
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

void sunapiReadOrThrow(const nx::kit::Json& json, const char* key, FrameSize frameSize, std::vector<PluginPoint>* result)
{
    NX_ASSERT(key);

    const auto& points = json[key];
    if (!points.is_array())
        throw DeviceValueError{};

    result->reserve(points.array_items().size());
    for (const nx::kit::Json& point : points.array_items())
    {
        if (!point["x"].is_number() || !point["y"].is_number())
            throw DeviceValueError{};

        const int ix = point["x"].int_value();
        const int iy = point["y"].int_value();
        result->emplace_back(frameSize.xAbsoluteToRelative(ix), frameSize.yAbsoluteToRelative(iy));
    }
}

void sunapiReadOrThrow(const nx::kit::Json& json, FrameSize frameSize, ObjectSizeConstraints* result)
{
    PluginPoint minSize, maxSize;
    sunapiReadOrThrow(json, "MinimumObjectSizeInPixels", frameSize, &minSize);
    sunapiReadOrThrow(json, "MaximumObjectSizeInPixels", frameSize, &maxSize);
    result->minWidth = minSize.x;
    result->minHeight = minSize.y;
    result->maxWidth = maxSize.x;
    result->maxHeight = maxSize.y;
}

void sunapiReadOrThrow(const nx::kit::Json& json, const char* key, FrameSize frameSize, UnnamedBoxFigure* result)
{
    PluginPoint point;
    sunapiReadOrThrow(json, key, frameSize, &point);
    result->width = point.x;
    result->height = point.y;
}

void sunapiReadOrThrow(const nx::kit::Json& json, const char* key, FrameSize frameSize, UnnamedPolygon* result)
{
    std::vector<PluginPoint> points;
    sunapiReadOrThrow(json, key, frameSize, &points);
    result->points = points;
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

void sunapiReadOrThrow(const nx::kit::Json& json, const char* key, FrameSize /*frameSize*/, Direction* result)
{
    NX_ASSERT(key);

    if (const auto& param = json[key]; param.is_string())
    {
        // Different firmware versions have different valid value sets for line crossing direction.
        if ((param.string_value() == "RightSide") || (param.string_value() == "Right"))
            *result = Direction::Right;
        else if ((param.string_value() == "LeftSide") || (param.string_value() == "Left"))
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

bool ShockDetection::operator==(const ShockDetection& rhs) const
{
    return initialized == rhs.initialized
        && enabled == rhs.enabled
        && thresholdLevel == rhs.thresholdLevel
        && sensitivityLevel == rhs.sensitivityLevel
        ;
}

void ShockDetection::readFromServerOrThrow(
    const nx::sdk::IStringMap* sourceMap, int /*roiIndex*/)
{
    using namespace basicServerSettingsIo;
    deserializeOrThrow(value(sourceMap, KeyIndex::enabled), &enabled);
    deserializeOrThrow(value(sourceMap, KeyIndex::thresholdLevel), &thresholdLevel);
    deserializeOrThrow(value(sourceMap, KeyIndex::sensitivityLevel), &sensitivityLevel);
    initialized = true;
}

void ShockDetection::writeToServer(nx::sdk::SettingsResponse* result, int /*roiIndex*/) const
{
    using namespace basicServerSettingsIo;
    result->setValue(key(KeyIndex::enabled), serialize(enabled));
    result->setValue(key(KeyIndex::thresholdLevel), serialize(thresholdLevel));
    result->setValue(key(KeyIndex::sensitivityLevel), serialize(sensitivityLevel));
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
            << "msubmenu=" << kSunapiEventName
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

void Motion::readFromServerOrThrow(const nx::sdk::IStringMap* sourceMap, int /*roiIndex*/)
{
    using namespace basicServerSettingsIo;
    deserializeOrThrow(value(sourceMap, KeyIndex::detectionType), &detectionType);
    initialized = true;
}

void Motion::writeToServer(nx::sdk::SettingsResponse* result, int /*roiIndex*/) const
{
    using namespace basicServerSettingsIo;
    result->setValue(key(KeyIndex::detectionType), serialize(detectionType));
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
        && constraints.minWidth == rhs.constraints.minWidth
        && constraints.minHeight == rhs.constraints.minHeight
        && constraints.maxWidth == rhs.constraints.maxWidth
        && constraints.maxHeight == rhs.constraints.maxHeight
        ;
}

void MotionDetectionObjectSize::readFromServerOrThrow(const nx::sdk::IStringMap* sourceMap, int /*roiIndex*/)
{
    using namespace basicServerSettingsIo;
    deserializeOrThrow(value(sourceMap, KeyIndex::constraints), &constraints);
    initialized = true;
}

void MotionDetectionObjectSize::writeToServer(
    nx::sdk::SettingsResponse* result, int /*roiIndex*/) const
{
    using namespace basicServerSettingsIo;
    result->setValue(key(KeyIndex::constraints), serialize(constraints));
}

void MotionDetectionObjectSize::readFromCameraOrThrow(const nx::kit::Json& channelInfo, FrameSize frameSize)
{
    nx::kit::Json objectSizeInfo = getObjectSizeInfo(channelInfo, "MotionDetection");
    sunapiReadOrThrow(objectSizeInfo, frameSize, &constraints);
    initialized = true;
}

std::string MotionDetectionObjectSize::buildMinObjectSize(FrameSize frameSize) const
{
    std::stringstream stream;
    stream
        << frameSize.xRelativeToAbsolute(constraints.minWidth)
        << ','
        << frameSize.yRelativeToAbsolute(constraints.minHeight);
    return stream.str();
}

std::string MotionDetectionObjectSize::buildMaxObjectSize(FrameSize frameSize) const
{
    std::stringstream stream;
    stream
        << frameSize.xRelativeToAbsolute(constraints.maxWidth)
        << ','
        << frameSize.yRelativeToAbsolute(constraints.maxHeight);
    return stream.str();
}

std::string MotionDetectionObjectSize::buildCameraWritingQuery(FrameSize frameSize, int channelNumber) const
{
    std::ostringstream query;
    if (initialized)
    {
        query
            << "msubmenu=" << kSunapiEventName
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
        && constraints.minWidth == rhs.constraints.minWidth
        && constraints.minHeight == rhs.constraints.minHeight
        && constraints.maxWidth == rhs.constraints.maxWidth
        && constraints.maxHeight == rhs.constraints.maxHeight
        ;
}

void IvaObjectSize::readFromServerOrThrow(const nx::sdk::IStringMap* sourceMap, int /*roiIndex*/)
{
    using namespace basicServerSettingsIo;
    deserializeOrThrow(value(sourceMap, KeyIndex::constraints), &constraints);
    initialized = true;
}

void IvaObjectSize::writeToServer(nx::sdk::SettingsResponse* result, int /*roiIndex*/) const
{
    using namespace basicServerSettingsIo;
    result->setValue(key(KeyIndex::constraints), serialize(constraints));
}

void IvaObjectSize::readFromCameraOrThrow(const nx::kit::Json& channelInfo, FrameSize frameSize)
{
    nx::kit::Json objectSizeInfo = getObjectSizeInfo(channelInfo, "MotionDetection");
    sunapiReadOrThrow(objectSizeInfo, frameSize, &constraints);
    initialized = true;
}

std::string IvaObjectSize::buildMinObjectSize(FrameSize frameSize) const
{
    std::stringstream stream;
    stream
        << frameSize.xRelativeToAbsolute(constraints.minWidth)
        << ','
        << frameSize.yRelativeToAbsolute(constraints.minHeight);
    return stream.str();
}

std::string IvaObjectSize::buildMaxObjectSize(FrameSize frameSize) const
{
    std::stringstream stream;
    stream
        << frameSize.xRelativeToAbsolute(constraints.maxWidth)
        << ','
        << frameSize.yRelativeToAbsolute(constraints.maxHeight);
    return stream.str();
}

std::string IvaObjectSize::buildCameraWritingQuery(FrameSize frameSize, int channelNumber) const
{
    std::ostringstream query;
    if (initialized)
    {
        query
            << "msubmenu=" << kSunapiEventName
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
        && unnamedPolygon == rhs.unnamedPolygon
        && thresholdLevel == rhs.thresholdLevel
        && sensitivityLevel == rhs.sensitivityLevel
        && minimumDuration == rhs.minimumDuration
        ;
}

void MotionDetectionIncludeArea::readFromServerOrThrow(const nx::sdk::IStringMap* sourceMap, int /*roiIndex*/)
{
    using namespace basicServerSettingsIo;
    deserializeOrThrow(value(sourceMap, KeyIndex::unnamedPolygon), &unnamedPolygon);
    deserializeOrThrow(value(sourceMap, KeyIndex::thresholdLevel), &thresholdLevel);
    deserializeOrThrow(value(sourceMap, KeyIndex::sensitivityLevel), &sensitivityLevel);
    deserializeOrThrow(value(sourceMap, KeyIndex::minimumDuration), &minimumDuration);
    initialized = true;
}

void MotionDetectionIncludeArea::writeToServer(nx::sdk::SettingsResponse* result, int /*roiIndex*/) const
{
    using namespace basicServerSettingsIo;
    result->setValue(key(KeyIndex::unnamedPolygon), serialize(unnamedPolygon));
    result->setValue(key(KeyIndex::thresholdLevel), serialize(thresholdLevel));
    result->setValue(key(KeyIndex::sensitivityLevel), serialize(sensitivityLevel));
    result->setValue(key(KeyIndex::minimumDuration), serialize(minimumDuration));
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

        sunapiReadOrThrow(roiInfo, "Coordinates", frameSize, &unnamedPolygon);
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
        if (!unnamedPolygon.points.empty())
        {
            const std::string prefix = "&ROI."s + std::to_string(deviceIndex());
            query
                << "msubmenu=" << kSunapiEventName
                << "&action=" << "set"
                << "&Channel=" << channelNumber
                << prefix << ".Coordinate=" << pluginPointsToSunapiString(unnamedPolygon.points, frameSize)
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
        && unnamedPolygon == rhs.unnamedPolygon
        ;
}

void MotionDetectionExcludeArea::readFromServerOrThrow(const nx::sdk::IStringMap* sourceMap, int /*roiIndex*/)
{
    using namespace basicServerSettingsIo;
    deserializeOrThrow(value(sourceMap, KeyIndex::unnamedPolygon), &unnamedPolygon);
    initialized = true;
}

void MotionDetectionExcludeArea::writeToServer(nx::sdk::SettingsResponse* result, int /*roiIndex*/) const
{
    using namespace basicServerSettingsIo;
    result->setValue(key(KeyIndex::unnamedPolygon), serialize(unnamedPolygon));
}

void MotionDetectionExcludeArea::readFromCameraOrThrow(const nx::kit::Json& channelInfo, FrameSize frameSize)
{
    nx::kit::Json roiInfo = getMdRoiInfo(channelInfo, this->deviceIndex());
    if (roiInfo == nx::kit::Json(this->deviceIndex()))
    {
        *this = MotionDetectionExcludeArea(this->nativeIndex());
        return;
    }
    sunapiReadOrThrow(roiInfo, "Coordinates", frameSize, &unnamedPolygon);
    initialized = true;
}

std::string MotionDetectionExcludeArea::buildCameraWritingQuery(FrameSize frameSize, int channelNumber) const
{
    std::ostringstream query;
    if (initialized)
    {
        if (!unnamedPolygon.points.empty())
        {
            const std::string prefix = "&ROI."s + std::to_string(deviceIndex());
            query
                << "msubmenu=" << kSunapiEventName
                << "&action=" << "set"
                << "&Channel=" << channelNumber
                << prefix << ".Coordinate=" << pluginPointsToSunapiString(unnamedPolygon.points, frameSize)
                ;
        }
        else
        {
            query
                << "msubmenu=" << kSunapiEventName
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

void TamperingDetection::readFromServerOrThrow(const nx::sdk::IStringMap* sourceMap, int /*roiIndex*/)
{
    using namespace basicServerSettingsIo;
    deserializeOrThrow(value(sourceMap, KeyIndex::enabled), &enabled);
    deserializeOrThrow(value(sourceMap, KeyIndex::thresholdLevel), &thresholdLevel);
    deserializeOrThrow(value(sourceMap, KeyIndex::sensitivityLevel), &sensitivityLevel);
    deserializeOrThrow(value(sourceMap, KeyIndex::minimumDuration), &minimumDuration);
    deserializeOrThrow(value(sourceMap, KeyIndex::exceptDarkImages), &exceptDarkImages);
    initialized = true;
}

void TamperingDetection::writeToServer(nx::sdk::SettingsResponse* result, int /*roiIndex*/) const
{
    using namespace basicServerSettingsIo;
    result->setValue(key(KeyIndex::enabled), serialize(enabled));
    result->setValue(key(KeyIndex::thresholdLevel), serialize(thresholdLevel));
    result->setValue(key(KeyIndex::sensitivityLevel), serialize(sensitivityLevel));
    result->setValue(key(KeyIndex::minimumDuration), serialize(minimumDuration));
    result->setValue(key(KeyIndex::exceptDarkImages), serialize(exceptDarkImages));
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
            << "msubmenu=" << kSunapiEventName
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

void DefocusDetection::readFromServerOrThrow(const nx::sdk::IStringMap* sourceMap, int /*roiIndex*/)
{
    using namespace basicServerSettingsIo;
    deserializeOrThrow(value(sourceMap, KeyIndex::enabled), &enabled);
    deserializeOrThrow(value(sourceMap, KeyIndex::thresholdLevel), &thresholdLevel);
    deserializeOrThrow(value(sourceMap, KeyIndex::sensitivityLevel), &sensitivityLevel);
    deserializeOrThrow(value(sourceMap, KeyIndex::minimumDuration), &minimumDuration);
    initialized = true;
}

void DefocusDetection::writeToServer(nx::sdk::SettingsResponse* result, int /*roiIndex*/) const
{
    using namespace basicServerSettingsIo;
    result->setValue(key(KeyIndex::enabled), serialize(enabled));
    result->setValue(key(KeyIndex::thresholdLevel), serialize(thresholdLevel));
    result->setValue(key(KeyIndex::sensitivityLevel), serialize(sensitivityLevel));
    result->setValue(key(KeyIndex::minimumDuration), serialize(minimumDuration));
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
            << "msubmenu=" << kSunapiEventName
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

void FogDetection::readFromServerOrThrow(const nx::sdk::IStringMap* sourceMap, int /*roiIndex*/)
{
    using namespace basicServerSettingsIo;
    deserializeOrThrow(value(sourceMap, KeyIndex::enabled), &enabled);
    deserializeOrThrow(value(sourceMap, KeyIndex::thresholdLevel), &thresholdLevel);
    deserializeOrThrow(value(sourceMap, KeyIndex::sensitivityLevel), &sensitivityLevel);
    deserializeOrThrow(value(sourceMap, KeyIndex::minimumDuration), &minimumDuration);
    initialized = true;
}

void FogDetection::writeToServer(nx::sdk::SettingsResponse* result, int /*roiIndex*/) const
{
    using namespace basicServerSettingsIo;
    result->setValue(key(KeyIndex::enabled), serialize(enabled));
    result->setValue(key(KeyIndex::thresholdLevel), serialize(thresholdLevel));
    result->setValue(key(KeyIndex::sensitivityLevel), serialize(sensitivityLevel));
    result->setValue(key(KeyIndex::minimumDuration), serialize(minimumDuration));
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
            << "msubmenu=" << kSunapiEventName
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

#if 0
Face detection event is now built in object detection event.

bool FaceDetectionGeneral::operator==(const FaceDetectionGeneral& rhs) const
{
    return initialized == rhs.initialized
        && enabled == rhs.enabled
        && sensitivityLevel == rhs.sensitivityLevel
        ;
}

void FaceDetectionGeneral::readFromServerOrThrow(const nx::sdk::IStringMap* sourceMap, int /*roiIndex*/)
{
    using namespace basicServerSettingsIo;
    deserializeOrThrow(value(sourceMap, KeyIndex::enabled), &enabled);
    deserializeOrThrow(value(sourceMap, KeyIndex::sensitivityLevel), &sensitivityLevel);
    initialized = true;
}

void FaceDetectionGeneral::writeToServer(nx::sdk::SettingsResponse* result, int /*roiIndex*/) const
{
    using namespace basicServerSettingsIo;
    result->setValue(key(KeyIndex::enabled), serialize(enabled));
    result->setValue(key(KeyIndex::sensitivityLevel), serialize(sensitivityLevel));
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
            << "msubmenu=" << kSunapiEventName
            << "&action=" << "set"
            << "&Channel=" << channelNumber
            << "&Enable=" << buildBool(enabled)
            << "&Sensitivity=" << sensitivityLevel
            ;
    }
    return query.str();
}

#endif

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

void ObjectDetectionGeneral::readFromServerOrThrow(const nx::sdk::IStringMap* sourceMap, int /*roiIndex*/)
{
    using namespace basicServerSettingsIo;
    deserializeOrThrow(value(sourceMap, KeyIndex::enabled), &enabled);
    deserializeOrThrow(value(sourceMap, KeyIndex::person), &person);
    deserializeOrThrow(value(sourceMap, KeyIndex::vehicle), &vehicle);
    deserializeOrThrow(value(sourceMap, KeyIndex::face), &face);
    deserializeOrThrow(value(sourceMap, KeyIndex::licensePlate), &licensePlate);
    deserializeOrThrow(value(sourceMap, KeyIndex::minimumDuration), &minimumDuration);
    initialized = true;
}

void ObjectDetectionGeneral::writeToServer(nx::sdk::SettingsResponse* result, int /*roiIndex*/) const
{
    using namespace basicServerSettingsIo;
    result->setValue(key(KeyIndex::enabled), serialize(enabled));
    result->setValue(key(KeyIndex::person), serialize(person));
    result->setValue(key(KeyIndex::vehicle), serialize(vehicle));
    result->setValue(key(KeyIndex::face), serialize(face));
    result->setValue(key(KeyIndex::licensePlate), serialize(licensePlate));
    result->setValue(key(KeyIndex::minimumDuration), serialize(minimumDuration));
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
            << "msubmenu=" << kSunapiEventName
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

void ObjectDetectionBestShot::readFromServerOrThrow(const nx::sdk::IStringMap* sourceMap, int /*roiIndex*/)
{
    using namespace basicServerSettingsIo;
    deserializeOrThrow(value(sourceMap, KeyIndex::person), &person);
    deserializeOrThrow(value(sourceMap, KeyIndex::vehicle), &vehicle);
    deserializeOrThrow(value(sourceMap, KeyIndex::face), &face);
    deserializeOrThrow(value(sourceMap, KeyIndex::licensePlate), &licensePlate);
    initialized = true;
}

void ObjectDetectionBestShot::writeToServer(nx::sdk::SettingsResponse* result, int /*roiIndex*/) const
{
    using namespace basicServerSettingsIo;
    result->setValue(key(KeyIndex::person), serialize(person));
    result->setValue(key(KeyIndex::vehicle), serialize(vehicle));
    result->setValue(key(KeyIndex::face), serialize(face));
    result->setValue(key(KeyIndex::licensePlate), serialize(licensePlate));
}

void ObjectDetectionBestShot::readFromCameraOrThrow(const nx::kit::Json& channelInfo, FrameSize frameSize)
{
    sunapiReadOrThrow(channelInfo, "ObjectTypes", frameSize, &person, "Person");
    sunapiReadOrThrow(channelInfo, "ObjectTypes", frameSize, &vehicle, "Vehicle");
    sunapiReadOrThrow(channelInfo, "ObjectTypes", frameSize, &face, "Face");
    sunapiReadOrThrow(channelInfo, "ObjectTypes", frameSize, &licensePlate, "LicensePlate");
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
            << "msubmenu=" << kSunapiEventName
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
        && unnamedPolygon == rhs.unnamedPolygon
        ;
}
void ObjectDetectionExcludeArea::readFromServerOrThrow(const nx::sdk::IStringMap* sourceMap, int /*roiIndex*/)
{
    using namespace basicServerSettingsIo;
    deserializeOrThrow(value(sourceMap, KeyIndex::unnamedPolygon), &unnamedPolygon);
    initialized = true;
}

void ObjectDetectionExcludeArea::writeToServer(nx::sdk::SettingsResponse* result, int /*roiIndex*/) const
{
    using namespace basicServerSettingsIo;
    result->setValue(key(KeyIndex::unnamedPolygon), serialize(unnamedPolygon));
}

void ObjectDetectionExcludeArea::readFromCameraOrThrow(const nx::kit::Json& channelInfo, FrameSize frameSize)
{
    nx::kit::Json roiInfo = getOdRoiInfo(channelInfo, this->deviceIndex());
    if (roiInfo == nx::kit::Json())
    {
        *this = ObjectDetectionExcludeArea(this->nativeIndex());
        return;
    }
    sunapiReadOrThrow(roiInfo, "Coordinates", frameSize, &unnamedPolygon);
    initialized = true;
}

std::string ObjectDetectionExcludeArea::buildCameraWritingQuery(FrameSize frameSize, int channelNumber) const
{
    std::ostringstream query;
    if (initialized)
    {
        if (!unnamedPolygon.points.empty())
        {
            const std::string prefix = "&ExcludeArea."s + std::to_string(deviceIndex());
            query
                << "msubmenu=" << kSunapiEventName
                << "&action=" << "set"
                << "&Channel=" << channelNumber
                << prefix << ".Coordinate=" << pluginPointsToSunapiString(unnamedPolygon.points, frameSize)
                ;
        }
        else
        {
            query
                << "msubmenu=" << kSunapiEventName
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
        && namedLineFigure == rhs.namedLineFigure
        && person == rhs.person
        && vehicle == rhs.vehicle
        && crossing == rhs.crossing
        ;
}

void IvaLine::readFromServerOrThrow(const nx::sdk::IStringMap* sourceMap, int /*roiIndex*/)
{
    using namespace basicServerSettingsIo;
    deserializeOrThrow(value(sourceMap, KeyIndex::namedLineFigure), &namedLineFigure);
    deserializeOrThrow(value(sourceMap, KeyIndex::person), &person);
    deserializeOrThrow(value(sourceMap, KeyIndex::vehicle), &vehicle);
    deserializeOrThrow(value(sourceMap, KeyIndex::crossing), &crossing);

    initialized = true;
}

void IvaLine::writeToServer(nx::sdk::SettingsResponse* result, int /*roiIndex*/) const
{
    using namespace basicServerSettingsIo;
    result->setValue(key(KeyIndex::namedLineFigure), serialize(namedLineFigure));
    result->setValue(key(KeyIndex::person), serialize(person));
    result->setValue(key(KeyIndex::vehicle), serialize(vehicle));
    result->setValue(key(KeyIndex::crossing), serialize(crossing));
}

void IvaLine::readFromCameraOrThrow(const nx::kit::Json& channelInfo, FrameSize frameSize)
{
    nx::kit::Json lineInfo = getIvaLineInfo(channelInfo, this->deviceIndex());
    if (lineInfo == nx::kit::Json())
    {
        *this = IvaLine(this->nativeIndex()); // reset value;
        return;
    }

    sunapiReadOrThrow(lineInfo, "Coordinates", frameSize, &namedLineFigure.points);
    sunapiReadOrThrow(lineInfo, "Mode", frameSize, &namedLineFigure.direction);
    sunapiReadOrThrow(lineInfo, "RuleName", frameSize, &namedLineFigure.name);
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
    if (namedLineFigure.direction == Direction::Right)
        return "Right";
    if (namedLineFigure.direction == Direction::Left)
        return "Left";
    if (namedLineFigure.direction == Direction::Both)
        return "BothDirections";

    NX_ASSERT(false);
    return {};
}

std::string IvaLine::buildCameraWritingQuery(FrameSize frameSize, int channelNumber) const
{
    std::ostringstream query;
    if (initialized)
    {
        if (!namedLineFigure.points.empty())
        {
            const std::string prefix = "&Line."s + std::to_string(deviceIndex());
            query
                << "msubmenu=" << kSunapiEventName
                << "&action=" << "set"
                << "&Channel=" << channelNumber
                << prefix << ".Coordinate=" << pluginPointsToSunapiString(namedLineFigure.points, frameSize)
                << prefix << ".RuleName=" << namedLineFigure.name
                << prefix << ".ObjectTypeFilter=" << buildFilter()
                << prefix << ".Mode=" << buildMode()
                ;
        }
        else
        {
            query
                << "msubmenu=" << kSunapiEventName
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
        && namedPolygon == rhs.namedPolygon
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

void IvaIncludeArea::readFromServerOrThrow(const nx::sdk::IStringMap* sourceMap, int /*roiIndex*/)
{
    using namespace basicServerSettingsIo;
    deserializeOrThrow(value(sourceMap, KeyIndex::namedPolygon), &namedPolygon);
    deserializeOrThrow(value(sourceMap, KeyIndex::person), &person);
    deserializeOrThrow(value(sourceMap, KeyIndex::vehicle), &vehicle);
    deserializeOrThrow(value(sourceMap, KeyIndex::intrusion), &intrusion);
    deserializeOrThrow(value(sourceMap, KeyIndex::enter), &enter);
    deserializeOrThrow(value(sourceMap, KeyIndex::exit), &exit);
    deserializeOrThrow(value(sourceMap, KeyIndex::appear), &appear);
    deserializeOrThrow(value(sourceMap, KeyIndex::loitering), &loitering);
    deserializeOrThrow(value(sourceMap, KeyIndex::intrusionDuration), &intrusionDuration);
    deserializeOrThrow(value(sourceMap, KeyIndex::appearDuration), &appearDuration);
    deserializeOrThrow(value(sourceMap, KeyIndex::loiteringDuration), &loiteringDuration);
    initialized = true;
}

void IvaIncludeArea::writeToServer(nx::sdk::SettingsResponse* result, int /*roiIndex*/) const
{
    using namespace basicServerSettingsIo;
    result->setValue(key(KeyIndex::namedPolygon), serialize(namedPolygon));
    result->setValue(key(KeyIndex::person), serialize(person));
    result->setValue(key(KeyIndex::vehicle), serialize(vehicle));
    result->setValue(key(KeyIndex::intrusion), serialize(intrusion));
    result->setValue(key(KeyIndex::enter), serialize(enter));
    result->setValue(key(KeyIndex::exit), serialize(exit));
    result->setValue(key(KeyIndex::appear), serialize(appear));
    result->setValue(key(KeyIndex::loitering), serialize(loitering));
    result->setValue(key(KeyIndex::intrusionDuration), serialize(intrusionDuration));
    result->setValue(key(KeyIndex::appearDuration), serialize(appearDuration));
    result->setValue(key(KeyIndex::loiteringDuration), serialize(loiteringDuration));
}

void IvaIncludeArea::readFromCameraOrThrow(const nx::kit::Json& channelInfo, FrameSize frameSize)
{

    nx::kit::Json roiInfo = getIvaRoiInfo(channelInfo, this->deviceIndex());
    if (roiInfo == nx::kit::Json())
    {
        *this = IvaIncludeArea(this->nativeIndex()); // reset value;
        return ;
    }

    sunapiReadOrThrow(roiInfo, "Coordinates", frameSize, &namedPolygon.points);
    sunapiReadOrThrow(roiInfo, "RuleName", frameSize, &namedPolygon.name);
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
        if (!namedPolygon.points.empty())
        {
            const std::string prefix = "&DefinedArea."s + std::to_string(deviceIndex());
            query
                << "msubmenu=" << kSunapiEventName
                << "&action=" << "set"
                << "&Channel=" << channelNumber
                << prefix << ".Coordinate=" << pluginPointsToSunapiString(namedPolygon.points, frameSize)
                << prefix << ".Type=" << "Inside"
                << prefix << ".RuleName=" << namedPolygon.name
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
                << "msubmenu=" << kSunapiEventName
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
        && unnamedPolygon == rhs.unnamedPolygon
        ;
}

void IvaExcludeArea::readFromServerOrThrow(const nx::sdk::IStringMap* sourceMap, int /*roiIndex*/)
{
    using namespace basicServerSettingsIo;
    deserializeOrThrow(value(sourceMap, KeyIndex::unnamedPolygon), &unnamedPolygon);
    initialized = true;
}

void IvaExcludeArea::writeToServer(nx::sdk::SettingsResponse* result, int /*roiIndex*/) const
{
    using namespace basicServerSettingsIo;
    result->setValue(key(KeyIndex::unnamedPolygon), serialize(unnamedPolygon));
}

void IvaExcludeArea::readFromCameraOrThrow(const nx::kit::Json& channelInfo, FrameSize frameSize)
{
    nx::kit::Json roiInfo = getIvaRoiInfo(channelInfo, this->deviceIndex());
    if (roiInfo == nx::kit::Json())
    {
        *this = IvaExcludeArea(this->nativeIndex());
        return;
    }
    sunapiReadOrThrow(roiInfo, "Coordinates", frameSize, &unnamedPolygon.points);
    initialized = true;
}

std::string IvaExcludeArea::buildCameraWritingQuery(FrameSize frameSize, int channelNumber) const
{
    std::ostringstream query;
    if (initialized)
    {
        if (!unnamedPolygon.points.empty())
        {
            const std::string prefix = "&DefinedArea."s + std::to_string(deviceIndex());
            query
                << "msubmenu=" << kSunapiEventName
                << "&action=" << "set"
                << "&Channel=" << channelNumber
                << prefix << ".Coordinate=" << pluginPointsToSunapiString(unnamedPolygon.points, frameSize)
                << prefix << ".Type=" << "Outside"
                ;
        }
        else
        {
            query
                << "msubmenu=" << kSunapiEventName
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

void AudioDetection::readFromServerOrThrow(const nx::sdk::IStringMap* sourceMap, int /*roiIndex*/)
{
    using namespace basicServerSettingsIo;
    deserializeOrThrow(value(sourceMap, KeyIndex::enabled), &enabled);
    deserializeOrThrow(value(sourceMap, KeyIndex::thresholdLevel), &thresholdLevel);
    initialized = true;
}

void AudioDetection::writeToServer(nx::sdk::SettingsResponse* result, int /*roiIndex*/) const
{
    using namespace basicServerSettingsIo;
    result->setValue(key(KeyIndex::enabled), serialize(enabled));
    result->setValue(key(KeyIndex::thresholdLevel), serialize(thresholdLevel));
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
            << "msubmenu=" << kSunapiEventName
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

void SoundClassification::readFromServerOrThrow(const nx::sdk::IStringMap* sourceMap, int /*roiIndex*/)
{
    using namespace basicServerSettingsIo;
    deserializeOrThrow(value(sourceMap, KeyIndex::enabled), &enabled);
    deserializeOrThrow(value(sourceMap, KeyIndex::noisefilter), &noisefilter);
    deserializeOrThrow(value(sourceMap, KeyIndex::thresholdLevel), &thresholdLevel);
    deserializeOrThrow(value(sourceMap, KeyIndex::scream), &scream);
    deserializeOrThrow(value(sourceMap, KeyIndex::gunshot), &gunshot);
    deserializeOrThrow(value(sourceMap, KeyIndex::explosion), &explosion);
    deserializeOrThrow(value(sourceMap, KeyIndex::crashingGlass), &crashingGlass);
    initialized = true;
}

void SoundClassification::writeToServer(nx::sdk::SettingsResponse* result, int /*roiIndex*/) const
{
    using namespace basicServerSettingsIo;
    result->setValue(key(KeyIndex::enabled), serialize(enabled));
    result->setValue(key(KeyIndex::noisefilter), serialize(noisefilter));
    result->setValue(key(KeyIndex::thresholdLevel), serialize(thresholdLevel));
    result->setValue(key(KeyIndex::scream), serialize(scream));
    result->setValue(key(KeyIndex::gunshot), serialize(gunshot));
    result->setValue(key(KeyIndex::explosion), serialize(explosion));
    result->setValue(key(KeyIndex::crashingGlass), serialize(crashingGlass));
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
            << "msubmenu=" << kSunapiEventName
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
