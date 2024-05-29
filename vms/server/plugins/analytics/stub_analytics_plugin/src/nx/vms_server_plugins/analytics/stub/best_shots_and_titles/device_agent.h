// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <nx/sdk/analytics/helpers/consuming_device_agent.h>
#include <nx/sdk/analytics/i_object_metadata_packet.h>
#include <nx/sdk/analytics/i_object_track_best_shot_packet.h>
#include <nx/sdk/analytics/i_object_track_title_packet.h>
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

    enum class TitleGenerationPolicy
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

    struct TitleGenerationConext
    {
        TitleGenerationPolicy policy;
        int frameNumberToGenerateTitle = 0;

        std::string url;

        std::string imageDataFormat;
        std::vector<char> imageData;

        std::string text;

        nx::sdk::analytics::Rect fixedTitleBoundingBox;
    };

private:
    using BestShotList = std::vector<nx::sdk::Ptr<nx::sdk::analytics::IObjectTrackBestShotPacket>>;
    BestShotList generateBestShots();

    using TitleList = std::vector<nx::sdk::Ptr<nx::sdk::analytics::IObjectTrackTitlePacket>>;
    TitleList generateTitles();

    nx::sdk::Ptr<nx::sdk::analytics::IObjectTrackBestShotPacket> generateFixedBestShot(
        nx::sdk::Uuid trackId);

    nx::sdk::Ptr<nx::sdk::analytics::IObjectTrackBestShotPacket> generateUrlBestShot(
        nx::sdk::Uuid trackId);

    nx::sdk::Ptr<nx::sdk::analytics::IObjectTrackBestShotPacket> generateImageBestShot(
        nx::sdk::Uuid trackId);

    nx::sdk::Ptr<nx::sdk::analytics::IObjectTrackTitlePacket> generateFixedTitle(
        nx::sdk::Uuid trackId);

    nx::sdk::Ptr<nx::sdk::analytics::IObjectTrackTitlePacket> generateUrlTitle(
        nx::sdk::Uuid trackId);

    nx::sdk::Ptr<nx::sdk::analytics::IObjectTrackTitlePacket> generateImageTitle(
        nx::sdk::Uuid trackId);

    void generateBestShotOrTitle();
    void generateBestShotObject();
    void generateTitleObject();

    void configureBestShots(std::map<std::string, std::string>& settings);
    void configureTitles(std::map<std::string, std::string>& settings);

    static BestShotGenerationPolicy bestShotGenerationPolicyFromString(const std::string& str);
    static TitleGenerationPolicy titleGenerationPolicyFromString(const std::string& str);

private:
    std::vector<TrackContext> m_bestShotTrackContexts;
    BestShotGenerationContext m_bestShotGenerationContext;
    std::map<nx::sdk::Uuid, int> m_bestShotGenerationCounterByTrackId;

    std::vector<TrackContext> m_titleTrackContexts;
    TitleGenerationConext m_titleGenerationContext;
    std::map<nx::sdk::Uuid, int> m_titleGenerationCounterByTrackId;

    int64_t m_lastFrameTimestampUs = 0;
    bool m_generateBestShotOrTitle = true;
};

} // namespace best_shots
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
