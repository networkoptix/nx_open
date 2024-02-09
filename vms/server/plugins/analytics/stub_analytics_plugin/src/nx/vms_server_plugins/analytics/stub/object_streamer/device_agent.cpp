// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_agent.h"

#include <fstream>
#include <streambuf>

#include <nx/sdk/analytics/helpers/object_metadata.h>
#include <nx/sdk/analytics/helpers/object_metadata_packet.h>
#include <nx/sdk/analytics/helpers/object_track_best_shot_packet.h>
#include <nx/sdk/helpers/uuid_helper.h>
#include <nx/sdk/helpers/settings_response.h>

#include <nx/kit/json.h>
#include <nx/kit/debug.h>

#include "../utils.h"
#include "utils.h"
#include "constants.h"
#include "stub_analytics_plugin_object_streamer_ini.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace object_streamer {

using namespace nx::sdk;
using namespace nx::sdk::analytics;
using Uuid = nx::sdk::Uuid;

using nx::kit::Json;

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
    // If we received at least one frame previously.
    if (m_lastFrameTimestampUs >= 0)
    {
        // Generate metadata for the previous frame.
        const int previousFrameNumber = (m_frameNumber == 0) ? m_maxFrameNumber : (m_frameNumber - 1);
        const int64_t previousFrameTimestampUs = m_lastFrameTimestampUs;
        const int64_t previousFrameDurationUs = std::max(videoPacket->timestampUs() - previousFrameTimestampUs, (int64_t) 0);

        for (auto& metadataPacket: generateMetadata(previousFrameNumber, previousFrameTimestampUs, previousFrameDurationUs))
        {
            pushMetadataPacket(metadataPacket.releasePtr());
        }

        // On wraparound, remove per-stream-cycle track ids.
        if (m_frameNumber == 0)
        {
            for (auto it = m_trackIdByRef.begin(); it != m_trackIdByRef.end();)
            {
                if (startsWith(it->first, kAutoTrackIdPerStreamCyclePrefix))
                    it = m_trackIdByRef.erase(it);
                else
                    ++it;
            }
        }
    }

    ++m_frameNumber;

    if (m_frameNumber > m_maxFrameNumber)
        m_frameNumber = 0;

    m_lastFrameTimestampUs = videoPacket->timestampUs();

    return true;
}

std::vector<Ptr<IMetadataPacket>> DeviceAgent::generateMetadata(
    int frameNumber, int64_t frameTimestampUs, int64_t durationUs)
{
    std::vector<Ptr<IMetadataPacket>> result;

    if (m_streamInfo.objectsByFrameNumber.find(frameNumber) == m_streamInfo.objectsByFrameNumber.cend())
        return result;

    std::map<int64_t, Ptr<ObjectMetadataPacket>> objectMetadataPacketByTimestamp;
    std::vector<Ptr<ObjectTrackBestShotPacket>> objectTrackBestShotPackets;
    for (Object& object: m_streamInfo.objectsByFrameNumber[frameNumber])
    {
        if (m_disabledObjectTypeIds.find(object.typeId) != m_disabledObjectTypeIds.cend())
            continue;

        const int64_t timestampUs = object.timestampUs >= 0
            ? object.timestampUs
            : frameTimestampUs;

        if (!object.trackIdRef.empty())
            object.trackId = obtainObjectTrackIdFromRef(object.trackIdRef);

        if (object.entryType == Object::EntryType::bestShot)
        {
            auto bestShotPacket = makePtr<ObjectTrackBestShotPacket>();
            bestShotPacket->setTimestampUs(timestampUs);
            bestShotPacket->setTrackId(object.trackId);
            bestShotPacket->setBoundingBox(object.boundingBox);
            for (const auto& item: object.attributes)
                bestShotPacket->addAttribute(makePtr<Attribute>(item.first, item.second));

            if (isHttpOrHttpsUrl(object.imageSource))
            {
                bestShotPacket->setImageUrl(object.imageSource);
            }
            else if (!object.imageSource.empty())
            {
                std::string imageFormat = imageFormatFromPath(object.imageSource);
                if (!imageFormat.empty())
                {
                    bestShotPacket->setImageDataFormat(std::move(imageFormat));
                    bestShotPacket->setImageData(loadFile(object.imageSource));
                }
            }

            objectTrackBestShotPackets.push_back(std::move(bestShotPacket));
        }
        else
        {
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
    }

    for (const auto& entry: objectMetadataPacketByTimestamp)
        result.push_back(entry.second);

    for (const auto& bestShotPacket: objectTrackBestShotPackets)
        result.push_back(bestShotPacket);

    return result;
}

Uuid DeviceAgent::obtainObjectTrackIdFromRef(const std::string& objectTrackIdRef)
{
    if (const auto it = m_trackIdByRef.find(objectTrackIdRef); it != m_trackIdByRef.cend())
        return it->second;

    const auto emplacementResult = m_trackIdByRef.emplace(
        objectTrackIdRef, UuidHelper::randomUuid());

    return emplacementResult.first->second;
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

void DeviceAgent::reportIssues(const Issues& issues) const
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

Ptr<ISettingsResponse> DeviceAgent::makeSettingsResponse(
    const std::string& manifestFilePath,
    const std::string& streamFilePath) const
{
    Issues issues;
    const StreamInfo streamInfo = parseObjectStreamFile(
        defaultStreamFilePath(m_pluginHomeDir),
        &issues);

    reportIssues(issues);

    const std::string settingsModel = makeSettingsModel(
        defaultManifestFilePath(m_pluginHomeDir),
        defaultStreamFilePath(m_pluginHomeDir),
        m_pluginHomeDir,
        streamInfo.objectTypeIds);

    std::map<std::string, std::string> values;
    for (const std::string& objectTypeId: m_streamInfo.objectTypeIds)
    {
        values[makeObjectTypeFilterSettingName(objectTypeId)] =
            m_disabledObjectTypeIds.find(objectTypeId) == m_disabledObjectTypeIds.cend()
                ? "true"
                : "false";
    }

    values[kManifestFileSetting] = manifestFilePath;
    values[kStreamFileSetting] = streamFilePath;

    auto settingsResponse = makePtr<SettingsResponse>();
    settingsResponse->setModel(settingsModel);
    settingsResponse->setValues(makePtr<StringMap>(std::move(values)));

    return settingsResponse;
}

} // namespace object_streamer
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
