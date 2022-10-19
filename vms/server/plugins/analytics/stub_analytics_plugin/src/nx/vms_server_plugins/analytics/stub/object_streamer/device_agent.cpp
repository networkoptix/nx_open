// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_agent.h"

#include <fstream>
#include <streambuf>

#include <nx/sdk/analytics/helpers/object_metadata.h>
#include <nx/sdk/analytics/helpers/object_metadata_packet.h>
#include <nx/sdk/helpers/uuid_helper.h>
#include <nx/sdk/helpers/settings_response.h>

#include <nx/kit/json.h>
#include <nx/kit/debug.h>

#include "../utils.h"
#include "stub_analytics_plugin_object_streamer_ini.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace object_streamer {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

using nx::kit::Json;

static const std::string kTypeIdField = "typeId";
static const std::string kTrackIdField = "trackId";
static const std::string kFrameNumberField = "frameNumber";
static const std::string kAttributesField = "attributes";
static const std::string kBoundingBoxField = "boundingBox";
static const std::string kTopLeftXField = "x";
static const std::string kTopLeftYfield = "y";
static const std::string kWidthField = "width";
static const std::string kHeightField = "height";
static const std::string kTimestampUsField = "timestampUs";

static const std::string kDefaultManifestFile = "manifest.json";
static const std::string kDefaultStreamFile = "stream.json";

static const std::string kManifestFileSetting = "manifestFile";
static const std::string kStreamFileSetting = "streamFile";

static const std::string kObjectTypeFilterPrefix = "object_type_filter_";

static std::string defaultManifestFilePath(const std::string& pluginHomeDir)
{
    return pluginHomeDir + "/" + kDefaultManifestFile;
}

static std::string defaultStreamFilePath(const std::string& pluginHomeDir)
{
    return pluginHomeDir + "/" + kDefaultStreamFile;
}

static std::string makeObjectTypeFilterSettingName(const std::string& objectTypeId)
{
    return kObjectTypeFilterPrefix + objectTypeId;
}

static std::string readFileToString(const std::string& filePath)
{
    std::ifstream t(filePath);
    return std::string(std::istreambuf_iterator<char>(t), std::istreambuf_iterator<char>());
}

DeviceAgent::DeviceAgent(const nx::sdk::IDeviceInfo* deviceInfo, std::string pluginHomeDir):
    ConsumingDeviceAgent(deviceInfo, ini().enableOutput),
    m_pluginHomeDir(std::move(pluginHomeDir))
{
}

DeviceAgent::~DeviceAgent()
{
}

std::string DeviceAgent::manifestString() const
{
    // Manifest is provided via "pushManifest" call.
    return "{}";
}

bool DeviceAgent::pushCompressedVideoFrame(const ICompressedVideoPacket* videoPacket)
{
    if (m_lastFrameTimestampUs >= 0)
    {
        for (auto& objectMetdataPacket: generateObjects(
            m_frameNumber == 0 ? m_maxFrameNumber : m_frameNumber - 1,
            m_lastFrameTimestampUs,
            std::max(videoPacket->timestampUs() - m_lastFrameTimestampUs, (int64_t) 0)))
        {
            pushMetadataPacket(objectMetdataPacket.releasePtr());
        }
    }

    ++m_frameNumber;
    if (m_frameNumber > m_maxFrameNumber)
        m_frameNumber = 0;

    m_lastFrameTimestampUs = videoPacket->timestampUs();

    return true;
}

std::vector<Ptr<IObjectMetadataPacket>> DeviceAgent::generateObjects(
    int frameNumber, int64_t frameTimestampUs, int64_t durationUs)
{
    std::vector<Ptr<IObjectMetadataPacket>> result;

    if (m_streamInfo.objectsByFrameNumber.find(frameNumber) == m_streamInfo.objectsByFrameNumber.cend())
        return result;

    std::map<int64_t, Ptr<ObjectMetadataPacket>> objectMetadataPacketByTimestamp;
    for (const Object& object: m_streamInfo.objectsByFrameNumber[frameNumber])
    {
        if (m_disabledObjectTypeIds.find(object.typeId) != m_disabledObjectTypeIds.cend())
            continue;

        const int64_t timestampUs = object.timestampUs >= 0
            ? object.timestampUs
            : frameTimestampUs;

        Ptr<ObjectMetadataPacket>& objectMetadataPacket =
            objectMetadataPacketByTimestamp[timestampUs];

        if (!objectMetadataPacket)
        {
            objectMetadataPacket = makePtr<ObjectMetadataPacket>();
            objectMetadataPacket->setTimestampUs(timestampUs);
            objectMetadataPacket->setDurationUs(durationUs);
        }

        auto objectMetadata = makePtr<ObjectMetadata>();
        objectMetadata->setTypeId(object.typeId);
        objectMetadata->setTrackId(object.trackId);
        objectMetadata->setBoundingBox(object.boundingBox);

        for (const auto& item: object.attributes)
            objectMetadata->addAttribute(makePtr<Attribute>(item.first, item.second));

        objectMetadataPacket->addItem(objectMetadata.get());
    }

    for (const auto& entry: objectMetadataPacketByTimestamp)
        result.push_back(entry.second);

    return result;
}

void DeviceAgent::doSetNeededMetadataTypes(
    nx::sdk::Result<void>* /*outValue*/,
    const nx::sdk::analytics::IMetadataTypes* /*neededMetadataTypes*/)
{
}

Result<const ISettingsResponse*> DeviceAgent::settingsReceived()
{
    std::string manifestFilePath;
    std::string streamFilePath;

    if (m_isInitialSettings)
    {
        manifestFilePath = defaultManifestFilePath(m_pluginHomeDir);
        streamFilePath = defaultStreamFilePath(m_pluginHomeDir);
        m_isInitialSettings = false;
    }

    std::map<std::string, std::string> settings = currentSettings();

    const auto manifestSettingIt = settings.find(kManifestFileSetting);
    if (manifestSettingIt != settings.cend())
        manifestFilePath = manifestSettingIt->second;

    const auto streamSettingIt = settings.find(kStreamFileSetting);
    if (streamSettingIt != settings.cend())
        streamFilePath = streamSettingIt->second;

    pushManifest(readFileToString(manifestFilePath));

    Issues issues;
    m_streamInfo = parseObjectStreamFile(streamFilePath, &issues);
    if (!m_streamInfo.objectsByFrameNumber.empty())
        m_maxFrameNumber = m_streamInfo.objectsByFrameNumber.rbegin()->first;

    m_frameNumber = 0;

    reportIssues(issues);

    m_disabledObjectTypeIds.clear();
    for (const auto& entry: settings)
    {
        const std::string& settingName = entry.first;
        const std::string& settingValue = entry.second;

        if (startsWith(settingName, kObjectTypeFilterPrefix) && !toBool(settingValue))
            m_disabledObjectTypeIds.insert(settingName.substr(kObjectTypeFilterPrefix.length()));
    }

    return makeSettingsResponse(manifestFilePath, streamFilePath).releasePtr();
}

void DeviceAgent::reportIssues(const Issues& issues)
{
    if (!issues.errors.empty())
    {
        pushPluginDiagnosticEvent(
            IPluginDiagnosticEvent::Level::error,
            "Serious issues in the Object stream",
            makePluginDiagnosticEventDescription(issues.errors));
    }

    if (!issues.warnings.empty())
    {
        pushPluginDiagnosticEvent(
            IPluginDiagnosticEvent::Level::warning,
            "Issues in the Object stream",
            makePluginDiagnosticEventDescription(issues.warnings));
    }
}

/*static*/ DeviceAgent::StreamInfo DeviceAgent::parseObjectStreamFile(
    const std::string& filePath, Issues* outIssues)
{
    const std::string jsonString = readFileToString(filePath);

    std::string error;
    const Json json = Json::parse(jsonString, error);

    if (!error.empty() && json.is_null())
    {
        outIssues->errors.insert(Issue::objectStreamIsNotAValidJson);
        return {};
    }

    if (!json.is_array())
    {
        outIssues->errors.insert(Issue::objectStreamIsNotAJsonArray);
        return {};
    }

    StreamInfo result;
    for (const Json& objectDescription: json.array_items())
    {
        if (!objectDescription.is_object())
        {
            outIssues->errors.insert(Issue::objectItemIsNotAJsonObject);
            continue;
        }

        Object object;
        if (!parseCommonFields(objectDescription, &object, outIssues))
            continue;
        if (!parseBoundingBox(objectDescription, &object.boundingBox, outIssues))
            continue;
        if (!parseAttributes(objectDescription, &object.attributes, outIssues))
            continue;
        if (!parseTimestamp(objectDescription, &object.timestampUs, outIssues))
            continue;

        result.objectTypeIds.insert(object.typeId);
        result.objectsByFrameNumber[object.frameNumberToGenerateObject].push_back(
            std::move(object));
    }

    return result;
}

/*static*/ bool DeviceAgent::parseCommonFields(
    const nx::kit::Json& objectDescription,
    Object* outObject,
    Issues* outIssues)
{
    if (!objectDescription[kTypeIdField].is_string())
    {
        outIssues->errors.insert(Issue::typeIdIsNotAString);
        return false;
    }

    outObject->typeId = objectDescription[kTypeIdField].string_value();

    if (!objectDescription[kTrackIdField].is_string())
    {
        outIssues->errors.insert(Issue::trackIdIsNotAString);
        return false;
    }

    outObject->trackId = UuidHelper::fromStdString(objectDescription[kTrackIdField].string_value());
    if (outObject->trackId.isNull())
    {
        outIssues->errors.insert(Issue::trackIdIsNotAUuid);
        return false;
    }

    if (!objectDescription[kFrameNumberField].is_number())
    {
        outIssues->errors.insert(Issue::frameNumberIsNotANumber);
        return false;
    }

    outObject->frameNumberToGenerateObject = objectDescription[kFrameNumberField].int_value();

    return true;
}

/*static*/ bool DeviceAgent::parseBoundingBox(
    const Json& objectDescription,
    Rect* outBoundingBox,
    Issues* outIssues)
{
    if (!objectDescription[kBoundingBoxField].is_object())
    {
        outIssues->errors.insert(Issue::boundingBoxIsNotAJsonObject);
        return false;
    }

    const Json boundingBox = objectDescription[kBoundingBoxField];
    if (!boundingBox[kTopLeftXField].is_number())
    {
        outIssues->errors.insert(Issue::topLeftXIsNotANumber);
        return false;
    }

    if (!boundingBox[kTopLeftYfield].is_number())
    {
        outIssues->errors.insert(Issue::topLeftYIsNotANumber);
        return false;
    }

    if (!boundingBox[kWidthField].is_number())
    {
        outIssues->errors.insert(Issue::widthIsNotANumber);
        return false;
    }

    if (!boundingBox[kHeightField].is_number())
    {
        outIssues->errors.insert(Issue::heightIsNotANumber);
        return false;
    }

    outBoundingBox->x = boundingBox[kTopLeftXField].number_value();
    outBoundingBox->y = boundingBox[kTopLeftYfield].number_value();
    outBoundingBox->width = boundingBox[kWidthField].number_value();
    outBoundingBox->height = boundingBox[kHeightField].number_value();

    if (outBoundingBox->x < 0 || outBoundingBox->y < 0
        || outBoundingBox->x + outBoundingBox->width > 1
        || outBoundingBox->y + outBoundingBox->height > 1)
    {
        outIssues->warnings.insert(Issue::objectIsOutOfBounds);
    }

    return true;
}

/*static*/ bool DeviceAgent::parseAttributes(
    const Json& objectDescription,
    std::map<std::string, std::string>* outAttributes,
    Issues* outIssues)
{
    if (objectDescription[kAttributesField].is_object())
    {
        const Json::object attributes = objectDescription[kAttributesField].object_items();
        for (const auto& item: attributes)
        {
            if (!item.second.is_string())
            {
                outIssues->warnings.insert(Issue::attributeValueIsNotAString);
                continue;
            }

            (*outAttributes)[item.first] = item.second.string_value();
        }
    }
    else if (!objectDescription[kAttributesField].is_object())
    {
        outIssues->warnings.insert(Issue::attributesFieldIsNotAJsonObject);
    }

    return true;
}

/*static*/ bool DeviceAgent::parseTimestamp(
    const nx::kit::Json& objectDescription,
    int64_t* outTimestampUs,
    Issues* outIssues)
{
    if (objectDescription[kTimestampUsField].is_number())
        *outTimestampUs = objectDescription[kTimestampUsField].number_value();
    else if (!objectDescription[kTimestampUsField].is_null())
        outIssues->warnings.insert(Issue::timestampIsNotANumber);

    return true;
}

/*static*/ std::string DeviceAgent::issueToString(Issue issue)
{
    switch (issue)
    {
        case Issue::objectStreamIsNotAValidJson:
            return "Object stream file contains invalid JSON";
        case Issue::objectStreamIsNotAJsonArray:
            return "Object stream must be a valid JSON array";
        case Issue::objectItemIsNotAJsonObject:
            return "Some of items in the Object stream are not valid JSON objects";
        case Issue::trackIdIsNotAString:
            return "Track id of some items in the Object stream is not a string";
        case Issue::trackIdIsNotAUuid:
            return "Track id of some items in the Object stream is not a valid UUID";
        case Issue::typeIdIsNotAString:
            return "Type id of some items in the Object stream is not a string";
        case Issue::frameNumberIsNotANumber:
            return "Frame number of some items in the Object stream is not a number";
        case Issue::boundingBoxIsNotAJsonObject:
            return "Bounding box of some items in the Object stream is not a valid JSON object";
        case Issue::topLeftXIsNotANumber:
            return "Bounding box X coordinate of some items in the Object stream is not a number";
        case Issue::topLeftYIsNotANumber:
            return "Bounding box Y coordinate of some items in the Object stream is not a number";
        case Issue::widthIsNotANumber:
            return "Bounding box width of some items in the Object stream is not a number";
        case Issue::heightIsNotANumber:
            return "Bounding box height coordinate of some items in the Object stream is not a number";
        case Issue::objectIsOutOfBounds:
            return "Bounding box of some items in the Object stream is out of bounds";
        case Issue::attributesFieldIsNotAJsonObject:
            return "Attribute field of some items in the Object stream is not a valid JSON object";
        case Issue::attributeValueIsNotAString:
            return "Attribute values of some items in the Object stream is not a string";
        case Issue::timestampIsNotANumber:
            return "Timestamp of some items in the Object stream is not a number";
        default:
            NX_KIT_ASSERT(false, "Unexpected issue");
            return {};
    };
}

/*static*/ std::string DeviceAgent::makePluginDiagnosticEventDescription(
    const std::set<Issue>& issues)
{
    std::vector<std::string> issueStrings;
    for (const Issue issue: issues)
        issueStrings.push_back(issueToString(issue));

    return "The following issues have been found: " + join(issueStrings, ",\n");
}

Ptr<ISettingsResponse> DeviceAgent::makeSettingsResponse(
    const std::string& manifestFilePath,
    const std::string& streamFilePath) const
{
    Json::object manifestPathSetting;
    manifestPathSetting["type"] = "TextArea";
    manifestPathSetting["name"] = kManifestFileSetting;
    manifestPathSetting["caption"] = "Path to Device Agent Manifest file";
    manifestPathSetting["defaultValue"] = defaultManifestFilePath(m_pluginHomeDir);

    Json::object streamFilePathSetting;
    streamFilePathSetting["type"] = "TextArea";
    streamFilePathSetting["name"] = kStreamFileSetting;
    streamFilePathSetting["caption"] = "Path to Object stream file";
    streamFilePathSetting["defaultValue"] = defaultStreamFilePath(m_pluginHomeDir);

    Json::array pathSettingGroupItems = {manifestPathSetting, streamFilePathSetting};
    Json::object pathSettingGroup;
    pathSettingGroup["type"] = "GroupBox";
    pathSettingGroup["caption"] = "Paths";
    pathSettingGroup["items"] = std::move(pathSettingGroupItems);

    std::map<std::string, std::string> values;
    Json::array filterSettingGroupItems;
    for (const std::string& objectTypeId: m_streamInfo.objectTypeIds)
    {
        Json::object objectTypeFilter;
        objectTypeFilter["type"] = "CheckBox";
        objectTypeFilter["name"] = makeObjectTypeFilterSettingName(objectTypeId);
        objectTypeFilter["caption"] = objectTypeId;
        objectTypeFilter["defaultValue"] = true;

        filterSettingGroupItems.push_back(std::move(objectTypeFilter));

        values[makeObjectTypeFilterSettingName(objectTypeId)] =
            m_disabledObjectTypeIds.find(objectTypeId) == m_disabledObjectTypeIds.cend()
                ? "true"
                : "false";
    }

    Json::object filterSettingGroup;
    filterSettingGroup["type"] = "GroupBox";
    filterSettingGroup["caption"] = "Filters";
    filterSettingGroup["items"] = std::move(filterSettingGroupItems);

    Json::object settingsModel;
    settingsModel["type"] = "Settings";
    settingsModel["items"] = std::vector<Json>{pathSettingGroup, filterSettingGroup};

    values[kManifestFileSetting] = manifestFilePath;
    values[kStreamFileSetting] = streamFilePath;

    auto settingsResponse = makePtr<SettingsResponse>();
    settingsResponse->setModel(Json(settingsModel).dump());
    settingsResponse->setValues(makePtr<StringMap>(std::move(values)));

    return settingsResponse;
}

} // namespace object_streamer
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
