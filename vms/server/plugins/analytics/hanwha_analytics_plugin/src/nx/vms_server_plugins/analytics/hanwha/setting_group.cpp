#include "setting_group.h"

#include <cstdlib>
#include <charconv>
#include <string>
#include <sstream>
#include <optional>
#include <algorithm>

#include <nx/utils/log/log.h>

#include "setting_primitives_io.h"
#include "device_response_json_parser.h"

using namespace std::literals;

namespace nx::vms_server_plugins::analytics::hanwha {

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

void SettingGroup::replenishErrorMap(nx::sdk::StringMap* errorMap, const std::string& reason) const
{
    for (int i = 0; i < serverKeyCount; ++i)
    {
        const std::string key = this->key(i);
        errorMap->setItem(key, reason);
    }
}

void SettingGroup::replenishValueMap(
    nx::sdk::StringMap* valueMap, const nx::sdk::IStringMap* sourceMap) const
{
    for (int i = 0; i < serverKeyCount; ++i)
    {
        const std::string key = this->key(i);
        valueMap->setItem(key, sourceMap->value(key.c_str()));
    }
}
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

std::string join(std::vector<const char*> items, char c = ',')
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

AnalyticsMode AnalyticsMode::addIntelligentVideoMode() const
{
    AnalyticsMode result = *this;
    if (result.detectionType == Off)
        result.detectionType = IV;
    else if (result.detectionType == MD)
        result.detectionType = MDAndIV;
    // else do nothing
    return result;
}
AnalyticsMode AnalyticsMode::removeIntelligentVideoMode() const
{
    AnalyticsMode result = *this;
    if (result.detectionType == IV)
        result.detectionType = Off;
    else if (result.detectionType == MDAndIV)
        result.detectionType = MD;
    // else do nothing
    return result;
}

bool AnalyticsMode::operator==(const AnalyticsMode& rhs) const
{
    return initialized == rhs.initialized
        && detectionType == rhs.detectionType
        ;
}

void AnalyticsMode::readFromServerOrThrow(const nx::sdk::IStringMap* sourceMap, int /*roiIndex*/)
{
    using namespace SettingPrimitivesServerIo;
    deserializeOrThrow(value(sourceMap, KeyIndex::detectionType), &detectionType);
    initialized = true;
}

void AnalyticsMode::writeToServer(nx::sdk::SettingsResponse* result, int /*roiIndex*/) const
{
    using namespace SettingPrimitivesServerIo;
    result->setValue(key(KeyIndex::detectionType), serialize(detectionType));
}

void AnalyticsMode::readFromDeviceReplyOrThrow(const nx::kit::Json& channelInfo)
{
    using namespace SettingPrimitivesDeviceIo;
    deserializeOrThrow(channelInfo, "DetectionType", m_roiResolution, &detectionType);
    initialized = true;
}

std::string AnalyticsMode::buildDeviceWritingQuery(int channelNumber) const
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
    using namespace SettingPrimitivesServerIo;
    deserializeOrThrow(value(sourceMap, KeyIndex::enabled), &enabled);
    deserializeOrThrow(value(sourceMap, KeyIndex::thresholdLevel), &thresholdLevel);
    deserializeOrThrow(value(sourceMap, KeyIndex::sensitivityLevel), &sensitivityLevel);
    initialized = true;
}

void ShockDetection::writeToServer(nx::sdk::SettingsResponse* result, int /*roiIndex*/) const
{
    using namespace SettingPrimitivesServerIo;
    result->setValue(key(KeyIndex::enabled), serialize(enabled));
    result->setValue(key(KeyIndex::thresholdLevel), serialize(thresholdLevel));
    result->setValue(key(KeyIndex::sensitivityLevel), serialize(sensitivityLevel));
}

void ShockDetection::readFromDeviceReplyOrThrow(const nx::kit::Json& channelInfo)
{
    using namespace SettingPrimitivesDeviceIo;
    deserializeOrThrow(channelInfo, "Enable", m_roiResolution, &enabled);
    deserializeOrThrow(channelInfo, "ThresholdLevel", m_roiResolution, &thresholdLevel);
    deserializeOrThrow(channelInfo, "Sensitivity", m_roiResolution, &sensitivityLevel);
    initialized = true;
}

std::string ShockDetection::buildDeviceWritingQuery(int channelNumber) const
{
    std::ostringstream query;
    if (initialized)
    {
        using namespace SettingPrimitivesDeviceIo;
        query
            << "msubmenu=" << kSunapiEventName
            << "&action=" << "set"
            << "&Channel=" << channelNumber
            << "&Enable=" << serialize(enabled)
            << "&ThresholdLevel=" << thresholdLevel
            << "&Sensitivity=" << sensitivityLevel
            ;
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
    using namespace SettingPrimitivesServerIo;
    deserializeOrThrow(value(sourceMap, KeyIndex::enabled), &enabled);
    deserializeOrThrow(value(sourceMap, KeyIndex::thresholdLevel), &thresholdLevel);
    deserializeOrThrow(value(sourceMap, KeyIndex::sensitivityLevel), &sensitivityLevel);
    deserializeOrThrow(value(sourceMap, KeyIndex::minimumDuration), &minimumDuration);
    deserializeOrThrow(value(sourceMap, KeyIndex::exceptDarkImages), &exceptDarkImages);
    initialized = true;
}

void TamperingDetection::writeToServer(nx::sdk::SettingsResponse* result, int /*roiIndex*/) const
{
    using namespace SettingPrimitivesServerIo;
    result->setValue(key(KeyIndex::enabled), serialize(enabled));
    result->setValue(key(KeyIndex::thresholdLevel), serialize(thresholdLevel));
    result->setValue(key(KeyIndex::sensitivityLevel), serialize(sensitivityLevel));
    result->setValue(key(KeyIndex::minimumDuration), serialize(minimumDuration));
    result->setValue(key(KeyIndex::exceptDarkImages), serialize(exceptDarkImages));
}

void TamperingDetection::readFromDeviceReplyOrThrow(const nx::kit::Json& channelInfo)
{
    using namespace SettingPrimitivesDeviceIo;
    deserializeOrThrow(channelInfo, "Enable", m_roiResolution, &enabled);
    deserializeOrThrow(channelInfo, "ThresholdLevel", m_roiResolution, &thresholdLevel);
    deserializeOrThrow(channelInfo, "SensitivityLevel", m_roiResolution, &sensitivityLevel);
    deserializeOrThrow(channelInfo, "Duration", m_roiResolution, &minimumDuration);
    deserializeOrThrow(channelInfo, "DarknessDetection", m_roiResolution, &exceptDarkImages);
    initialized = true;
}

std::string TamperingDetection::buildDeviceWritingQuery(int channelNumber) const
{
    std::ostringstream query;
    if (initialized)
    {
        using namespace SettingPrimitivesDeviceIo;
        query
            << "msubmenu=" << kSunapiEventName
            << "&action=" << "set"
            << "&Channel=" << channelNumber
            << "&Enable=" << serialize(enabled)
            << "&ThresholdLevel=" << thresholdLevel
            << "&SensitivityLevel=" << sensitivityLevel
            << "&Duration=" << minimumDuration
            << "&DarknessDetection=" << serialize(exceptDarkImages)
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
    using namespace SettingPrimitivesServerIo;
    deserializeOrThrow(value(sourceMap, KeyIndex::enabled), &enabled);
    deserializeOrThrow(value(sourceMap, KeyIndex::thresholdLevel), &thresholdLevel);
    deserializeOrThrow(value(sourceMap, KeyIndex::sensitivityLevel), &sensitivityLevel);
    deserializeOrThrow(value(sourceMap, KeyIndex::minimumDuration), &minimumDuration);
    initialized = true;
}

void DefocusDetection::writeToServer(nx::sdk::SettingsResponse* result, int /*roiIndex*/) const
{
    using namespace SettingPrimitivesServerIo;
    result->setValue(key(KeyIndex::enabled), serialize(enabled));
    result->setValue(key(KeyIndex::thresholdLevel), serialize(thresholdLevel));
    result->setValue(key(KeyIndex::sensitivityLevel), serialize(sensitivityLevel));
    result->setValue(key(KeyIndex::minimumDuration), serialize(minimumDuration));
}

void DefocusDetection::readFromDeviceReplyOrThrow(const nx::kit::Json& channelInfo)
{
    using namespace SettingPrimitivesDeviceIo;
    deserializeOrThrow(channelInfo, "Enable", m_roiResolution, &enabled);
    deserializeOrThrow(channelInfo, "ThresholdLevel", m_roiResolution, &thresholdLevel);
    deserializeOrThrow(channelInfo, "Sensitivity", m_roiResolution, &sensitivityLevel);
    deserializeOrThrow(channelInfo, "Duration", m_roiResolution, &minimumDuration);
    initialized = true;
}

std::string DefocusDetection::buildDeviceWritingQuery(int channelNumber) const
{
    std::ostringstream query;
    if (initialized)
    {
        using namespace SettingPrimitivesDeviceIo;
        query
            << "msubmenu=" << kSunapiEventName
            << "&action=" << "set"
            << "&Channel=" << channelNumber
            << "&Enable=" << serialize(enabled)
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
    using namespace SettingPrimitivesServerIo;
    deserializeOrThrow(value(sourceMap, KeyIndex::enabled), &enabled);
    deserializeOrThrow(value(sourceMap, KeyIndex::thresholdLevel), &thresholdLevel);
    deserializeOrThrow(value(sourceMap, KeyIndex::sensitivityLevel), &sensitivityLevel);
    deserializeOrThrow(value(sourceMap, KeyIndex::minimumDuration), &minimumDuration);
    initialized = true;
}

void FogDetection::writeToServer(nx::sdk::SettingsResponse* result, int /*roiIndex*/) const
{
    using namespace SettingPrimitivesServerIo;
    result->setValue(key(KeyIndex::enabled), serialize(enabled));
    result->setValue(key(KeyIndex::thresholdLevel), serialize(thresholdLevel));
    result->setValue(key(KeyIndex::sensitivityLevel), serialize(sensitivityLevel));
    result->setValue(key(KeyIndex::minimumDuration), serialize(minimumDuration));
}

void FogDetection::readFromDeviceReplyOrThrow(const nx::kit::Json& channelInfo)
{
    using namespace SettingPrimitivesDeviceIo;
    deserializeOrThrow(channelInfo, "Enable", m_roiResolution, &enabled);
    deserializeOrThrow(channelInfo, "ThresholdLevel", m_roiResolution, &thresholdLevel);
    deserializeOrThrow(channelInfo, "SensitivityLevel", m_roiResolution, &sensitivityLevel);
    deserializeOrThrow(channelInfo, "Duration", m_roiResolution, &minimumDuration);
    initialized = true;
}

std::string FogDetection::buildDeviceWritingQuery(int channelNumber) const
{
    std::ostringstream query;
    if (initialized)
    {
        using namespace SettingPrimitivesDeviceIo;
        query
            << "msubmenu=" << kSunapiEventName
            << "&action=" << "set"
            << "&Channel=" << channelNumber
            << "&Enable=" << serialize(enabled)
            << "&ThresholdLevel=" << thresholdLevel
            << "&SensitivityLevel=" << sensitivityLevel
            << "&Duration=" << minimumDuration
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

void ObjectDetectionGeneral::readFromServerOrThrow(const nx::sdk::IStringMap* sourceMap, int /*roiIndex*/)
{
    using namespace SettingPrimitivesServerIo;
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
    using namespace SettingPrimitivesServerIo;
    result->setValue(key(KeyIndex::enabled), serialize(enabled));
    result->setValue(key(KeyIndex::person), serialize(person));
    result->setValue(key(KeyIndex::vehicle), serialize(vehicle));
    result->setValue(key(KeyIndex::face), serialize(face));
    result->setValue(key(KeyIndex::licensePlate), serialize(licensePlate));
    result->setValue(key(KeyIndex::minimumDuration), serialize(minimumDuration));
}

void ObjectDetectionGeneral::readFromDeviceReplyOrThrow(const nx::kit::Json& channelInfo)
{
    using namespace SettingPrimitivesDeviceIo;
    deserializeOrThrow(channelInfo, "Enable", m_roiResolution, &enabled);
    deserializeOrThrow(channelInfo, "ObjectTypes", m_roiResolution, &person, "Person");
    deserializeOrThrow(channelInfo, "ObjectTypes", m_roiResolution, &vehicle, "Vehicle");
    deserializeOrThrow(channelInfo, "ObjectTypes", m_roiResolution, &face, "Face");
    deserializeOrThrow(channelInfo, "ObjectTypes", m_roiResolution, &licensePlate, "LicensePlate");
    deserializeOrThrow(channelInfo, "Duration", m_roiResolution, &minimumDuration);
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

    return join(mode);
}

std::string ObjectDetectionGeneral::buildDeviceWritingQuery(int channelNumber) const
{
    std::ostringstream query;
    if (initialized)
    {
        using namespace SettingPrimitivesDeviceIo;
        query
            << "msubmenu=" << kSunapiEventName
            << "&action=" << "set"
            << "&Channel=" << channelNumber
            << "&Enable=" << serialize(enabled)
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
    using namespace SettingPrimitivesServerIo;
    deserializeOrThrow(value(sourceMap, KeyIndex::person), &person);
    deserializeOrThrow(value(sourceMap, KeyIndex::vehicle), &vehicle);
    deserializeOrThrow(value(sourceMap, KeyIndex::face), &face);
    deserializeOrThrow(value(sourceMap, KeyIndex::licensePlate), &licensePlate);
    initialized = true;
}

void ObjectDetectionBestShot::writeToServer(nx::sdk::SettingsResponse* result, int /*roiIndex*/) const
{
    using namespace SettingPrimitivesServerIo;
    result->setValue(key(KeyIndex::person), serialize(person));
    result->setValue(key(KeyIndex::vehicle), serialize(vehicle));
    result->setValue(key(KeyIndex::face), serialize(face));
    result->setValue(key(KeyIndex::licensePlate), serialize(licensePlate));
}

void ObjectDetectionBestShot::readFromDeviceReplyOrThrow(const nx::kit::Json& channelInfo)
{
    using namespace SettingPrimitivesDeviceIo;
    deserializeOrThrow(channelInfo, "ObjectTypes", m_roiResolution, &person, "Person");
    deserializeOrThrow(channelInfo, "ObjectTypes", m_roiResolution, &vehicle, "Vehicle");
    deserializeOrThrow(channelInfo, "ObjectTypes", m_roiResolution, &face, "Face");
    deserializeOrThrow(channelInfo, "ObjectTypes", m_roiResolution, &licensePlate, "LicensePlate");
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

    return join(mode);
}

std::string ObjectDetectionBestShot::buildDeviceWritingQuery(int channelNumber) const
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
    using namespace SettingPrimitivesServerIo;
    deserializeOrThrow(value(sourceMap, KeyIndex::constraints), &constraints);
    initialized = true;
}

void IvaObjectSize::writeToServer(nx::sdk::SettingsResponse* result, int /*roiIndex*/) const
{
    using namespace SettingPrimitivesServerIo;
    result->setValue(key(KeyIndex::constraints), serialize(constraints));
}

void IvaObjectSize::readFromDeviceReplyOrThrow(const nx::kit::Json& channelInfo)
{
    using namespace SettingPrimitivesDeviceIo;
    nx::kit::Json objectSizeInfo = DeviceResponseJsonParser::extractObjectSizeInfo(channelInfo, "IntelligentVideo");
    deserializeOrThrow(objectSizeInfo, m_roiResolution, &constraints);
    initialized = true;
}

std::string IvaObjectSize::buildMinObjectSize() const
{
    std::stringstream stream;
    stream
        << m_roiResolution.xRelativeToAbsolute(constraints.minWidth)
        << ','
        << m_roiResolution.yRelativeToAbsolute(constraints.minHeight);
    return stream.str();
}

std::string IvaObjectSize::buildMaxObjectSize() const
{
    std::stringstream stream;
    stream
        << m_roiResolution.xRelativeToAbsolute(constraints.maxWidth)
        << ','
        << m_roiResolution.yRelativeToAbsolute(constraints.maxHeight);
    return stream.str();
}

std::string IvaObjectSize::buildDeviceWritingQuery(int channelNumber) const
{
    std::ostringstream query;
    if (initialized)
    {
        query
            << "msubmenu=" << kSunapiEventName
            << "&action=" << "set"
            << "&Channel=" << channelNumber
            << "&DetectionType.IntelligentVideo.MinimumObjectSizeInPixels="
                << buildMinObjectSize()
            << "&DetectionType.IntelligentVideo.MaximumObjectSizeInPixels="
                << buildMaxObjectSize()
            ;
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

void IvaLine::assignExclusiveFrom(const IvaLine& other)
{
    if (!m_settingsCapabilities.ivaLineRuleName)
        this->namedLineFigure.name = other.namedLineFigure.name;
}

void IvaLine::readExclusiveFromServerOrThrow(const nx::sdk::IStringMap* sourceMap)
{
    using namespace SettingPrimitivesServerIo;
    NamedLineFigure tmpLine;
    deserializeOrThrow(value(sourceMap, KeyIndex::namedLineFigure), &tmpLine);
    namedLineFigure.name = tmpLine.name;
}

void IvaLine::readFromServerOrThrow(
    const nx::sdk::IStringMap* sourceMap,
    int /*roiIndex*/)
{
    using namespace SettingPrimitivesServerIo;
    deserializeOrThrow(value(sourceMap, KeyIndex::namedLineFigure), &namedLineFigure);
    if (m_settingsCapabilities.ivaLineObjectTypeFilter)
    {
        deserializeOrThrow(value(sourceMap, KeyIndex::person), &person);
        deserializeOrThrow(value(sourceMap, KeyIndex::vehicle), &vehicle);
    }
    deserializeOrThrow(value(sourceMap, KeyIndex::crossing), &crossing);

    initialized = true;
}

void IvaLine::writeToServer(
    nx::sdk::SettingsResponse* result,
    int /*roiIndex*/) const
{
    using namespace SettingPrimitivesServerIo;
    result->setValue(key(KeyIndex::namedLineFigure), serialize(namedLineFigure));
    if (m_settingsCapabilities.ivaLineObjectTypeFilter)
    {
        result->setValue(key(KeyIndex::person), serialize(person));
        result->setValue(key(KeyIndex::vehicle), serialize(vehicle));
    }
    result->setValue(key(KeyIndex::crossing), serialize(crossing));
}

void IvaLine::readFromDeviceReplyOrThrow(const nx::kit::Json& channelInfo)
{
    const nx::kit::Json lineInfo = DeviceResponseJsonParser::extractIvaLineInfo(
        channelInfo, this->deviceIndex());

    if (lineInfo == nx::kit::Json())
    {
        *this = IvaLine(m_settingsCapabilities, m_roiResolution, this->nativeIndex()); // reset value;
        initialized = true;
        return;
    }

    using namespace SettingPrimitivesDeviceIo;
    deserializeOrThrow(lineInfo, "Coordinates", m_roiResolution, &namedLineFigure.points);

    deserializeOrThrow(lineInfo, "Mode", m_roiResolution, &namedLineFigure.direction);

    // Direction in the plugin and direction in the device are defined in a different way
    // After reading direction from the device the direction should be treated in a special way.
    if (namedLineFigure.points[0].y < namedLineFigure.points[1].y)
        namedLineFigure.direction = invertedDirection(namedLineFigure.direction);

    if (m_settingsCapabilities.ivaLineRuleName)
        deserializeOrThrow(lineInfo, "RuleName", m_roiResolution, &namedLineFigure.name);

    if (m_settingsCapabilities.ivaLineObjectTypeFilter)
    {
        deserializeOrThrow(lineInfo, "ObjectTypeFilter", m_roiResolution, &person, "Person");
        deserializeOrThrow(lineInfo, "ObjectTypeFilter", m_roiResolution, &vehicle, "Vehicle");
    }

    std::string Crossing;
    deserializeOrThrow(lineInfo, "Mode", m_roiResolution, &Crossing);
    this->crossing = Crossing != "Off";
    initialized = true;
}

/*static*/ Direction IvaLine::invertedDirection(Direction direction)
{
    switch (direction)
    {
        case Direction::Right: return Direction::Left;
        case Direction::Left: return Direction::Right;
    }
    return direction;
}

std::string IvaLine::buildFilter() const
{
    std::vector<const char*> mode;

    if (person)
        mode.push_back("Person");

    if (vehicle)
        mode.push_back("Vehicle");

    return join(mode);
}

std::string IvaLine::buildMode(/*bool inverted = false*/) const
{
    if (!crossing)
        return "Off";

    Direction deviceDirection = namedLineFigure.direction;

    // Direction in the plugin and direction in the device are defined in a different way
    // Before sending direction to the device the direction should be treated in a special way.
    if (namedLineFigure.points[0].y < namedLineFigure.points[1].y)
        deviceDirection = invertedDirection(deviceDirection);

    if (deviceDirection == Direction::Right)
        return "Right";
    if (deviceDirection == Direction::Left)
        return "Left";
    if (deviceDirection == Direction::Both)
        return "BothDirections";

    NX_ASSERT(false);
    return {};
}

std::string IvaLine::buildDeviceWritingQuery(int channelNumber) const
{
    std::ostringstream query;
    if (initialized)
    {
        if (!namedLineFigure.points.empty())
        {

            using namespace SettingPrimitivesDeviceIo;
            const std::string prefix = "&Line."s + std::to_string(deviceIndex());
            query
                << "msubmenu=" << kSunapiEventName
                << "&action=" << "set"
                << "&Channel=" << channelNumber
                << prefix << ".Coordinate=" << serialize(namedLineFigure.points, m_roiResolution);
            if (m_settingsCapabilities.ivaLineRuleName)
                query << prefix << ".RuleName=" << namedLineFigure.name;
            if (m_settingsCapabilities.ivaLineObjectTypeFilter)
                query << prefix << ".ObjectTypeFilter=" << buildFilter();
            query << prefix << ".Mode=" << buildMode();
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

bool IvaArea::operator==(const IvaArea& rhs) const
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

void IvaArea::assignExclusiveFrom(const IvaArea& other)
{
    if (!m_settingsCapabilities.ivaLineRuleName)
        this->namedPolygon.name = other.namedPolygon.name;
}

void IvaArea::readExclusiveFromServerOrThrow(const nx::sdk::IStringMap* sourceMap)
{
    using namespace SettingPrimitivesServerIo;
    NamedPolygon tmpPolygon;
    deserializeOrThrow(value(sourceMap, KeyIndex::namedPolygon), &tmpPolygon);
    namedPolygon.name = tmpPolygon.name;
}

void IvaArea::readFromServerOrThrow(const nx::sdk::IStringMap* sourceMap, int /*roiIndex*/)
{
    using namespace SettingPrimitivesServerIo;
    deserializeOrThrow(value(sourceMap, KeyIndex::namedPolygon), &namedPolygon);
    if (m_settingsCapabilities.ivaAreaObjectTypeFilter)
    {
        deserializeOrThrow(value(sourceMap, KeyIndex::person), &person);
        deserializeOrThrow(value(sourceMap, KeyIndex::vehicle), &vehicle);
    }

    if (m_settingsCapabilities.ivaAreaIntrusion)
        deserializeOrThrow(value(sourceMap, KeyIndex::intrusion), &intrusion);
    if (m_settingsCapabilities.ivaAreaEnter)
        deserializeOrThrow(value(sourceMap, KeyIndex::enter), &enter);
    if (m_settingsCapabilities.ivaAreaExit)
        deserializeOrThrow(value(sourceMap, KeyIndex::exit), &exit);
    if (m_settingsCapabilities.ivaAreaAppear)
        deserializeOrThrow(value(sourceMap, KeyIndex::appear), &appear);
    if (m_settingsCapabilities.ivaAreaLoitering)
        deserializeOrThrow(value(sourceMap, KeyIndex::loitering), &loitering);
    if (m_settingsCapabilities.ivaAreaIntrusionDuration)
        deserializeOrThrow(value(sourceMap, KeyIndex::intrusionDuration), &intrusionDuration);
    if (m_settingsCapabilities.ivaAreaAppearDuration)
        deserializeOrThrow(value(sourceMap, KeyIndex::appearDuration), &appearDuration);
    if (m_settingsCapabilities.ivaAreaLoiteringDuration)
        deserializeOrThrow(value(sourceMap, KeyIndex::loiteringDuration), &loiteringDuration);
    initialized = true;
}

void IvaArea::writeToServer(nx::sdk::SettingsResponse* result, int /*roiIndex*/) const
{
    using namespace SettingPrimitivesServerIo;
    result->setValue(key(KeyIndex::namedPolygon), serialize(namedPolygon));
    if (m_settingsCapabilities.ivaAreaObjectTypeFilter)
    {
        result->setValue(key(KeyIndex::person), serialize(person));
        result->setValue(key(KeyIndex::vehicle), serialize(vehicle));
    }
    if (m_settingsCapabilities.ivaAreaIntrusion)
        result->setValue(key(KeyIndex::intrusion), serialize(intrusion));
    if (m_settingsCapabilities.ivaAreaEnter)
        result->setValue(key(KeyIndex::enter), serialize(enter));
    if (m_settingsCapabilities.ivaAreaExit)
        result->setValue(key(KeyIndex::exit), serialize(exit));
    if (m_settingsCapabilities.ivaAreaAppear)
        result->setValue(key(KeyIndex::appear), serialize(appear));
    if (m_settingsCapabilities.ivaAreaLoitering)
        result->setValue(key(KeyIndex::loitering), serialize(loitering));
    if (m_settingsCapabilities.ivaAreaIntrusionDuration)
        result->setValue(key(KeyIndex::intrusionDuration), serialize(intrusionDuration));
    if (m_settingsCapabilities.ivaAreaAppearDuration)
        result->setValue(key(KeyIndex::appearDuration), serialize(appearDuration));
    if (m_settingsCapabilities.ivaAreaLoiteringDuration)
        result->setValue(key(KeyIndex::loiteringDuration), serialize(loiteringDuration));
}

void IvaArea::readFromDeviceReplyOrThrow(const nx::kit::Json& channelInfo)
{
    nx::kit::Json roiInfo = DeviceResponseJsonParser::extractIvaRoiInfo(channelInfo, this->deviceIndex());
    if (roiInfo == nx::kit::Json())
    {
        *this = IvaArea(m_settingsCapabilities, m_roiResolution, this->nativeIndex()); // reset value;
        initialized = true;
        return ;
    }

    using namespace SettingPrimitivesDeviceIo;
    deserializeOrThrow(roiInfo, "Coordinates", m_roiResolution, &namedPolygon.points);

    if (m_settingsCapabilities.ivaAreaRuleName)
        deserializeOrThrow(roiInfo, "RuleName", m_roiResolution, &namedPolygon.name);

    if (m_settingsCapabilities.ivaAreaObjectTypeFilter)
    {
        deserializeOrThrow(roiInfo, "ObjectTypeFilter", m_roiResolution, &person, "Person");
        deserializeOrThrow(roiInfo, "ObjectTypeFilter", m_roiResolution, &vehicle, "Vehicle");
    }

    if (m_settingsCapabilities.ivaAreaIntrusion)
        deserializeOrThrow(roiInfo, "Mode", m_roiResolution, &intrusion, "Intrusion");
    if (m_settingsCapabilities.ivaAreaEnter)
        deserializeOrThrow(roiInfo, "Mode", m_roiResolution, &enter, "Entering");
    if (m_settingsCapabilities.ivaAreaExit)
        deserializeOrThrow(roiInfo, "Mode", m_roiResolution, &exit, "Exiting");
    if (m_settingsCapabilities.ivaAreaAppear)
        deserializeOrThrow(roiInfo, "Mode", m_roiResolution, &appear, "AppearDisappear");
    if (m_settingsCapabilities.ivaAreaLoitering)
        deserializeOrThrow(roiInfo, "Mode", m_roiResolution, &loitering, "Loitering");

    if (m_settingsCapabilities.ivaAreaIntrusionDuration)
        deserializeOrThrow(roiInfo, "IntrusionDuration", m_roiResolution, &intrusionDuration);
    if (m_settingsCapabilities.ivaAreaAppearDuration)
        deserializeOrThrow(roiInfo, "AppearanceDuration", m_roiResolution, &appearDuration);
    if (m_settingsCapabilities.ivaAreaLoiteringDuration)
        deserializeOrThrow(roiInfo, "LoiteringDuration", m_roiResolution, &loiteringDuration);
    initialized = true;
}

std::string IvaArea::buildMode() const
{
    std::vector<const char*> mode;

    if (m_settingsCapabilities.ivaAreaIntrusion && intrusion)
        mode.push_back("Intrusion");

    if (m_settingsCapabilities.ivaAreaEnter && enter)
        mode.push_back("Entering");

    if (m_settingsCapabilities.ivaAreaExit && exit)
        mode.push_back("Exiting");

    if (m_settingsCapabilities.ivaAreaAppear && appear)
        mode.push_back("AppearDisappear");

    if (m_settingsCapabilities.ivaAreaLoitering && loitering)
        mode.push_back("Loitering");

    return join(mode);
}

std::string IvaArea::buildFilter() const
{
    std::vector<const char*> mode;

    if (person)
        mode.push_back("Person");

    if (vehicle)
        mode.push_back("Vehicle");

    return join(mode);
}

std::string IvaArea::buildDeviceWritingQuery(int channelNumber) const
{
    std::ostringstream query;
    if (initialized)
    {
        if (!namedPolygon.points.empty())
        {
            using namespace SettingPrimitivesDeviceIo;
            const std::string prefix = "&DefinedArea."s + std::to_string(deviceIndex());
            query
                << "msubmenu=" << kSunapiEventName
                << "&action=" << "set"
                << "&Channel=" << channelNumber
                << prefix << ".Coordinate=" << serialize(namedPolygon.points, m_roiResolution)
                << prefix << ".Type=" << "Inside";

            if (m_settingsCapabilities.ivaAreaRuleName)
                query << prefix << ".RuleName=" << namedPolygon.name;

            if (m_settingsCapabilities.ivaAreaObjectTypeFilter)
                query << prefix << ".ObjectTypeFilter=" << buildFilter();

            if (m_settingsCapabilities.ivaAreaIntrusion || m_settingsCapabilities.ivaAreaEnter
                || m_settingsCapabilities.ivaAreaExit || m_settingsCapabilities.ivaAreaAppear
                || m_settingsCapabilities.ivaAreaLoitering)
            {
                query << prefix << ".Mode=" << buildMode();
            }

            if (m_settingsCapabilities.ivaAreaIntrusionDuration)
                query << prefix << ".IntrusionDuration=" << intrusionDuration;

            if (m_settingsCapabilities.ivaAreaAppearDuration)
                query << prefix << ".AppearanceDuration=" << appearDuration;
            if (m_settingsCapabilities.ivaAreaLoiteringDuration)
                query << prefix << ".LoiteringDuration=" << loiteringDuration;
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
    using namespace SettingPrimitivesServerIo;
    deserializeOrThrow(value(sourceMap, KeyIndex::unnamedPolygon), &unnamedPolygon);
    initialized = true;
}

void IvaExcludeArea::writeToServer(nx::sdk::SettingsResponse* result, int /*roiIndex*/) const
{
    using namespace SettingPrimitivesServerIo;
    result->setValue(key(KeyIndex::unnamedPolygon), serialize(unnamedPolygon));
}

void IvaExcludeArea::readFromDeviceReplyOrThrow(const nx::kit::Json& channelInfo)
{
    nx::kit::Json roiInfo = DeviceResponseJsonParser::extractIvaRoiInfo(channelInfo, this->deviceIndex());
    if (roiInfo == nx::kit::Json())
    {
        *this = IvaExcludeArea(m_settingsCapabilities, m_roiResolution, this->nativeIndex());
        initialized = true;
        return;
    }

    using namespace SettingPrimitivesDeviceIo;
    deserializeOrThrow(roiInfo, "Coordinates", m_roiResolution, &unnamedPolygon.points);
    initialized = true;
}

std::string IvaExcludeArea::buildDeviceWritingQuery(int channelNumber) const
{
    std::ostringstream query;
    if (initialized)
    {
        if (!unnamedPolygon.points.empty())
        {
            using namespace SettingPrimitivesDeviceIo;
            const std::string prefix = "&DefinedArea."s + std::to_string(deviceIndex());
            query
                << "msubmenu=" << kSunapiEventName
                << "&action=" << "set"
                << "&Channel=" << channelNumber
                << prefix << ".Coordinate=" << serialize(unnamedPolygon.points, m_roiResolution)
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
    using namespace SettingPrimitivesServerIo;
    deserializeOrThrow(value(sourceMap, KeyIndex::enabled), &enabled);
    deserializeOrThrow(value(sourceMap, KeyIndex::thresholdLevel), &thresholdLevel);
    initialized = true;
}

void AudioDetection::writeToServer(nx::sdk::SettingsResponse* result, int /*roiIndex*/) const
{
    using namespace SettingPrimitivesServerIo;
    result->setValue(key(KeyIndex::enabled), serialize(enabled));
    result->setValue(key(KeyIndex::thresholdLevel), serialize(thresholdLevel));
}

void AudioDetection::readFromDeviceReplyOrThrow(const nx::kit::Json& channelInfo)
{
    using namespace SettingPrimitivesDeviceIo;
    deserializeOrThrow(channelInfo, "Enable", m_roiResolution, &enabled);
    deserializeOrThrow(channelInfo, "InputThresholdLevel", m_roiResolution, &thresholdLevel);
    initialized = true;
}

std::string AudioDetection::buildDeviceWritingQuery(int channelNumber) const
{
    std::ostringstream query;
    if (initialized)
    {
        using namespace SettingPrimitivesDeviceIo;
        query
            << "msubmenu=" << kSunapiEventName
            << "&action=" << "set"
            << "&Channel=" << channelNumber
            << "&Enable=" << serialize(enabled)
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
    using namespace SettingPrimitivesServerIo;
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
    using namespace SettingPrimitivesServerIo;
    result->setValue(key(KeyIndex::enabled), serialize(enabled));
    result->setValue(key(KeyIndex::noisefilter), serialize(noisefilter));
    result->setValue(key(KeyIndex::thresholdLevel), serialize(thresholdLevel));
    result->setValue(key(KeyIndex::scream), serialize(scream));
    result->setValue(key(KeyIndex::gunshot), serialize(gunshot));
    result->setValue(key(KeyIndex::explosion), serialize(explosion));
    result->setValue(key(KeyIndex::crashingGlass), serialize(crashingGlass));
}

void SoundClassification::readFromDeviceReplyOrThrow(
    const nx::kit::Json& channelInfo)
{
    using namespace SettingPrimitivesDeviceIo;
    deserializeOrThrow(channelInfo, "Enable", m_roiResolution, &enabled);
    deserializeOrThrow(channelInfo, "NoiseReduction", m_roiResolution, &noisefilter);
    deserializeOrThrow(channelInfo, "ThresholdLevel", m_roiResolution, &thresholdLevel);
    deserializeOrThrow(channelInfo, "SoundType", m_roiResolution, &scream, "Scream");
    deserializeOrThrow(channelInfo, "SoundType", m_roiResolution, &gunshot, "Gunshot");
    deserializeOrThrow(channelInfo, "SoundType", m_roiResolution, &explosion, "Explosion");
    deserializeOrThrow(channelInfo, "SoundType", m_roiResolution, &crashingGlass, "GlassBreak");
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

    return join(mode);
}

std::string SoundClassification::buildDeviceWritingQuery(int channelNumber) const
{
    std::ostringstream query;
    if (initialized)
    {
        using namespace SettingPrimitivesDeviceIo;
        query
            << "msubmenu=" << kSunapiEventName
            << "&action=" << "set"
            << "&Channel=" << channelNumber
            << "&Enable=" << serialize(enabled)
            << "&NoiseReduction=" << serialize(noisefilter)
            << "&ThresholdLevel=" << thresholdLevel
            << "&SoundType=" << buildSoundType()
            ;
    }
    return query.str();
}

//-------------------------------------------------------------------------------------------------

bool FaceMaskDetection::operator==(const FaceMaskDetection& rhs) const
{
    return initialized == rhs.initialized
        && enabled == rhs.enabled
        && detectionMode == rhs.detectionMode;
}

/*static*/
void FaceMaskDetection::deserializeDetectionModeOrThrow(
    const char* serializedDetectionMode, DetectionMode* outDetectionMode)
{
    using namespace SettingPrimitivesServerIo;

    if (strcmp(serializedDetectionMode, kMaskDetectionMode) == 0)
    {
        *outDetectionMode = DetectionMode::mask;
        return;
    }

    if (strcmp(serializedDetectionMode, kNoMaskDetectionMode) == 0)
    {
        *outDetectionMode = DetectionMode::noMask;
        return;
    }

    throw DeserializationError();
}

void FaceMaskDetection::readFromServerOrThrow(
    const nx::sdk::IStringMap* settings, int /*roiIndex*/)
{
    using namespace SettingPrimitivesServerIo;
    deserializeOrThrow(value(settings, KeyIndex::enabled), &enabled);
    deserializeDetectionModeOrThrow(value(settings, KeyIndex::detectionMode), &detectionMode);
    initialized = true;
}

void FaceMaskDetection::writeToServer(
    nx::sdk::SettingsResponse* settingsDestination, int /*roiIndex*/) const
{
    using namespace SettingPrimitivesServerIo;
    settingsDestination->setValue(key(KeyIndex::enabled), serialize(enabled));
    settingsDestination->setValue(key(KeyIndex::detectionMode), buildDetectionMode());
}

void FaceMaskDetection::readFromDeviceReplyOrThrow(
    const nx::kit::Json& channelInfo)
{
    using namespace SettingPrimitivesDeviceIo;
    deserializeOrThrow(channelInfo, "Enable", m_roiResolution, &enabled);

    std::string detectionModeString;
    deserializeOrThrow(channelInfo, "DetectionMode", m_roiResolution, &detectionModeString);
    deserializeDetectionModeOrThrow(detectionModeString.c_str(), &detectionMode);
    initialized = true;
}

std::string FaceMaskDetection::buildDeviceWritingQuery(int channelNumber) const
{
    std::ostringstream query;
    if (initialized)
    {
        using namespace SettingPrimitivesDeviceIo;
        query
            << "msubmenu=" << kSunapiEventName
            << "&action=" << "set"
            << "&Channel=" << channelNumber
            << "&Enable=" << serialize(enabled)
            << "&DetectionMode=" << buildDetectionMode();
    }
    return query.str();
}

std::string FaceMaskDetection::buildDetectionMode() const
{
    return detectionMode == DetectionMode::mask
        ? kMaskDetectionMode
        : kNoMaskDetectionMode;
}

//-------------------------------------------------------------------------------------------------

bool TemperatureChangeDetectionItem::operator==(const TemperatureChangeDetectionItem& rhs) const
{
    return initialized == rhs.initialized
        && unnamedRect == rhs.unnamedRect
        && temperatureType == rhs.temperatureType
        && detectionType == rhs.detectionType
        && thresholdTemperature == rhs.thresholdTemperature
        && duration == rhs.duration
        && areaEmissivity == rhs.areaEmissivity;
}

void TemperatureChangeDetectionItem::readFromServerOrThrow(
    const nx::sdk::IStringMap* sourceMap, int /*roiIndex*/)
{
    using namespace SettingPrimitivesServerIo;
    if (m_settingsCapabilities.temperatureDetection.coordinate)
        deserializeOrThrow(value(sourceMap, KeyIndex::unnamedRect), &unnamedRect);

    if (m_settingsCapabilities.temperatureDetection.temperatureType)
        deserializeOrThrow(value(sourceMap, KeyIndex::temperatureType), &temperatureType);

    if (m_settingsCapabilities.temperatureDetection.detectionType)
        deserializeOrThrow(value(sourceMap, KeyIndex::detectionType), &detectionType);

    if (m_settingsCapabilities.temperatureDetection.thresholdTemperature)
        deserializeOrThrow(value(sourceMap, KeyIndex::thresholdTemperature), &thresholdTemperature);

    if (m_settingsCapabilities.temperatureDetection.duration)
        deserializeOrThrow(value(sourceMap, KeyIndex::duration), &duration);

    if (m_settingsCapabilities.temperatureDetection.areaEmissivity)
        deserializeOrThrow(value(sourceMap, KeyIndex::areaEmissivity), &areaEmissivity);
    initialized = true;
}

void TemperatureChangeDetectionItem::writeToServer(
    nx::sdk::SettingsResponse* result, int /*roiIndex*/) const
{
    using namespace SettingPrimitivesServerIo;
    if (m_settingsCapabilities.temperatureDetection.coordinate)
        result->setValue(key(KeyIndex::unnamedRect), serialize(unnamedRect));

    if (m_settingsCapabilities.temperatureDetection.temperatureType)
        result->setValue(key(KeyIndex::temperatureType), serialize(temperatureType));

    if (m_settingsCapabilities.temperatureDetection.detectionType)
        result->setValue(key(KeyIndex::detectionType), serialize(detectionType));

    if (m_settingsCapabilities.temperatureDetection.thresholdTemperature)
        result->setValue(key(KeyIndex::thresholdTemperature), serialize(thresholdTemperature));

    if (m_settingsCapabilities.temperatureDetection.duration)
        result->setValue(key(KeyIndex::duration), serialize(duration));

    if (m_settingsCapabilities.temperatureDetection.areaEmissivity)
        result->setValue(key(KeyIndex::areaEmissivity), serialize(areaEmissivity));
}

void TemperatureChangeDetectionItem::readFromDeviceReplyOrThrow(const nx::kit::Json& channelInfo)
{
    nx::kit::Json roiInfo =
        DeviceResponseJsonParser::extractTemperatureRoiInfo(channelInfo, this->deviceIndex());
    if (roiInfo == nx::kit::Json())
    {
        *this = TemperatureChangeDetectionItem(
            m_settingsCapabilities, m_roiResolution, this->nativeIndex()); // reset value;
        initialized = true;
        return;
    }

    using namespace SettingPrimitivesDeviceIo;

    if (m_settingsCapabilities.temperatureDetection.coordinate)
        deserializeOrThrow(roiInfo, "Coordinates", m_roiResolution, &unnamedRect.points);

    if (m_settingsCapabilities.temperatureDetection.temperatureType)
        deserializeOrThrow(roiInfo, "TemperatureType", m_roiResolution, &temperatureType);
    if (m_settingsCapabilities.temperatureDetection.detectionType)
        deserializeOrThrow(roiInfo, "DetectionType", m_roiResolution, &detectionType);
    if (m_settingsCapabilities.temperatureDetection.thresholdTemperature)
        deserializeOrThrow(roiInfo, "ThresholdTemperature", m_roiResolution, &thresholdTemperature);
    if (m_settingsCapabilities.temperatureDetection.duration)
        deserializeOrThrow(roiInfo, "Duration", m_roiResolution, &duration);
    if (m_settingsCapabilities.temperatureDetection.areaEmissivity)
        deserializeOrThrow(roiInfo, "NormalizedEmissivity", m_roiResolution, &areaEmissivity);
    initialized = true;
}

std::string TemperatureChangeDetectionItem::buildDeviceWritingQuery(int channelNumber) const
{
    std::ostringstream query;
    if (initialized)
    {
        if (!unnamedRect.points.empty())
        {
            using namespace SettingPrimitivesDeviceIo;
            const std::string prefix = "&ROI."s + std::to_string(deviceIndex());
            query << "msubmenu=" << kSunapiEventName << "&action="
                  << "set"
                  << "&Channel=" << channelNumber;

            if (m_settingsCapabilities.temperatureDetection.coordinate)
                query << prefix << ".Coordinate=" << serialize(unnamedRect.points, m_roiResolution);

            if (m_settingsCapabilities.temperatureDetection.temperatureType)
                query << prefix << ".TemperatureType=" << temperatureType;

            if (m_settingsCapabilities.temperatureDetection.detectionType)
                query << prefix << ".DetectionType=" << detectionType;

            if (m_settingsCapabilities.temperatureDetection.thresholdTemperature)
                query << prefix << ".ThresholdTemperature=" << thresholdTemperature;

            if (m_settingsCapabilities.temperatureDetection.duration)
                query << prefix << ".Duration=" << duration;

            if (m_settingsCapabilities.temperatureDetection.duration)
                query << prefix << ".NormalizedEmissivity=" << areaEmissivity;

            //query << "TemperatureUnit=" << "Fahrenheit"; //"Celsius"
        }
        else
        {
            query << "msubmenu=" << kSunapiEventName << "&action="
                  << "remove"
                  << "&Channel=" << channelNumber
                  << "&ROIIndex=" << deviceIndex();
        }
    }
    return query.str();
}

//-------------------------------------------------------------------------------------------------

bool TemperatureChangeDetectionToggle::operator==(
    const TemperatureChangeDetectionToggle& rhs) const
{
    return initialized == rhs.initialized
        && enabled == rhs.enabled;
}

void TemperatureChangeDetectionToggle::readFromServerOrThrow(
    const nx::sdk::IStringMap* sourceMap, int /*roiIndex*/)
{
    using namespace SettingPrimitivesServerIo;
    deserializeOrThrow(value(sourceMap, KeyIndex::enabled), &enabled);

    initialized = true;
}

void TemperatureChangeDetectionToggle::writeToServer(
    nx::sdk::SettingsResponse* result, int /*roiIndex*/) const
{
    using namespace SettingPrimitivesServerIo;
    result->setValue(key(KeyIndex::enabled), serialize(enabled));
}

void TemperatureChangeDetectionToggle::readFromDeviceReplyOrThrow(const nx::kit::Json& channelInfo)
{
    enabled = DeviceResponseJsonParser::extractTemperatureChangeDetectionToggle(
        channelInfo, this->deviceIndex()).value_or(false);
    initialized = true;
}

std::string TemperatureChangeDetectionToggle::buildDeviceWritingQuery(int channelNumber) const
{
    std::ostringstream query;
    if (initialized)
    {
            using namespace SettingPrimitivesDeviceIo;
            query << "msubmenu=" << kSunapiEventName
                << "&action=" << "set"
                << "&Channel=" << channelNumber
                << "&Enable=" << serialize(enabled);
    }
    return query.str();
}

//-------------------------------------------------------------------------------------------------

} // namespace nx::vms_server_plugins::analytics::hanwha
