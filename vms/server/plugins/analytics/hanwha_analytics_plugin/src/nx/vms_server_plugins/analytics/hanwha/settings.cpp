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

ShockDetection::ShockDetection(const std::vector<std::string>& params)
{
    if (std::size(params) != std::size(kParams))
        return;
    enabled = (params[0] == "True");
    char* end = nullptr;
    // Standard locale-independent conversion designed for JSON.
    std::from_chars(&params[1].front(), &params[1].back() + 1, thresholdLevel);
    std::from_chars(&params[2].front(), &params[2].back() + 1, sensitivityLevel);
    initialized = true;
}

bool ShockDetection::operator==(const ShockDetection& rhs) const
{
    return enabled == rhs.enabled
        && thresholdLevel == rhs.thresholdLevel
        && sensitivityLevel == rhs.sensitivityLevel
        && initialized == rhs.initialized;
}

//-------------------------------------------------------------------------------------------------

Motion::Motion(const std::vector<std::string>& params)
{
    if (std::size(params) != std::size(kParams))
        return;

    detectionType = params[0];
    initialized = true;
}

bool Motion::operator==(const Motion& rhs) const
{
    return detectionType == rhs.detectionType
        && initialized == rhs.initialized;
}

//-------------------------------------------------------------------------------------------------

IncludeArea::IncludeArea(const std::vector<std::string>& params)
{
    if (std::size(params) != std::size(kParams))
        return;

    char* end = nullptr;
    // Standard locale-independent conversion designed for JSON.
    points = sunapiStringToSunapiPoints(params[0]);
    std::from_chars(&params[1].front(), &params[1].back() + 1, thresholdLevel);
    std::from_chars(&params[2].front(), &params[2].back() + 1, sensitivityLevel);
    std::from_chars(&params[3].front(), &params[3].back() + 1, minimumDuration);
    initialized = true;
}

bool IncludeArea::operator==(const IncludeArea& rhs) const
{
    return points == rhs.points
        && thresholdLevel == rhs.thresholdLevel
        && sensitivityLevel == rhs.sensitivityLevel
        && minimumDuration == rhs.minimumDuration
        && initialized == rhs.initialized;
}

//-------------------------------------------------------------------------------------------------

ExcludeArea::ExcludeArea(const std::vector<std::string>& params)
{
    if (std::size(params) != std::size(kParams))
        return;

    char* end = nullptr;
    points = sunapiStringToSunapiPoints(params[0]);
    initialized = true;
}

//-------------------------------------------------------------------------------------------------

TamperingDetection::TamperingDetection(const std::vector<std::string>& params)
{
    if (std::size(params) != std::size(kParams))
        return;

    enabled = (params[0] == "True");
    char* end = nullptr;
    std::from_chars(&params[1].front(), &params[1].back() + 1, thresholdLevel);
    std::from_chars(&params[2].front(), &params[2].back() + 1, sensitivityLevel);
    std::from_chars(&params[3].front(), &params[3].back() + 1, minimumDuration);
    exceptDarkImages = (params[4] == "True");
    initialized = true;
}

bool TamperingDetection::operator==(const TamperingDetection& rhs) const
{
    return enabled == rhs.enabled
        && thresholdLevel == rhs.thresholdLevel
        && sensitivityLevel == rhs.sensitivityLevel
        && minimumDuration == rhs.minimumDuration
        && exceptDarkImages == rhs.exceptDarkImages
        && initialized == rhs.initialized;
}

//-------------------------------------------------------------------------------------------------

DefocusDetection::DefocusDetection(const std::vector<std::string>& params)
{
    if (std::size(params) != std::size(kParams))
        return;

    enabled = (params[0] == "True");
    char* end = nullptr;
    std::from_chars(&params[1].front(), &params[1].back() + 1, thresholdLevel);
    std::from_chars(&params[2].front(), &params[2].back() + 1, sensitivityLevel);
    std::from_chars(&params[3].front(), &params[3].back() + 1, minimumDuration);
    initialized = true;
}

bool DefocusDetection::operator==(const DefocusDetection& rhs) const
{
    return enabled == rhs.enabled
        && thresholdLevel == rhs.thresholdLevel
        && sensitivityLevel == rhs.sensitivityLevel
        && minimumDuration == rhs.minimumDuration
        && initialized == rhs.initialized;
}

//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
struct PluginValueError {};

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

    std::optional<std::vector<PluginPoint>> tmp = parsePluginPoints(value);
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

    std::optional<Direction> tmp = parsePluginDirection(value);
    if (tmp)
    {
        *result = *tmp;
        return;
    }

    throw PluginValueError();
}

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

bool ObjectDetectionObjects::operator==(const ObjectDetectionObjects& rhs) const
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

bool ObjectDetectionObjects::loadFromServer(const nx::sdk::IStringMap* settings, int objectIndex)
{
    ObjectDetectionObjects tmp;
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

void ObjectDetectionObjects::replanishErrorMap(
    nx::sdk::Ptr<nx::sdk::StringMap>& errorMap, const std::string& reason) const
{
    for (const auto paramName : kServerParamsNames)
        errorMap->setItem(paramName, reason);
}

std::string buildObjectTypes(const ObjectDetectionObjects& settingGroup)
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
    const ObjectDetectionObjects& settingGroup, FrameSize /*frameSize*/, int channelNumber)
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

bool ObjectDetectionBestShot::operator==(const ObjectDetectionBestShot& rhs) const
{
    return
        initialized == rhs.initialized

        && person == rhs.person
        && vehicle == rhs.vehicle
        && face == rhs.face
        && licensePlate == rhs.licensePlate
        ;
}

bool ObjectDetectionBestShot::loadFromServer(const nx::sdk::IStringMap* settings, int objectIndex)
{
    ObjectDetectionBestShot tmp;
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

void ObjectDetectionBestShot::replanishErrorMap(
    nx::sdk::Ptr<nx::sdk::StringMap>& errorMap, const std::string& reason) const
{
    for (const auto paramName : kServerParamsNames)
        errorMap->setItem(paramName, reason);
}

std::string buildObjectTypes(const ObjectDetectionBestShot& settingGroup)
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
    const ObjectDetectionBestShot& settingGroup, FrameSize /*frameSize*/, int channelNumber)
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

bool ObjectDetectionExcludeArea::loadFromServer(const nx::sdk::IStringMap* settings, int objectIndex)
{
    ObjectDetectionExcludeArea tmp;
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

bool ObjectDetectionExcludeArea::operator==(const ObjectDetectionExcludeArea& rhs) const
{
    return
        initialized == rhs.initialized
        && internalObjectIndex == rhs.internalObjectIndex

        && points == rhs.points
        ;
}
bool ObjectDetectionExcludeArea::differesEnoughFrom(const ObjectDetectionExcludeArea& rhs) const
{
    if (this->points.empty() && rhs.points.empty())
        return false;

    return *this != rhs;
}

void ObjectDetectionExcludeArea::replanishErrorMap(
    nx::sdk::Ptr<nx::sdk::StringMap>& errorMap, const std::string& reason) const
{
    for (const auto paramName : kServerParamsNames)
        errorMap->setItem(paramName, reason);
}

std::string buildCameraRequestQuery(
    const ObjectDetectionExcludeArea& area, FrameSize frameSize, int channelNumber)
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
    IvaLine tmp;
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
        //NX_READ_SETTING_OR_THROW(tmp, direction);
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
    IvaIncludeArea tmp;
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
    IvaExcludeArea tmp;
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
    AudioDetection tmp;
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
    SoundClassification tmp;
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
