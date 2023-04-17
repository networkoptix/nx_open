// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <nx/sdk/analytics/helpers/consuming_device_agent.h>
#include <nx/sdk/analytics/i_object_metadata_packet.h>
#include <nx/sdk/analytics/i_object_track_best_shot_packet.h>
#include <nx/sdk/analytics/rect.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace best_shots {

class DeviceAgent: public nx::sdk::analytics::ConsumingDeviceAgent
{
public:
    DeviceAgent(const nx::sdk::IDeviceInfo* deviceInfo);
    virtual ~DeviceAgent() override;

protected:
    virtual std::string manifestString() const override;

    virtual bool pushCompressedVideoFrame(
        const nx::sdk::analytics::ICompressedVideoPacket* videoFrame) override;

    virtual void doSetNeededMetadataTypes(
        nx::sdk::Result<void>* outValue,
        const nx::sdk::analytics::IMetadataTypes* neededMetadataTypes) override;

    virtual nx::sdk::Result<const nx::sdk::ISettingsResponse*> settingsReceived() override;

private:
    struct TrackContext
    {
        nx::sdk::Uuid trackId;
        nx::sdk::analytics::Rect boundingBox;
    };

    enum class BestShotGenerationPolicy
    {
        fixed,
        url,
        image,
    };

    struct BestShotGenerationContext
    {
        BestShotGenerationPolicy policy;
        int frameNumberToGenerateBestShot = 0;

        std::string url;

        std::string imageDataFormat;
        std::vector<char> imageData;

        nx::sdk::analytics::Rect fixedBestShotBoundingBox;
    };

private:
    using BestShotList = std::vector<nx::sdk::Ptr<nx::sdk::analytics::IObjectTrackBestShotPacket>>;
    BestShotList generateBestShots();

    nx::sdk::Ptr<nx::sdk::analytics::IObjectTrackBestShotPacket> generateFixedBestShot(
        nx::sdk::Uuid trackId);

    nx::sdk::Ptr<nx::sdk::analytics::IObjectTrackBestShotPacket> generateUrlBestShot(
        nx::sdk::Uuid trackId);

    nx::sdk::Ptr<nx::sdk::analytics::IObjectTrackBestShotPacket> generateImageBestShot(
        nx::sdk::Uuid trackId);

    nx::sdk::Ptr<nx::sdk::analytics::IObjectMetadataPacket> generateObjects();

    static BestShotGenerationPolicy bestShotGenerationPolicyFromString(const std::string& str);

private:
    std::vector<TrackContext> m_trackContexts;
    BestShotGenerationContext m_bestShotGenerationContext;
    int64_t m_lastFrameTimestampUs = 0;
    std::map<nx::sdk::Uuid, int> m_bestShotGenerationCounterByTrackId;
};

} // namespace best_shots
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
