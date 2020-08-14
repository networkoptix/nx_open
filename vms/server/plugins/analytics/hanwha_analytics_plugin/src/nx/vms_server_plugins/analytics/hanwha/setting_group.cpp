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

void AnalyticsMode::readFromDeviceReplyOrThrow(const nx::kit::Json& channelInfo, FrameSize frameSize)
{
    using namespace SettingPrimitivesDeviceIo;
    deserializeOrThrow(channelInfo, "DetectionType", frameSize, &detectionType);
    initialized = true;
}

std::string AnalyticsMode::buildDeviceWritingQuery(FrameSize /*frameSize*/, int channelNumber) const
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

#if 1
// Hanwha analyticsMode detection support is currently removed from the Clients interface

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
    using namespace SettingPrimitivesServerIo;
    deserializeOrThrow(value(sourceMap, KeyIndex::constraints), &constraints);
    initialized = true;
}

void MotionDetectionObjectSize::writeToServer(
    nx::sdk::SettingsResponse* result, int /*roiIndex*/) const
{
    using namespace SettingPrimitivesServerIo;
    result->setValue(key(KeyIndex::constraints), serialize(constraints));
}

void MotionDetectionObjectSize::readFromDeviceReplyOrThrow(const nx::kit::Json& channelInfo, FrameSize frameSize)
{
    using namespace SettingPrimitivesDeviceIo;
    nx::kit::Json objectSizeInfo = DeviceResponseJsonParser::extractObjectSizeInfo(channelInfo, "MotionDetection");
    deserializeOrThrow(objectSizeInfo, frameSize, &constraints);
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

std::string MotionDetectionObjectSize::buildDeviceWritingQuery(FrameSize frameSize, int channelNumber) const
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
    using namespace SettingPrimitivesServerIo;
    deserializeOrThrow(value(sourceMap, KeyIndex::unnamedPolygon), &unnamedPolygon);
    deserializeOrThrow(value(sourceMap, KeyIndex::thresholdLevel), &thresholdLevel);
    deserializeOrThrow(value(sourceMap, KeyIndex::sensitivityLevel), &sensitivityLevel);
    deserializeOrThrow(value(sourceMap, KeyIndex::minimumDuration), &minimumDuration);
    initialized = true;
}

void MotionDetectionIncludeArea::writeToServer(nx::sdk::SettingsResponse* result, int /*roiIndex*/) const
{
    using namespace SettingPrimitivesServerIo;
    result->setValue(key(KeyIndex::unnamedPolygon), serialize(unnamedPolygon));
    result->setValue(key(KeyIndex::thresholdLevel), serialize(thresholdLevel));
    result->setValue(key(KeyIndex::sensitivityLevel), serialize(sensitivityLevel));
    result->setValue(key(KeyIndex::minimumDuration), serialize(minimumDuration));
}

void MotionDetectionIncludeArea::readFromDeviceReplyOrThrow(const nx::kit::Json& channelInfo, FrameSize frameSize)
{
    nx::kit::Json roiInfo = DeviceResponseJsonParser::extractMdRoiInfo(channelInfo, this->deviceIndex());
    if (roiInfo == nx::kit::Json())
    {
        // No roi info found for current channel => *this should be set into default state.
        // Current SettingGroup is considered to be uninitialized.
        *this = MotionDetectionIncludeArea(this->nativeIndex());
        initialized = true;
        return;
    }

    using namespace SettingPrimitivesDeviceIo;
    deserializeOrThrow(roiInfo, "Coordinates", frameSize, &unnamedPolygon);
    deserializeOrThrow(roiInfo, "ThresholdLevel", frameSize, &thresholdLevel);
    deserializeOrThrow(roiInfo, "SensitivityLevel", frameSize, &sensitivityLevel);
    deserializeOrThrow(roiInfo, "Duration", frameSize, &minimumDuration);
    initialized = true;
}

std::string MotionDetectionIncludeArea::buildDeviceWritingQuery(FrameSize frameSize, int channelNumber) const
{
    std::ostringstream query;
    if (initialized)
    {
        if (!unnamedPolygon.points.empty())
        {
            using namespace SettingPrimitivesDeviceIo;
            const std::string prefix = "&ROI."s + std::to_string(deviceIndex());
            query
                << "msubmenu=" << kSunapiEventName
                << "&action=" << "set"
                << "&Channel=" << channelNumber
                << prefix << ".Coordinate=" << serialize(unnamedPolygon.points, frameSize)
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
    using namespace SettingPrimitivesServerIo;
    deserializeOrThrow(value(sourceMap, KeyIndex::unnamedPolygon), &unnamedPolygon);
    initialized = true;
}

void MotionDetectionExcludeArea::writeToServer(nx::sdk::SettingsResponse* result, int /*roiIndex*/) const
{
    using namespace SettingPrimitivesServerIo;
    result->setValue(key(KeyIndex::unnamedPolygon), serialize(unnamedPolygon));
}

void MotionDetectionExcludeArea::readFromDeviceReplyOrThrow(const nx::kit::Json& channelInfo, FrameSize frameSize)
{
    nx::kit::Json roiInfo = DeviceResponseJsonParser::extractMdRoiInfo(channelInfo, this->deviceIndex());
    if (roiInfo == nx::kit::Json(this->deviceIndex()))
    {
        *this = MotionDetectionExcludeArea(this->nativeIndex());
        initialized = true;
        return;
    }

    using namespace SettingPrimitivesDeviceIo;
    deserializeOrThrow(roiInfo, "Coordinates", frameSize, &unnamedPolygon);
    initialized = true;
}

std::string MotionDetectionExcludeArea::buildDeviceWritingQuery(FrameSize frameSize, int channelNumber) const
{
    std::ostringstream query;
    if (initialized)
    {
        if (!unnamedPolygon.points.empty())
        {
            using namespace SettingPrimitivesDeviceIo;
            const std::string prefix = "&ROI."s + std::to_string(deviceIndex());
            query
                << "msubmenu=" << kSunapiEventName
                << "&action=" << "set"
                << "&Channel=" << channelNumber
                << prefix << ".Coordinate=" << serialize(unnamedPolygon.points, frameSize)
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

#endif
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

void ShockDetection::readFromDeviceReplyOrThrow(const nx::kit::Json& channelInfo, FrameSize frameSize)
{
    using namespace SettingPrimitivesDeviceIo;
    deserializeOrThrow(channelInfo, "Enable", frameSize, &enabled);
    deserializeOrThrow(channelInfo, "ThresholdLevel", frameSize, &thresholdLevel);
    deserializeOrThrow(channelInfo, "Sensitivity", frameSize, &sensitivityLevel);
    initialized = true;
}

std::string ShockDetection::buildDeviceWritingQuery(FrameSize /*frameSize*/, int channelNumber) const
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

void TamperingDetection::readFromDeviceReplyOrThrow(const nx::kit::Json& channelInfo, FrameSize frameSize)
{
    using namespace SettingPrimitivesDeviceIo;
    deserializeOrThrow(channelInfo, "Enable", frameSize, &enabled);
    deserializeOrThrow(channelInfo, "ThresholdLevel", frameSize, &thresholdLevel);
    deserializeOrThrow(channelInfo, "SensitivityLevel", frameSize, &sensitivityLevel);
    deserializeOrThrow(channelInfo, "Duration", frameSize, &minimumDuration);
    deserializeOrThrow(channelInfo, "DarknessDetection", frameSize, &exceptDarkImages);
    initialized = true;
}

std::string TamperingDetection::buildDeviceWritingQuery(FrameSize /*frameSize*/, int channelNumber) const
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

void DefocusDetection::readFromDeviceReplyOrThrow(const nx::kit::Json& channelInfo, FrameSize frameSize)
{
    using namespace SettingPrimitivesDeviceIo;
    deserializeOrThrow(channelInfo, "Enable", frameSize, &enabled);
    deserializeOrThrow(channelInfo, "ThresholdLevel", frameSize, &thresholdLevel);
    deserializeOrThrow(channelInfo, "Sensitivity", frameSize, &sensitivityLevel);
    deserializeOrThrow(channelInfo, "Duration", frameSize, &minimumDuration);
    initialized = true;
}

std::string DefocusDetection::buildDeviceWritingQuery(FrameSize /*frameSize*/, int channelNumber) const
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

void FogDetection::readFromDeviceReplyOrThrow(const nx::kit::Json& channelInfo, FrameSize frameSize)
{
    using namespace SettingPrimitivesDeviceIo;
    deserializeOrThrow(channelInfo, "Enable", frameSize, &enabled);
    deserializeOrThrow(channelInfo, "ThresholdLevel", frameSize, &thresholdLevel);
    deserializeOrThrow(channelInfo, "SensitivityLevel", frameSize, &sensitivityLevel);
    deserializeOrThrow(channelInfo, "Duration", frameSize, &minimumDuration);
    initialized = true;
}

std::string FogDetection::buildDeviceWritingQuery(FrameSize /*frameSize*/, int channelNumber) const
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

void ObjectDetectionGeneral::readFromDeviceReplyOrThrow(const nx::kit::Json& channelInfo, FrameSize frameSize)
{
    using namespace SettingPrimitivesDeviceIo;
    deserializeOrThrow(channelInfo, "Enable", frameSize, &enabled);
    deserializeOrThrow(channelInfo, "ObjectTypes", frameSize, &person, "Person");
    deserializeOrThrow(channelInfo, "ObjectTypes", frameSize, &vehicle, "Vehicle");
    deserializeOrThrow(channelInfo, "ObjectTypes", frameSize, &face, "Face");
    deserializeOrThrow(channelInfo, "ObjectTypes", frameSize, &licensePlate, "LicensePlate");
    deserializeOrThrow(channelInfo, "Duration", frameSize, &minimumDuration);
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

std::string ObjectDetectionGeneral::buildDeviceWritingQuery(FrameSize /*frameSize*/, int channelNumber) const
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

void ObjectDetectionBestShot::readFromDeviceReplyOrThrow(const nx::kit::Json& channelInfo, FrameSize frameSize)
{
    using namespace SettingPrimitivesDeviceIo;
    deserializeOrThrow(channelInfo, "ObjectTypes", frameSize, &person, "Person");
    deserializeOrThrow(channelInfo, "ObjectTypes", frameSize, &vehicle, "Vehicle");
    deserializeOrThrow(channelInfo, "ObjectTypes", frameSize, &face, "Face");
    deserializeOrThrow(channelInfo, "ObjectTypes", frameSize, &licensePlate, "LicensePlate");
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

std::string ObjectDetectionBestShot::buildDeviceWritingQuery(FrameSize /*frameSize*/, int channelNumber) const
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

void IvaObjectSize::readFromDeviceReplyOrThrow(const nx::kit::Json& channelInfo, FrameSize frameSize)
{
    using namespace SettingPrimitivesDeviceIo;
    nx::kit::Json objectSizeInfo = DeviceResponseJsonParser::extractObjectSizeInfo(channelInfo, "IntelligentVideo");
    deserializeOrThrow(objectSizeInfo, frameSize, &constraints);
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

std::string IvaObjectSize::buildDeviceWritingQuery(FrameSize frameSize, int channelNumber) const
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
    using namespace SettingPrimitivesServerIo;
    deserializeOrThrow(value(sourceMap, KeyIndex::namedLineFigure), &namedLineFigure);
    deserializeOrThrow(value(sourceMap, KeyIndex::person), &person);
    deserializeOrThrow(value(sourceMap, KeyIndex::vehicle), &vehicle);
    deserializeOrThrow(value(sourceMap, KeyIndex::crossing), &crossing);

    initialized = true;
}

void IvaLine::writeToServer(nx::sdk::SettingsResponse* result, int /*roiIndex*/) const
{
    using namespace SettingPrimitivesServerIo;
    result->setValue(key(KeyIndex::namedLineFigure), serialize(namedLineFigure));
    result->setValue(key(KeyIndex::person), serialize(person));
    result->setValue(key(KeyIndex::vehicle), serialize(vehicle));
    result->setValue(key(KeyIndex::crossing), serialize(crossing));
}

void IvaLine::readFromDeviceReplyOrThrow(const nx::kit::Json& channelInfo, FrameSize frameSize)
{
    nx::kit::Json lineInfo = DeviceResponseJsonParser::extractIvaLineInfo(channelInfo, this->deviceIndex());
    if (lineInfo == nx::kit::Json())
    {
        *this = IvaLine(this->nativeIndex()); // reset value;
        initialized = true;
        return;
    }

    using namespace SettingPrimitivesDeviceIo;
    deserializeOrThrow(lineInfo, "Coordinates", frameSize, &namedLineFigure.points);

    deserializeOrThrow(lineInfo, "Mode", frameSize, &namedLineFigure.direction);

    // Direction in the plugin and direction in the device are defined in a different way
    // After reading direction from the device the direction should be treated in a special way.
    if (namedLineFigure.points[0].y < namedLineFigure.points[1].y)
        namedLineFigure.direction = invertedDirection(namedLineFigure.direction);

    deserializeOrThrow(lineInfo, "RuleName", frameSize, &namedLineFigure.name);
    deserializeOrThrow(lineInfo, "ObjectTypeFilter", frameSize, &person, "Person");
    deserializeOrThrow(lineInfo, "ObjectTypeFilter", frameSize, &vehicle, "Vehicle");

    std::string Crossing;
    deserializeOrThrow(lineInfo, "Mode", frameSize, &Crossing);
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

std::string IvaLine::buildMode(bool inverted /*= false*/) const
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

std::string IvaLine::buildDeviceWritingQuery(FrameSize frameSize, int channelNumber) const
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
                << prefix << ".Coordinate=" << serialize(namedLineFigure.points, frameSize)
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

void IvaArea::readFromServerOrThrow(const nx::sdk::IStringMap* sourceMap, int /*roiIndex*/)
{
    using namespace SettingPrimitivesServerIo;
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

void IvaArea::writeToServer(nx::sdk::SettingsResponse* result, int /*roiIndex*/) const
{
    using namespace SettingPrimitivesServerIo;
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

void IvaArea::readFromDeviceReplyOrThrow(const nx::kit::Json& channelInfo, FrameSize frameSize)
{
    nx::kit::Json roiInfo = DeviceResponseJsonParser::extractIvaRoiInfo(channelInfo, this->deviceIndex());
    if (roiInfo == nx::kit::Json())
    {
        *this = IvaArea(this->nativeIndex()); // reset value;
        initialized = true;
        return ;
    }

    using namespace SettingPrimitivesDeviceIo;
    deserializeOrThrow(roiInfo, "Coordinates", frameSize, &namedPolygon.points);
    deserializeOrThrow(roiInfo, "RuleName", frameSize, &namedPolygon.name);
    deserializeOrThrow(roiInfo, "ObjectTypeFilter", frameSize, &person, "Person");
    deserializeOrThrow(roiInfo, "ObjectTypeFilter", frameSize, &vehicle, "Vehicle");

    deserializeOrThrow(roiInfo, "Mode", frameSize, &intrusion, "Intrusion");
    deserializeOrThrow(roiInfo, "Mode", frameSize, &enter, "Entering");
    deserializeOrThrow(roiInfo, "Mode", frameSize, &exit, "Exiting");
    deserializeOrThrow(roiInfo, "Mode", frameSize, &appear, "AppearDisappear");
    deserializeOrThrow(roiInfo, "Mode", frameSize, &loitering, "Loitering");

    deserializeOrThrow(roiInfo, "IntrusionDuration", frameSize, &intrusionDuration);
    deserializeOrThrow(roiInfo, "AppearanceDuration", frameSize, &appearDuration);
    deserializeOrThrow(roiInfo, "LoiteringDuration", frameSize, &loiteringDuration);
    initialized = true;
}

std::string IvaArea::buildMode() const
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

std::string IvaArea::buildDeviceWritingQuery(FrameSize frameSize, int channelNumber) const
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
                << prefix << ".Coordinate=" << serialize(namedPolygon.points, frameSize)
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
    using namespace SettingPrimitivesServerIo;
    deserializeOrThrow(value(sourceMap, KeyIndex::unnamedPolygon), &unnamedPolygon);
    initialized = true;
}

void IvaExcludeArea::writeToServer(nx::sdk::SettingsResponse* result, int /*roiIndex*/) const
{
    using namespace SettingPrimitivesServerIo;
    result->setValue(key(KeyIndex::unnamedPolygon), serialize(unnamedPolygon));
}

void IvaExcludeArea::readFromDeviceReplyOrThrow(const nx::kit::Json& channelInfo, FrameSize frameSize)
{
    nx::kit::Json roiInfo = DeviceResponseJsonParser::extractIvaRoiInfo(channelInfo, this->deviceIndex());
    if (roiInfo == nx::kit::Json())
    {
        *this = IvaExcludeArea(this->nativeIndex());
        initialized = true;
        return;
    }

    using namespace SettingPrimitivesDeviceIo;
    deserializeOrThrow(roiInfo, "Coordinates", frameSize, &unnamedPolygon.points);
    initialized = true;
}

std::string IvaExcludeArea::buildDeviceWritingQuery(FrameSize frameSize, int channelNumber) const
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
                << prefix << ".Coordinate=" << serialize(unnamedPolygon.points, frameSize)
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

void AudioDetection::readFromDeviceReplyOrThrow(const nx::kit::Json& channelInfo, FrameSize frameSize)
{
    using namespace SettingPrimitivesDeviceIo;
    deserializeOrThrow(channelInfo, "Enable", frameSize, &enabled);
    deserializeOrThrow(channelInfo, "InputThresholdLevel", frameSize, &thresholdLevel);
    initialized = true;
}

std::string AudioDetection::buildDeviceWritingQuery(FrameSize /*frameSize*/, int channelNumber) const
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
    const nx::kit::Json& channelInfo, FrameSize frameSize)
{
    using namespace SettingPrimitivesDeviceIo;
    deserializeOrThrow(channelInfo, "Enable", frameSize, &enabled);
    deserializeOrThrow(channelInfo, "NoiseReduction", frameSize, &noisefilter);
    deserializeOrThrow(channelInfo, "ThresholdLevel", frameSize, &thresholdLevel);
    deserializeOrThrow(channelInfo, "SoundType", frameSize, &scream, "Scream");
    deserializeOrThrow(channelInfo, "SoundType", frameSize, &gunshot, "Gunshot");
    deserializeOrThrow(channelInfo, "SoundType", frameSize, &explosion, "Explosion");
    deserializeOrThrow(channelInfo, "SoundType", frameSize, &crashingGlass, "GlassBreak");
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

std::string SoundClassification::buildDeviceWritingQuery(FrameSize /*frameSize*/, int channelNumber) const
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
    const nx::kit::Json& channelInfo, FrameSize frameSize)
{
    using namespace SettingPrimitivesDeviceIo;
    deserializeOrThrow(channelInfo, "Enable", frameSize, &enabled);

    std::string detectionModeString;
    deserializeOrThrow(channelInfo, "DetectionMode", frameSize, &detectionModeString);
    deserializeDetectionModeOrThrow(detectionModeString.c_str(), &detectionMode);
    initialized = true;
}

std::string FaceMaskDetection::buildDeviceWritingQuery(
    FrameSize /*frameSize*/, int channelNumber) const
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

} // namespace nx::vms_server_plugins::analytics::hanwha
