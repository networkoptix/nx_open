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

const char* upperIfBoolean(const char* value)
{
    if (!value)
        return nullptr;
    if (strcmp(value, "false") == 0)
        return "False";
    if (strcmp(value, "true") == 0)
        return "True";
    return value;
}

std::string specify(const char* v, unsigned int n)
{
    NX_ASSERT(v);
    std::string result(v);
    if (n == 0)
        return result;

    const std::string nAsString =
        static_cast<const std::stringstream&>((std::stringstream() << n)).str();
    result.replace(result.find("#"), 1, nAsString);

    return result;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
struct PluginValueError {};
struct SunapiValueError {};

void ReadOrThrow(const char* value, bool* result)
{
    if(!value)
        throw PluginValueError();

    if (strcmp(value, "true") == 0)
    {
        *result = true;
        return;
    }
    if (strcmp(value, "false") == 0)
    {
        *result = false;
        return;
    }

    throw PluginValueError();
}

void ReadOrThrow(const char* value, int* result)
{
    if (!value)
        throw PluginValueError();

    const char* end = value + strlen(value);
    std::from_chars_result conversionResult = std::from_chars(value, end, *result);
    if (conversionResult.ptr == end)
        return;

    throw PluginValueError();
}

void ReadOrThrow(const char* value, std::string* result)
{
    if (!value)
        throw PluginValueError();

    *result = value;
    if (result->size() >= 2 && result->front() == '"' && result->back() == '"')
    {
        *result = result->substr(1, result->size() - 2);
        return;
    }

    throw PluginValueError();
}

void ReadOrThrow(const char* value, std::vector<PluginPoint>* result)
{
    if (!value)
        throw PluginValueError();

    std::optional<std::vector<PluginPoint>> tmp = ServerStringToPluginPoints(value);
    if (tmp)
    {
        *result = *tmp;
        return;
    }

    throw PluginValueError();
}

void ReadOrThrow(const char* value, Direction* result)
{
    if (!value)
        throw PluginValueError();

    std::optional<Direction> tmp = ServerStringToDirection(value);
    if (tmp)
    {
        *result = *tmp;
        return;
    }

    throw PluginValueError();
}

void ReadOrThrow(const char* value, Width* result)
{
    if (!value)
        throw PluginValueError();

    std::optional<Width> tmp = ServerStringToWidth(value);
    if (tmp)
    {
        *result = *tmp;
        return;
    }

    throw PluginValueError();
}

void ReadOrThrow(const char* value, Height* result)
{
    if (!value)
        throw PluginValueError();

    std::optional<Height> tmp = ServerStringToHeight(value);
    if (tmp)
    {
        *result = *tmp;
        return;
    }

    throw PluginValueError();
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void ReadOrThrow2(const nx::kit::Json& json, const char* key, bool* result)
{
    NX_ASSERT(key);

    if (const auto& param = json[key]; param.is_bool())
        *result = param.bool_value();
    else
        throw SunapiValueError{};
}

void ReadOrThrow2(const nx::kit::Json& json, const char* key, int* result)
{
    NX_ASSERT(key);

    if (const auto& param = json[key]; param.is_number())
        *result = param.int_value();
    else
        throw SunapiValueError{};
}

void ReadOrThrow2(const nx::kit::Json& json, const char* key, std::string* result)
{
    NX_ASSERT(key);

    if (const auto& param = json[key]; param.is_string())
        *result = param.string_value();
    else
        throw SunapiValueError{};
}

void ReadOrThrow2(const nx::kit::Json& json, const char* key, PluginPoint* result)
{
    NX_ASSERT(key);

    const auto& param = json[key];
    if (!param.is_string())
        throw SunapiValueError{};

    const std::string value = param.string_value();

    FrameSize frameSize{ 3840, 2160 };
    if (!result->fromSunapiString(value, frameSize))
        throw SunapiValueError();
}

void ReadOrThrow2(const nx::kit::Json& json, const char* key, Width* result)
{
    PluginPoint point;
    ReadOrThrow2(json, key, &point);
    *result = point.x;
}

void ReadOrThrow2(const nx::kit::Json& json, const char* key, Height* result)
{
    PluginPoint point;
    ReadOrThrow2(json, key, &point);
    *result = point.y;
}

void ReadOrThrow2(const nx::kit::Json& json, const char* key, std::vector<PluginPoint>* result)
{
    NX_ASSERT(key);
    FrameSize frameSize{ 3840, 2160 };

    const auto& points = json[key];
    if (!points.is_array())
        throw SunapiValueError{};

    result->reserve(points.array_items().size());
    for (const nx::kit::Json& point: points.array_items())
    {
        if (!point["x"].is_number() || !point["y"].is_number())
            throw SunapiValueError{};

        const int ix = point["x"].int_value();
        const int iy = point["y"].int_value();
        result->emplace_back(frameSize.xAbsoluteToRelative(ix), frameSize.yAbsoluteToRelative(iy));
    }
}

void ReadOrThrow2(const nx::kit::Json& json, const char* key, Direction* result)
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
            throw SunapiValueError{}; //< unknown direction
    }
    else
        throw SunapiValueError{};
}

void ReadOrThrow2(const nx::kit::Json& json, const char* key, bool* result, const char* desired)
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
        throw SunapiValueError{};
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

#define NX_READ_SETTING_OR_THROW(TMP_OBJECT, FIELD_NAME) ReadOrThrow( \
            settings->value(specify(kServerParamsNames[(int)ServerParamIndex::FIELD_NAME], \
            objectIndex).c_str()), &TMP_OBJECT.FIELD_NAME)

//-------------------------------------------------------------------------------------------------

bool ShockDetection::operator==(const ShockDetection& rhs) const
{
    return
        initialized == rhs.initialized
        && enabled == rhs.enabled
        && thresholdLevel == rhs.thresholdLevel
        && sensitivityLevel == rhs.sensitivityLevel
        ;
}

bool ShockDetection::loadFromServer(const nx::sdk::IStringMap* settings, int objectIndex)
{
    std::decay_t<decltype(*this)> tmp;
    try
    {
        NX_READ_SETTING_OR_THROW(tmp, enabled);
        NX_READ_SETTING_OR_THROW(tmp, thresholdLevel);
        NX_READ_SETTING_OR_THROW(tmp, sensitivityLevel);
    }
    catch (PluginValueError&)
    {
        return false;
    }
    *this = std::move(tmp);
    this->initialized = true;
    return true;

}

nx::kit::Json getChannelOrThrow(
    const std::string& cameraReply, const char* eventName, int channelNumber)
{
    std::string err;
    nx::kit::Json json = nx::kit::Json::parse(cameraReply, err);
    if (!json.is_object())
        throw SunapiValueError{};

    const nx::kit::Json& jsonChannels = json[eventName];
    if (!jsonChannels.is_array())
        throw SunapiValueError{};

    for (const auto& channel: jsonChannels.array_items())
    {
        if (const auto& j = channel["Channel"]; j.is_number() && j.int_value() == channelNumber)
            return channel;
    }
    throw SunapiValueError{};
}

bool ShockDetection::loadFromSunapi(const std::string& cameraReply, int channelNumber)
{
    std::decay_t<decltype(*this)> tmp;
    try
    {
        nx::kit::Json channel = getChannelOrThrow(cameraReply, "ShockDetection", channelNumber);

        ReadOrThrow2(channel, "Enable", &tmp.enabled);
        ReadOrThrow2(channel, "ThresholdLevel", &tmp.thresholdLevel);
        ReadOrThrow2(channel, "Sensitivity", &tmp.sensitivityLevel);
    }
    catch (SunapiValueError&)
    {
        return false;
    }
    *this = std::move(tmp);
    return true;
}

void ShockDetection::replanishErrorMap(
    nx::sdk::Ptr<nx::sdk::StringMap>& errorMap, const std::string& reason) const
{
    for (const auto paramName : kServerParamsNames)
        errorMap->setItem(paramName, reason);
}

std::string buildCameraRequestQuery(
    const ShockDetection& settingGroup, FrameSize frameSize, int channelNumber)
{
    std::ostringstream query;
    if (settingGroup)
    {
        query
            << "msubmenu=" << "shockdetection"
            << "&action=" << "set"
            << "&Channel=" << channelNumber
            << "&Enable=" << buildBool(settingGroup.enabled)
            << "&ThresholdLevel=" << settingGroup.thresholdLevel
            << "&Sensitivity=" << settingGroup.sensitivityLevel
            ;
    }
    return query.str();
}

//-------------------------------------------------------------------------------------------------

bool Motion::operator==(const Motion& rhs) const
{
    return
        initialized == rhs.initialized

        && detectionType == rhs.detectionType
        ;
}

bool Motion::loadFromServer(const nx::sdk::IStringMap* settings, int objectIndex)
{
    std::decay_t<decltype(*this)> tmp;
    try
    {
        NX_READ_SETTING_OR_THROW(tmp, detectionType);
    }
    catch (PluginValueError&)
    {
        return false;
    }
    *this = std::move(tmp);
    this->initialized = true;
    return true;

}

bool Motion::loadFromSunapi(const std::string& cameraReply, int channelNumber)
{
    std::decay_t<decltype(*this)> tmp;
    try
    {
        nx::kit::Json channel = getChannelOrThrow(cameraReply, "VideoAnalysis", channelNumber);

        ReadOrThrow2(channel, "DetectionType", &tmp.detectionType);
    }
    catch (SunapiValueError&)
    {
        return false;
    }
    *this = std::move(tmp);
    return true;
}

void Motion::replanishErrorMap(
    nx::sdk::Ptr<nx::sdk::StringMap>& errorMap, const std::string& reason) const
{
    for (const auto paramName: kServerParamsNames)
        errorMap->setItem(paramName, reason);
}

std::string buildCameraRequestQuery(
    const Motion& settingGroup, FrameSize frameSize, int channelNumber)
{
    std::ostringstream query;
    if (settingGroup)
    {
        query
            << "msubmenu=" << "videoanalysis2"
            << "&action=" << "set"
            << "&Channel=" << channelNumber
            << "&DetectionType=" << settingGroup.detectionType
            ;
    }
    return query.str();
}

//-------------------------------------------------------------------------------------------------

bool MdObjectSize::operator==(const MdObjectSize& rhs) const
{
    return
        initialized == rhs.initialized

        && minWidth == rhs.minWidth
        && minHeight == rhs.minHeight
        && maxWidth == rhs.maxWidth
        && maxHeight == rhs.maxHeight
        ;
}

bool MdObjectSize::loadFromServer(const nx::sdk::IStringMap* settings, int objectIndex)
{
    std::decay_t<decltype(*this)> tmp;
    try
    {
        NX_READ_SETTING_OR_THROW(tmp, minWidth);
        NX_READ_SETTING_OR_THROW(tmp, minHeight);
        NX_READ_SETTING_OR_THROW(tmp, maxWidth);
        NX_READ_SETTING_OR_THROW(tmp, maxHeight);
    }
    catch (PluginValueError&)
    {
        return false;
    }
    *this = std::move(tmp);
    this->initialized = true;
    return true;
}

nx::kit::Json getObjectSizeJsonObject(const nx::kit::Json& eventJsonObject,
    const std::string& detectionTypeValue)
{
    nx::kit::Json result;
    const nx::kit::Json& objectSizeList = eventJsonObject["ObjectSizeByDetectionTypes"];
    if (!objectSizeList.is_array())
        return result;

    for (const nx::kit::Json& objectSize: objectSizeList.array_items())
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

bool MdObjectSize::loadFromSunapi(const std::string& cameraReply, int channelNumber)
{
    std::decay_t<decltype(*this)> tmp;
    try
    {
        nx::kit::Json channel = getChannelOrThrow(cameraReply, "VideoAnalysis", channelNumber);

        nx::kit::Json objectSizeObject = getObjectSizeJsonObject(channel, "MotionDetection");

        ReadOrThrow2(objectSizeObject, "MinimumObjectSizeInPixels", &tmp.minWidth);
        ReadOrThrow2(objectSizeObject, "MinimumObjectSizeInPixels", &tmp.minHeight);
        ReadOrThrow2(objectSizeObject, "MaximumObjectSizeInPixels", &tmp.maxWidth);
        ReadOrThrow2(objectSizeObject, "MaximumObjectSizeInPixels", &tmp.maxHeight);
    }
    catch (SunapiValueError&)
    {
        return false;
    }
    *this = std::move(tmp);
    return true;
}

void MdObjectSize::replanishErrorMap(
    nx::sdk::Ptr<nx::sdk::StringMap>& errorMap, const std::string& reason) const
{
    for (const auto paramName: kServerParamsNames)
        errorMap->setItem(paramName, reason);
}

std::string buildMinObjectSize(const MdObjectSize& settingGroup, FrameSize frameSize)
{
    std::stringstream stream;
    stream
        << frameSize.xRelativeToAbsolute(settingGroup.minWidth)
        << ','
        << frameSize.yRelativeToAbsolute(settingGroup.minHeight);
    return stream.str();
}

std::string buildMaxObjectSize(const MdObjectSize& settingGroup, FrameSize frameSize)
{
    std::stringstream stream;
    stream
        << frameSize.xRelativeToAbsolute(settingGroup.maxWidth)
        << ','
        << frameSize.yRelativeToAbsolute(settingGroup.maxHeight);
    return stream.str();
}

std::string buildCameraRequestQuery(
    const MdObjectSize& settingGroup, FrameSize frameSize, int channelNumber)
{
    std::ostringstream query;
    if (settingGroup)
    {
        query
            << "msubmenu=" << "videoanalysis2"
            << "&action=" << "set"
            << "&Channel=" << channelNumber
            << "&DetectionType.MotionDetection.MinimumObjectSizeInPixels="
                << buildMinObjectSize(settingGroup, frameSize)
            << "&DetectionType.MotionDetection.MaximumObjectSizeInPixels="
                << buildMaxObjectSize(settingGroup, frameSize)
            ;
    }
    return query.str();
}

//-------------------------------------------------------------------------------------------------

bool IvaObjectSize::operator==(const IvaObjectSize& rhs) const
{
    return
        initialized == rhs.initialized

        && minWidth == rhs.minWidth
        && minHeight == rhs.minHeight
        && maxWidth == rhs.maxWidth
        && maxHeight == rhs.maxHeight
        ;
}

bool IvaObjectSize::loadFromServer(const nx::sdk::IStringMap* settings, int objectIndex)
{
    std::decay_t<decltype(*this)> tmp;
    try
    {
        NX_READ_SETTING_OR_THROW(tmp, minWidth);
        NX_READ_SETTING_OR_THROW(tmp, minHeight);
        NX_READ_SETTING_OR_THROW(tmp, maxWidth);
        NX_READ_SETTING_OR_THROW(tmp, maxHeight);
    }
    catch (PluginValueError&)
    {
        return false;
    }
    *this = std::move(tmp);
    this->initialized = true;
    return true;
}

bool IvaObjectSize::loadFromSunapi(const std::string& cameraReply, int channelNumber)
{
    std::decay_t<decltype(*this)> tmp;
    try
    {
        nx::kit::Json channel = getChannelOrThrow(cameraReply, "VideoAnalysis", channelNumber);

        nx::kit::Json objectSizeObject = getObjectSizeJsonObject(channel, "IntelligentVideo");

        ReadOrThrow2(objectSizeObject, "MinimumObjectSizeInPixels", &tmp.minWidth);
        ReadOrThrow2(objectSizeObject, "MinimumObjectSizeInPixels", &tmp.minHeight);
        ReadOrThrow2(objectSizeObject, "MaximumObjectSizeInPixels", &tmp.maxWidth);
        ReadOrThrow2(objectSizeObject, "MaximumObjectSizeInPixels", &tmp.maxHeight);
    }
    catch (SunapiValueError&)
    {
        return false;
    }
    *this = std::move(tmp);
    return true;
}

void IvaObjectSize::replanishErrorMap(
    nx::sdk::Ptr<nx::sdk::StringMap>& errorMap, const std::string& reason) const
{
    for (const auto paramName : kServerParamsNames)
        errorMap->setItem(paramName, reason);
}

std::string buildMinObjectSize(const IvaObjectSize& settingGroup, FrameSize frameSize)
{
    std::stringstream stream;
    stream
        << frameSize.xRelativeToAbsolute(settingGroup.minWidth)
        << ','
        << frameSize.yRelativeToAbsolute(settingGroup.minHeight);
    return stream.str();
}

std::string buildMaxObjectSize(const IvaObjectSize& settingGroup, FrameSize frameSize)
{
    std::stringstream stream;
    stream
        << frameSize.xRelativeToAbsolute(settingGroup.maxWidth)
        << ','
        << frameSize.yRelativeToAbsolute(settingGroup.maxHeight);
    return stream.str();
}

std::string buildCameraRequestQuery(
    const IvaObjectSize& settingGroup, FrameSize frameSize, int channelNumber)
{
    std::ostringstream query;
    if (settingGroup)
    {
        query
            << "msubmenu=" << "videoanalysis2"
            << "&action=" << "set"
            << "&Channel=" << channelNumber
            << "&DetectionType.IntelligentVideo.MinimumObjectSizeInPixels="
                << buildMinObjectSize(settingGroup, frameSize)
            << "&DetectionType.IntelligentVideo.MaximumObjectSizeInPixels="
                << buildMaxObjectSize(settingGroup, frameSize)
            ;
    }
    return query.str();
}

//-------------------------------------------------------------------------------------------------

bool MdIncludeArea::operator==(const MdIncludeArea& rhs) const
{
    return
        initialized == rhs.initialized

        && points == rhs.points
        && thresholdLevel == rhs.thresholdLevel
        && sensitivityLevel == rhs.sensitivityLevel
        && minimumDuration == rhs.minimumDuration
        ;
}

bool MdIncludeArea::differesEnoughFrom(const MdIncludeArea& rhs) const
{
    if (this->points.empty() && rhs.points.empty())
        return false;

    return *this != rhs;
}

bool MdIncludeArea::loadFromServer(const nx::sdk::IStringMap* settings, int objectIndex)
{
    std::decay_t<decltype(*this)> tmp;
    try
    {
        NX_READ_SETTING_OR_THROW(tmp, points);
        NX_READ_SETTING_OR_THROW(tmp, thresholdLevel);
        NX_READ_SETTING_OR_THROW(tmp, sensitivityLevel);
        NX_READ_SETTING_OR_THROW(tmp, minimumDuration);
    }
    catch (PluginValueError&)
    {
        return false;
    }
    *this = std::move(tmp);
    this->internalObjectIndex = objectIndex;
    this->initialized = true;
    return true;
}

/** returns empty json object if no object with `internalIndex` found*/
nx::kit::Json getMdRoiJsonObject(nx::kit::Json channelData, int internalIndex)
{
    nx::kit::Json result;
    const nx::kit::Json& Lines = channelData["ROIs"];
    if (!Lines.is_array())
        return result;

    for (const nx::kit::Json& Line: Lines.array_items())
    {
        const nx::kit::Json& roiIndex = Line["ROI"];
        if (roiIndex.is_number() && roiIndex.int_value() == internalIndex)
        {
            result = Line;
            return result;
        }
    }
    return result;;
}

nx::kit::Json getLineJsonObject(nx::kit::Json channelData, int internalIndex)
{
    nx::kit::Json result;
    const nx::kit::Json& Lines = channelData["Lines"];
    if (!Lines.is_array())
        return result;

    for (const nx::kit::Json& Line: Lines.array_items())
    {
        const nx::kit::Json& roiIndex = Line["Line"];
        if (roiIndex.is_number() && roiIndex.int_value() == internalIndex)
        {
            result = Line;
            return result;
        }
    }
    return result;;
}

nx::kit::Json getIvaRoiJsonObject(nx::kit::Json channelData, int internalIndex)
{
    nx::kit::Json result;
    const nx::kit::Json& Lines = channelData["DefinedAreas"];
    if (!Lines.is_array())
        return result;

    for (const nx::kit::Json& Line : Lines.array_items())
    {
        const nx::kit::Json& roiIndex = Line["DefinedArea"];
        if (roiIndex.is_number() && roiIndex.int_value() == internalIndex)
        {
            result = Line;
            return result;
        }
    }
    return result;;
}

nx::kit::Json getOdRoiJsonObject(nx::kit::Json channelData, int internalIndex)
{
    nx::kit::Json result;
    const nx::kit::Json& Areas = channelData["ExcludeAreas"];
    if (!Areas.is_array())
        return result;

    for (const nx::kit::Json& Area : Areas.array_items())
    {
        const nx::kit::Json& roiIndex = Area["ExcludeArea"];
        if (roiIndex.is_number() && roiIndex.int_value() == internalIndex)
        {
            result = Area;
            return result;
        }
    }
    return result;;
}

bool MdIncludeArea::loadFromSunapi(
    const std::string& cameraReply, int channelNumber, int objectIndex)
{
    std::decay_t<decltype(*this)> tmp;
    try
    {
        nx::kit::Json channel = getChannelOrThrow(cameraReply, "VideoAnalysis", channelNumber);

        nx::kit::Json roiObject = getMdRoiJsonObject(channel, objectIndex);
        if (roiObject == nx::kit::Json())
        {
            *this = std::move(tmp); // reset value;
            return true;
        }

        ReadOrThrow2(roiObject, "Coordinates", &tmp.points);
        ReadOrThrow2(roiObject, "ThresholdLevel", &tmp.thresholdLevel);
        ReadOrThrow2(roiObject, "SensitivityLevel", &tmp.sensitivityLevel);
        ReadOrThrow2(roiObject, "Duration", &tmp.minimumDuration);
    }
    catch (SunapiValueError&)
    {
        return false;
    }
    *this = std::move(tmp);
    return true;
}

void MdIncludeArea::replanishErrorMap(
    nx::sdk::Ptr<nx::sdk::StringMap>& errorMap, const std::string& reason) const
{
    for (const auto paramName: kServerParamsNames)
        errorMap->setItem(paramName, reason);
}

std::string buildCameraRequestQuery(
    const MdIncludeArea& area, FrameSize frameSize, int channelNumber)
{
    std::ostringstream query;
    if (area)
    {
        if (!area.points.empty())
        {
            const std::string prefix = "&ROI."s + std::to_string(area.internalObjectIndex);
            query
                << "msubmenu=" << "videoanalysis2"
                << "&action=" << "set"
                << "&Channel=" << channelNumber
                << prefix << ".Coordinate=" << pluginPointsToSunapiString(area.points, frameSize)
                << prefix << ".ThresholdLevel=" << area.thresholdLevel
                << prefix << ".SensitivityLevel=" << area.sensitivityLevel
                << prefix << ".Duration=" << area.minimumDuration
                ;
        }
        else
        {
            query
                << "msubmenu=" << "videoanalysis2"
                << "&action=" << "remove"
                << "&Channel=" << channelNumber
                << "&ROIIndex=" << area.internalObjectIndex;
        }
    }
    return query.str();

}
//-------------------------------------------------------------------------------------------------
bool MdExcludeArea::loadFromServer(const nx::sdk::IStringMap* settings, int objectIndex)
{
    std::decay_t<decltype(*this)> tmp;
    try
    {
        NX_READ_SETTING_OR_THROW(tmp, points);
    }
    catch (PluginValueError&)
    {
        return false;
    }
    *this = std::move(tmp);
    this->internalObjectIndex = objectIndex + 8;
    this->initialized = true;
    return true;
}

bool MdExcludeArea::loadFromSunapi(
    const std::string& cameraReply, int channelNumber, int objectIndex)
{
    std::decay_t<decltype(*this)> tmp;

    try
    {
        nx::kit::Json channel = getChannelOrThrow(cameraReply, "VideoAnalysis", channelNumber);

        nx::kit::Json roiObject = getMdRoiJsonObject(channel, objectIndex + 8);
        if (roiObject == nx::kit::Json())
        {
            *this = std::move(tmp); // reset value;
            return true;
        }

        ReadOrThrow2(roiObject, "Coordinates", &tmp.points);
    }
    catch (SunapiValueError&)
    {
        return false;
    }
    *this = std::move(tmp);
    return true;
}

bool MdExcludeArea::operator==(const MdExcludeArea& rhs) const
{
    return
        initialized == rhs.initialized
        && internalObjectIndex == rhs.internalObjectIndex

        && points == rhs.points
        ;
}
bool MdExcludeArea::differesEnoughFrom(const MdExcludeArea& rhs) const
{
    if (this->points.empty() && rhs.points.empty())
        return false;

    return *this != rhs;
}

void MdExcludeArea::replanishErrorMap(
    nx::sdk::Ptr<nx::sdk::StringMap>& errorMap, const std::string& reason) const
{
    for (const auto paramName : kServerParamsNames)
        errorMap->setItem(paramName, reason);
}

std::string buildCameraRequestQuery(
    const MdExcludeArea& area, FrameSize frameSize, int channelNumber)
{
    std::ostringstream query;
    if (area)
    {
        if (!area.points.empty())
        {
            const std::string prefix = "&ROI."s + std::to_string(area.internalObjectIndex);
            query
                << "msubmenu=" << "videoanalysis2"
                << "&action=" << "set"
                << "&Channel=" << channelNumber
                << prefix << ".Coordinate=" << pluginPointsToSunapiString(area.points, frameSize)
                ;
        }
        else
        {
            query
                << "msubmenu=" << "videoanalysis2"
                << "&action=" << "remove"
                << "&Channel=" << channelNumber
                << "&ROIIndex=" << area.internalObjectIndex
                ;
        }
    }
    return query.str();
}

//-------------------------------------------------------------------------------------------------

bool TamperingDetection::operator==(const TamperingDetection& rhs) const
{
    return
        initialized == rhs.initialized

        && enabled == rhs.enabled
        && thresholdLevel == rhs.thresholdLevel
        && sensitivityLevel == rhs.sensitivityLevel
        && minimumDuration == rhs.minimumDuration
        && exceptDarkImages == rhs.exceptDarkImages
        ;
}

bool TamperingDetection::loadFromServer(const nx::sdk::IStringMap* settings, int objectIndex)
{
    std::decay_t<decltype(*this)> tmp;
    try
    {
        NX_READ_SETTING_OR_THROW(tmp, enabled);
        NX_READ_SETTING_OR_THROW(tmp, thresholdLevel);
        NX_READ_SETTING_OR_THROW(tmp, sensitivityLevel);
        NX_READ_SETTING_OR_THROW(tmp, minimumDuration);
        NX_READ_SETTING_OR_THROW(tmp, exceptDarkImages);
    }
    catch (PluginValueError&)
    {
        return false;
    }
    *this = std::move(tmp);
    this->initialized = true;
    return true;
}

bool TamperingDetection::loadFromSunapi(const std::string& cameraReply, int channelNumber)
{
    std::decay_t<decltype(*this)> tmp;
    try
    {
        nx::kit::Json channel = getChannelOrThrow(cameraReply, "TamperingDetection", channelNumber);

        ReadOrThrow2(channel, "Enable", &tmp.enabled);
        ReadOrThrow2(channel, "ThresholdLevel", &tmp.thresholdLevel);
        ReadOrThrow2(channel, "SensitivityLevel", &tmp.sensitivityLevel);
        ReadOrThrow2(channel, "Duration", &tmp.minimumDuration);
        ReadOrThrow2(channel, "DarknessDetection", &tmp.exceptDarkImages);
    }
    catch (SunapiValueError&)
    {
        return false;
    }
    *this = std::move(tmp);
    return true;
}

void TamperingDetection::replanishErrorMap(
    nx::sdk::Ptr<nx::sdk::StringMap>& errorMap, const std::string& reason) const
{
    for (const auto paramName: kServerParamsNames)
        errorMap->setItem(paramName, reason);

}

std::string buildCameraRequestQuery(
    const TamperingDetection& settingGroup, FrameSize frameSize, int channelNumber)
{
    std::ostringstream query;
    if (settingGroup)
    {
        query
            << "msubmenu=" << "tamperingdetection"
            << "&action=" << "set"
            << "&Channel=" << channelNumber
            << "&Enable=" << buildBool(settingGroup.enabled)
            << "&ThresholdLevel=" << settingGroup.thresholdLevel
            << "&SensitivityLevel=" << settingGroup.sensitivityLevel
            << "&Duration=" << settingGroup.minimumDuration
            << "&DarknessDetection=" << buildBool(settingGroup.exceptDarkImages)
            ;
    }
    return query.str();
}

//-------------------------------------------------------------------------------------------------
bool DefocusDetection::operator==(const DefocusDetection& rhs) const
{
    return
        initialized == rhs.initialized

        && enabled == rhs.enabled
        && thresholdLevel == rhs.thresholdLevel
        && sensitivityLevel == rhs.sensitivityLevel
        && minimumDuration == rhs.minimumDuration
        ;
}

bool DefocusDetection::loadFromServer(const nx::sdk::IStringMap* settings, int objectIndex)
{
    std::decay_t<decltype(*this)> tmp;
    try
    {
        NX_READ_SETTING_OR_THROW(tmp, enabled);
        NX_READ_SETTING_OR_THROW(tmp, thresholdLevel);
        NX_READ_SETTING_OR_THROW(tmp, sensitivityLevel);
        NX_READ_SETTING_OR_THROW(tmp, minimumDuration);
    }
    catch (PluginValueError&)
    {
        return false;
    }
    *this = std::move(tmp);
    this->initialized = true;
    return true;
}

bool DefocusDetection::loadFromSunapi(const std::string& cameraReply, int channelNumber)
{
    std::decay_t<decltype(*this)> tmp;
    try
    {
        nx::kit::Json channel = getChannelOrThrow(cameraReply, "DefocusDetection", channelNumber);

        ReadOrThrow2(channel, "Enable", &tmp.enabled);
        ReadOrThrow2(channel, "ThresholdLevel", &tmp.thresholdLevel);
        ReadOrThrow2(channel, "Sensitivity", &tmp.sensitivityLevel);
        ReadOrThrow2(channel, "Duration", &tmp.minimumDuration);
    }
    catch (SunapiValueError&)
    {
        return false;
    }
    *this = std::move(tmp);
    return true;
}

void DefocusDetection::replanishErrorMap(
    nx::sdk::Ptr<nx::sdk::StringMap>& errorMap, const std::string& reason) const
{
    for (const auto paramName: kServerParamsNames)
        errorMap->setItem(paramName, reason);
}

std::string buildCameraRequestQuery(
    const DefocusDetection& settingGroup, FrameSize frameSize, int channelNumber)
{
    std::ostringstream query;
    if (settingGroup)
    {
        query
            << "msubmenu=" << "defocusdetection"
            << "&action=" << "set"
            << "&Channel=" << channelNumber
            << "&Enable=" << buildBool(settingGroup.enabled)
            << "&ThresholdLevel=" << settingGroup.thresholdLevel
            << "&Sensitivity=" << settingGroup.sensitivityLevel
            << "&Duration=" << settingGroup.minimumDuration
            ;
    }
    return query.str();
}

//-------------------------------------------------------------------------------------------------

bool OdObjects::operator==(const OdObjects& rhs) const
{
    return
        initialized == rhs.initialized

        && enabled == rhs.enabled
        && person == rhs.person
        && vehicle == rhs.vehicle
        && face == rhs.face
        && licensePlate == rhs.licensePlate
        && minimumDuration == rhs.minimumDuration
        ;
}

bool OdObjects::loadFromServer(const nx::sdk::IStringMap* settings, int objectIndex)
{
    std::decay_t<decltype(*this)> tmp;
    try
    {
        NX_READ_SETTING_OR_THROW(tmp, enabled);
        NX_READ_SETTING_OR_THROW(tmp, person);
        NX_READ_SETTING_OR_THROW(tmp, vehicle);
        NX_READ_SETTING_OR_THROW(tmp, face);
        NX_READ_SETTING_OR_THROW(tmp, licensePlate);
        NX_READ_SETTING_OR_THROW(tmp, minimumDuration);
    }
    catch (PluginValueError&)
    {
        return false;
    }
    *this = std::move(tmp);
    this->initialized = true;
    return true;
}

bool OdObjects::loadFromSunapi(const std::string& cameraReply, int channelNumber)
{
    std::decay_t<decltype(*this)> tmp;
    try
    {
        nx::kit::Json channel = getChannelOrThrow(cameraReply, "ObjectDetection", channelNumber);

        ReadOrThrow2(channel, "Enable", &tmp.enabled);
        ReadOrThrow2(channel, "ObjectTypes", &tmp.person, "Person");
        ReadOrThrow2(channel, "ObjectTypes", &tmp.vehicle, "Vehicle");
        ReadOrThrow2(channel, "ObjectTypes", &tmp.face, "Face");
        ReadOrThrow2(channel, "ObjectTypes", &tmp.licensePlate, "LicensePlate");
        ReadOrThrow2(channel, "Duration", &tmp.minimumDuration);
    }
    catch (SunapiValueError&)
    {
        return false;
    }
    *this = std::move(tmp);
    return true;
}

void OdObjects::replanishErrorMap(
    nx::sdk::Ptr<nx::sdk::StringMap>& errorMap, const std::string& reason) const
{
    for (const auto paramName : kServerParamsNames)
        errorMap->setItem(paramName, reason);
}

std::string buildObjectTypes(const OdObjects& settingGroup)
{
    std::vector<const char*> mode;

    if (settingGroup.person)
        mode.push_back("Person");

    if (settingGroup.vehicle)
        mode.push_back("Vehicle");

    if (settingGroup.face)
        mode.push_back("Face");

    if (settingGroup.licensePlate)
        mode.push_back("LicensePlate");

    return concat(mode);
}

std::string buildCameraRequestQuery(
    const OdObjects& settingGroup, FrameSize /*frameSize*/, int channelNumber)
{
    std::ostringstream query;
    if (settingGroup)
    {
        query
            << "msubmenu=" << "objectdetection"
            << "&action=" << "set"
            << "&Channel=" << channelNumber
            << "&Enable=" << buildBool(settingGroup.enabled)
            << "&ObjectTypes=" << buildObjectTypes(settingGroup)
            << "&Duration=" << settingGroup.minimumDuration
            ;
    }
    return query.str();
}

//-------------------------------------------------------------------------------------------------

bool OdBestShot::operator==(const OdBestShot& rhs) const
{
    return
        initialized == rhs.initialized

        && person == rhs.person
        && vehicle == rhs.vehicle
        && face == rhs.face
        && licensePlate == rhs.licensePlate
        ;
}

bool OdBestShot::loadFromServer(const nx::sdk::IStringMap* settings, int objectIndex)
{
    std::decay_t<decltype(*this)> tmp;
    try
    {
        NX_READ_SETTING_OR_THROW(tmp, person);
        NX_READ_SETTING_OR_THROW(tmp, vehicle);
        NX_READ_SETTING_OR_THROW(tmp, face);
        NX_READ_SETTING_OR_THROW(tmp, licensePlate);
    }
    catch (PluginValueError&)
    {
        return false;
    }
    *this = std::move(tmp);
    this->initialized = true;
    return true;
}

bool OdBestShot::loadFromSunapi(const std::string& cameraReply, int channelNumber)
{
    std::decay_t<decltype(*this)> tmp;
    try
    {
        nx::kit::Json channel = getChannelOrThrow(cameraReply, "ObjectDetection", channelNumber);
        // This doesn't work in the PNO-A0981R now (bug in the camera).
        ReadOrThrow2(channel, "BestShot", &tmp.person, "Person");
        ReadOrThrow2(channel, "BestShot", &tmp.vehicle, "Vehicle");
        ReadOrThrow2(channel, "BestShot", &tmp.face, "Face");
        ReadOrThrow2(channel, "BestShot", &tmp.licensePlate, "LicensePlate");
    }
    catch (SunapiValueError&)
    {
        return false;
    }
    *this = std::move(tmp);
    return true;
}

void OdBestShot::replanishErrorMap(
    nx::sdk::Ptr<nx::sdk::StringMap>& errorMap, const std::string& reason) const
{
    for (const auto paramName : kServerParamsNames)
        errorMap->setItem(paramName, reason);
}

std::string buildObjectTypes(const OdBestShot& settingGroup)
{
    std::vector<const char*> mode;

    if (settingGroup.person)
        mode.push_back("Person");

    if (settingGroup.vehicle)
        mode.push_back("Vehicle");

    if (settingGroup.face)
        mode.push_back("Face");

    if (settingGroup.licensePlate)
        mode.push_back("LicensePlate");

    return concat(mode);
}

std::string buildCameraRequestQuery(
    const OdBestShot& settingGroup, FrameSize /*frameSize*/, int channelNumber)
{
    std::ostringstream query;
    if (settingGroup)
    {
        query
            << "msubmenu=" << "metaimagetransfer"
            << "&action=" << "set"
            << "&Channel=" << channelNumber
            << "&ObjectTypes=" << buildObjectTypes(settingGroup)
            ;
    }
    return query.str();
}

//-------------------------------------------------------------------------------------------------

bool OdExcludeArea::loadFromServer(const nx::sdk::IStringMap* settings, int objectIndex)
{
    std::decay_t<decltype(*this)> tmp;
    try
    {
        NX_READ_SETTING_OR_THROW(tmp, points);
    }
    catch (PluginValueError&)
    {
        return false;
    }
    *this = std::move(tmp);
    this->internalObjectIndex = objectIndex;
    this->initialized = true;
    return true;
}

bool OdExcludeArea::loadFromSunapi(
    const std::string& cameraReply, int channelNumber, int objectIndex)
{
    std::decay_t<decltype(*this)> tmp;

    try
    {
        nx::kit::Json channel = getChannelOrThrow(cameraReply, "ObjectDetection", channelNumber);

        nx::kit::Json areObject = getOdRoiJsonObject(channel, objectIndex);
        if (areObject == nx::kit::Json())
        {
            *this = std::move(tmp); // reset value;
            return true;
        }

        ReadOrThrow2(areObject, "Coordinates", &tmp.points);
    }
    catch (SunapiValueError&)
    {
        return false;
    }
    *this = std::move(tmp);
    return true;
}

bool OdExcludeArea::operator==(const OdExcludeArea& rhs) const
{
    return
        initialized == rhs.initialized
        && internalObjectIndex == rhs.internalObjectIndex

        && points == rhs.points
        ;
}
bool OdExcludeArea::differesEnoughFrom(const OdExcludeArea& rhs) const
{
    if (this->points.empty() && rhs.points.empty())
        return false;

    return *this != rhs;
}

void OdExcludeArea::replanishErrorMap(
    nx::sdk::Ptr<nx::sdk::StringMap>& errorMap, const std::string& reason) const
{
    for (const auto paramName : kServerParamsNames)
        errorMap->setItem(paramName, reason);
}

std::string buildCameraRequestQuery(
    const OdExcludeArea& area, FrameSize frameSize, int channelNumber)
{
    std::ostringstream query;
    if (area)
    {
        if (!area.points.empty())
        {
            const std::string prefix = "&ExcludeArea."s + std::to_string(area.internalObjectIndex);
            query
                << "msubmenu=" << "objectdetection"
                << "&action=" << "set"
                << "&Channel=" << channelNumber
                << prefix << ".Coordinate=" << pluginPointsToSunapiString(area.points, frameSize)
                ;
        }
        else
        {
            query
                << "msubmenu=" << "objectdetection"
                << "&action=" << "remove"
                << "&Channel=" << channelNumber
                << "&ExcludeAreaIndex=" << area.internalObjectIndex
                ;
        }
    }
    return query.str();
}

//-------------------------------------------------------------------------------------------------

bool IvaLine::operator==(const IvaLine& rhs) const
{
    return
        initialized == rhs.initialized
        && internalObjectIndex == rhs.internalObjectIndex

        && points == rhs.points
        && name == rhs.name
        && person == rhs.person
        && vehicle == rhs.vehicle
        && crossing == rhs.crossing
        && direction == rhs.direction
        ;
}

bool IvaLine::differesEnoughFrom(const IvaLine& rhs) const
{
    if (this->points.empty() && rhs.points.empty())
        return false;

    return *this != rhs;
}

bool IvaLine::loadFromServer(const nx::sdk::IStringMap* settings, int objectIndex)
{
    std::decay_t<decltype(*this)> tmp;
    try
    {
        NX_READ_SETTING_OR_THROW(tmp, points);
        if (!tmp.points.empty())
        {
            NX_READ_SETTING_OR_THROW(tmp, direction);
        }
        NX_READ_SETTING_OR_THROW(tmp, name);
        NX_READ_SETTING_OR_THROW(tmp, person);
        NX_READ_SETTING_OR_THROW(tmp, vehicle);
        NX_READ_SETTING_OR_THROW(tmp, crossing);
    }
    catch (PluginValueError&)
    {
        return false;
    }
    *this = std::move(tmp);
    this->internalObjectIndex = objectIndex;
    this->initialized = true;
    return true;
}

bool IvaLine::loadFromSunapi(
    const std::string& cameraReply, int channelNumber, int objectIndex)
{
    std::decay_t<decltype(*this)> tmp;
    try
    {
        nx::kit::Json channel = getChannelOrThrow(cameraReply, "VideoAnalysis", channelNumber);

        nx::kit::Json lineObject = getLineJsonObject(channel, objectIndex);
        if (lineObject == nx::kit::Json())
        {
            *this = std::move(tmp); // reset value;
            return true;
        }

        ReadOrThrow2(lineObject, "Coordinates", &tmp.points);
        ReadOrThrow2(lineObject, "Mode", &tmp.direction);
        ReadOrThrow2(lineObject, "RuleName", &tmp.name);
        ReadOrThrow2(lineObject, "ObjectTypeFilter", &tmp.person, "Person");
        ReadOrThrow2(lineObject, "ObjectTypeFilter", &tmp.vehicle, "Vehicle");

        std::string Crossing;
        ReadOrThrow2(lineObject, "Mode", &Crossing);
        tmp.crossing = Crossing != "Off";
    }
    catch (SunapiValueError&)
    {
        return false;
    }
    *this = std::move(tmp);
    return true;
}
void IvaLine::replanishErrorMap(
    nx::sdk::Ptr<nx::sdk::StringMap>& errorMap, const std::string& reason) const
{
    for (const auto paramName: kServerParamsNames)
        errorMap->setItem(paramName, reason);
}

std::string buildFilter(const IvaLine& line)
{
    std::vector<const char*> mode;

    if (line.person)
        mode.push_back("Person");

    if (line.vehicle)
        mode.push_back("Vehicle");

    return concat(mode);
}

std::string buildMode(const IvaLine& line)
{
    if (!line.crossing)
        return "Off";
    if (line.direction == Direction::Right)
        return "Right";
    if (line.direction == Direction::Left)
        return "Left";
    if (line.direction == Direction::Both)
        return "BothDirections";

    NX_ASSERT(false);
    return {};
}

std::string buildCameraRequestQuery(
    const IvaLine& line, FrameSize frameSize, int channelNumber)
{
    std::ostringstream query;
    if (line)
    {
        if (!line.points.empty())
        {
            const std::string prefix = "&Line."s + std::to_string(line.internalObjectIndex);
            query
                << "msubmenu=" << "videoanalysis2"
                << "&action=" << "set"
                << "&Channel=" << channelNumber
                << prefix << ".Coordinate=" << pluginPointsToSunapiString(line.points, frameSize)
                << prefix << ".RuleName=" << line.name
                << prefix << ".ObjectTypeFilter=" << buildFilter(line)
                << prefix << ".Mode=" << buildMode(line)
                ;
        }
        else
        {
            query
                << "msubmenu=" << "videoanalysis2"
                << "&action=" << "remove"
                << "&Channel=" << channelNumber
                << "&LineIndex=" << line.internalObjectIndex;
        }
    }
    return query.str();
}

//-------------------------------------------------------------------------------------------------

bool IvaIncludeArea::loadFromServer(const nx::sdk::IStringMap* settings, int objectIndex)
{
    std::decay_t<decltype(*this)> tmp;
    try
    {
        NX_READ_SETTING_OR_THROW(tmp, points);
        NX_READ_SETTING_OR_THROW(tmp, name);
        NX_READ_SETTING_OR_THROW(tmp, person);
        NX_READ_SETTING_OR_THROW(tmp, vehicle);
        NX_READ_SETTING_OR_THROW(tmp, intrusion);
        NX_READ_SETTING_OR_THROW(tmp, enter);
        NX_READ_SETTING_OR_THROW(tmp, exit);
        NX_READ_SETTING_OR_THROW(tmp, appear);
        NX_READ_SETTING_OR_THROW(tmp, loitering);
        NX_READ_SETTING_OR_THROW(tmp, intrusionDuration);
        NX_READ_SETTING_OR_THROW(tmp, appearDuration);
        NX_READ_SETTING_OR_THROW(tmp, loiteringDuration);
    }
    catch (PluginValueError&)
    {
        return false;
    }
    *this = std::move(tmp);
    this->internalObjectIndex = objectIndex;
    this->initialized = true;
    return true;
}

bool IvaIncludeArea::loadFromSunapi(
    const std::string& cameraReply, int channelNumber, int objectIndex)
{
    std::decay_t<decltype(*this)> tmp;
    try
    {
        nx::kit::Json channel = getChannelOrThrow(cameraReply, "VideoAnalysis", channelNumber);

        nx::kit::Json ivaRoiObject = getIvaRoiJsonObject(channel, objectIndex);
        if (ivaRoiObject == nx::kit::Json())
        {
            *this = std::move(tmp); // reset value;
            return true;
        }

        ReadOrThrow2(ivaRoiObject, "Coordinates", &tmp.points);
        ReadOrThrow2(ivaRoiObject, "RuleName", &tmp.name);
        ReadOrThrow2(ivaRoiObject, "ObjectTypeFilter", &tmp.person, "Person");
        ReadOrThrow2(ivaRoiObject, "ObjectTypeFilter", &tmp.vehicle, "Vehicle");

        ReadOrThrow2(ivaRoiObject, "Mode", &tmp.intrusion, "Intrusion");
        ReadOrThrow2(ivaRoiObject, "Mode", &tmp.enter, "Entering");
        ReadOrThrow2(ivaRoiObject, "Mode", &tmp.exit, "Exiting");
        ReadOrThrow2(ivaRoiObject, "Mode", &tmp.appear, "AppearDisappear");
        ReadOrThrow2(ivaRoiObject, "Mode", &tmp.loitering, "Loitering");

        ReadOrThrow2(ivaRoiObject, "IntrusionDuration", &tmp.intrusionDuration);
        ReadOrThrow2(ivaRoiObject, "AppearanceDuration", &tmp.appearDuration);
        ReadOrThrow2(ivaRoiObject, "LoiteringDuration", &tmp.loiteringDuration);
    }
    catch (SunapiValueError&)
    {
        return false;
    }
    *this = std::move(tmp);
    return true;
}

bool IvaIncludeArea::operator==(const IvaIncludeArea& rhs) const
{
    return
        initialized == rhs.initialized
        && internalObjectIndex == rhs.internalObjectIndex

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
bool IvaIncludeArea::differesEnoughFrom(const IvaIncludeArea& rhs) const
{
    if (this->points.empty() && rhs.points.empty())
        return false;

    return *this != rhs;
}

void IvaIncludeArea::replanishErrorMap(
    nx::sdk::Ptr<nx::sdk::StringMap>& errorMap, const std::string& reason) const
{
    for (const auto paramName: kServerParamsNames)
        errorMap->setItem(paramName, reason);
}

//-------------------------------------------------------------------------------------------------

std::string buildMode(const IvaIncludeArea& area)
{
    std::vector<const char*> mode;

    if (area.intrusion)
        mode.push_back("Intrusion");

    if (area.enter)
        mode.push_back("Entering");

    if (area.exit)
        mode.push_back("Exiting");

    if (area.appear)
        mode.push_back("AppearDisappear");

    if (area.loitering)
        mode.push_back("Loitering");

    return concat(mode);
}

std::string buildFilter(const IvaIncludeArea& area)
{
    std::vector<const char*> mode;

    if (area.person)
        mode.push_back("Person");

    if (area.vehicle)
        mode.push_back("Vehicle");

    return concat(mode);
}

std::string buildCameraRequestQuery(
    const IvaIncludeArea& area, FrameSize frameSize, int channelNumber)
{
    std::ostringstream query;
    if (area)
    {
        if (!area.points.empty())
        {
            const std::string prefix = "&DefinedArea."s + std::to_string(area.internalObjectIndex);
            query
                << "msubmenu=" << "videoanalysis2"
                << "&action=" << "set"
                << "&Channel=" << channelNumber
                << prefix << ".Coordinate=" << pluginPointsToSunapiString(area.points, frameSize)
                << prefix << ".Type=" << "Inside"
                << prefix << ".RuleName=" << area.name
                << prefix << ".ObjectTypeFilter=" << buildFilter(area)
                << prefix << ".Mode=" << buildMode(area)
                << prefix << ".IntrusionDuration=" << area.intrusionDuration
                << prefix << ".AppearanceDuration=" << area.appearDuration
                << prefix << ".LoiteringDuration=" << area.loiteringDuration
                ;
        }
        else
        {
                query
                << "msubmenu=" << "videoanalysis2"
                << "&action=" << "remove"
                << "&Channel=" << channelNumber
                << "&DefinedAreaIndex=" << area.internalObjectIndex;
        }
    }
    return query.str();
}

//-------------------------------------------------------------------------------------------------

bool IvaExcludeArea::loadFromServer(const nx::sdk::IStringMap* settings, int objectIndex)
{
    std::decay_t<decltype(*this)> tmp;
    try
    {
        NX_READ_SETTING_OR_THROW(tmp, points);
    }
    catch (PluginValueError&)
    {
        return false;
    }
    *this = std::move(tmp);
    this->internalObjectIndex = objectIndex + 8;
    this->initialized = true;
    return true;
}

bool IvaExcludeArea::loadFromSunapi(
    const std::string& cameraReply, int channelNumber, int objectIndex)
{
    std::decay_t<decltype(*this)> tmp;
    try
    {
        nx::kit::Json channel = getChannelOrThrow(cameraReply, "VideoAnalysis", channelNumber);

        nx::kit::Json ivaRoiObject = getIvaRoiJsonObject(channel, objectIndex + 8);
        if (ivaRoiObject == nx::kit::Json())
        {
            *this = std::move(tmp); // reset value;
            return true;
        }

        ReadOrThrow2(ivaRoiObject, "Coordinates", &tmp.points);
    }
    catch (SunapiValueError&)
    {
        return false;
    }
    *this = std::move(tmp);
    return true;
}

bool IvaExcludeArea::operator==(const IvaExcludeArea& rhs) const
{
    return
        initialized == rhs.initialized
        && internalObjectIndex == rhs.internalObjectIndex

        && points == rhs.points
        ;
}
bool IvaExcludeArea::differesEnoughFrom(const IvaExcludeArea& rhs) const
{
    if (this->points.empty() && rhs.points.empty())
        return false;

    return *this != rhs;
}

void IvaExcludeArea::replanishErrorMap(
    nx::sdk::Ptr<nx::sdk::StringMap>& errorMap, const std::string& reason) const
{
    for (const auto paramName : kServerParamsNames)
        errorMap->setItem(paramName, reason);
}

std::string buildCameraRequestQuery(
    const IvaExcludeArea& area, FrameSize frameSize, int channelNumber)
{
    std::ostringstream query;
    if (area)
    {
        if (!area.points.empty())
        {
            const std::string prefix = "&DefinedArea."s + std::to_string(area.internalObjectIndex);
            query
                << "msubmenu=" << "videoanalysis2"
                << "&action=" << "set"
                << "&Channel=" << channelNumber
                << prefix << ".Coordinate=" << pluginPointsToSunapiString(area.points, frameSize)
                << prefix << ".Type=" << "Outside"
                ;
        }
        else
        {
            query
                << "msubmenu=" << "videoanalysis2"
                << "&action=" << "remove"
                << "&Channel=" << channelNumber
                << "&DefinedAreaIndex=" << area.internalObjectIndex
                ;
        }
    }
    return query.str();
}

//-------------------------------------------------------------------------------------------------

bool AudioDetection::operator==(const AudioDetection& rhs) const
{
    return
        initialized == rhs.initialized

        && enabled == rhs.enabled
        && thresholdLevel == rhs.thresholdLevel
        ;
}

bool AudioDetection::loadFromServer(const nx::sdk::IStringMap* settings, int objectIndex)
{
    std::decay_t<decltype(*this)> tmp;
    try
    {
        NX_READ_SETTING_OR_THROW(tmp, enabled);
        NX_READ_SETTING_OR_THROW(tmp, thresholdLevel);
    }
    catch (PluginValueError&)
    {
        return false;
    }
    *this = std::move(tmp);
    this->initialized = true;
    return true;
}

bool AudioDetection::loadFromSunapi(const std::string& cameraReply, int channelNumber)
{
    std::decay_t<decltype(*this)> tmp;
    try
    {
        nx::kit::Json channel = getChannelOrThrow(cameraReply, "AudioDetection", channelNumber);

        ReadOrThrow2(channel, "Enable", &tmp.enabled);
        ReadOrThrow2(channel, "InputThresholdLevel", &tmp.thresholdLevel);
    }
    catch (SunapiValueError&)
    {
        return false;
    }
    *this = std::move(tmp);
    return true;
}

void AudioDetection::replanishErrorMap(
    nx::sdk::Ptr<nx::sdk::StringMap>& errorMap, const std::string& reason) const
{
    for (const auto paramName : kServerParamsNames)
        errorMap->setItem(paramName, reason);
}

std::string buildCameraRequestQuery(
    const AudioDetection& settingGroup, FrameSize /*frameSize*/, int channelNumber)
{
    std::ostringstream query;
    if (settingGroup)
    {
        query
            << "msubmenu=" << "audiodetection"
            << "&action=" << "set"
            << "&Channel=" << channelNumber
            << "&Enable=" << buildBool(settingGroup.enabled)
            << "&InputThresholdLevel=" << settingGroup.thresholdLevel
            ;
    }
    return query.str();
}

//-------------------------------------------------------------------------------------------------

bool SoundClassification::operator==(const SoundClassification& rhs) const
{
    return
        initialized == rhs.initialized

        && enabled == rhs.enabled
        && noisefilter == rhs.noisefilter
        && thresholdLevel == rhs.thresholdLevel
        && scream == rhs.scream
        && gunshot == rhs.gunshot
        && explosion == rhs.explosion
        && crashingGlass == rhs.crashingGlass
        ;
}

bool SoundClassification::loadFromServer(const nx::sdk::IStringMap* settings, int objectIndex)
{
    std::decay_t<decltype(*this)> tmp;
    try
    {
        NX_READ_SETTING_OR_THROW(tmp, enabled);
        NX_READ_SETTING_OR_THROW(tmp, noisefilter);
        NX_READ_SETTING_OR_THROW(tmp, thresholdLevel);
        NX_READ_SETTING_OR_THROW(tmp, scream);
        NX_READ_SETTING_OR_THROW(tmp, gunshot);
        NX_READ_SETTING_OR_THROW(tmp, explosion);
        NX_READ_SETTING_OR_THROW(tmp, crashingGlass);
    }
    catch (PluginValueError&)
    {
        return false;
    }
    *this = std::move(tmp);
    this->initialized = true;
    return true;
}

bool SoundClassification::loadFromSunapi(const std::string& cameraReply, int channelNumber)
{
    std::decay_t<decltype(*this)> tmp;
    try
    {
        nx::kit::Json channel = getChannelOrThrow(cameraReply, "AudioAnalysis", channelNumber);

        ReadOrThrow2(channel, "Enable", &tmp.enabled);
        ReadOrThrow2(channel, "NoiseReduction", &tmp.noisefilter);
        ReadOrThrow2(channel, "ThresholdLevel", &tmp.thresholdLevel);
        ReadOrThrow2(channel, "SoundType", &tmp.scream, "Scream");
        ReadOrThrow2(channel, "SoundType", &tmp.gunshot, "Gunshot");
        ReadOrThrow2(channel, "SoundType", &tmp.explosion, "Explosion");
        ReadOrThrow2(channel, "SoundType", &tmp.crashingGlass, "GlassBreak");
    }
    catch (SunapiValueError&)
    {
        return false;
    }
    *this = std::move(tmp);
    return true;
}

void SoundClassification::replanishErrorMap(
    nx::sdk::Ptr<nx::sdk::StringMap>& errorMap, const std::string& reason) const
{
    for (const auto paramName: kServerParamsNames)
        errorMap->setItem(paramName, reason);
}

std::string buildSoundType(const SoundClassification& settingGroup)
{
    std::vector<const char*> mode;

    if (settingGroup.scream)
        mode.push_back("Scream");

    if (settingGroup.gunshot)
        mode.push_back("Gunshot");

    if (settingGroup.explosion)
        mode.push_back("Explosion");

    if (settingGroup.crashingGlass)
        mode.push_back("GlassBreak");

    return concat(mode);
}

std::string buildCameraRequestQuery(
    const SoundClassification& settingGroup, FrameSize /*frameSize*/, int channelNumber)
{
    std::ostringstream query;
    if (settingGroup)
    {
        query
            << "msubmenu=" << "audioanalysis"
            << "&action=" << "set"
            << "&Channel=" << channelNumber
            << "&Enable=" << buildBool(settingGroup.enabled)
            << "&NoiseReduction=" << buildBool(settingGroup.noisefilter)
            << "&ThresholdLevel=" << settingGroup.thresholdLevel
            << "&SoundType=" << buildSoundType(settingGroup)
            ;
    }
    return query.str();
}

//-------------------------------------------------------------------------------------------------

} // namespace nx::vms_server_plugins::analytics::hanwha
