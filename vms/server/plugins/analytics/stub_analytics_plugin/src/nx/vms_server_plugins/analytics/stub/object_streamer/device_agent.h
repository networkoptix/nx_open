// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <set>

#include <nx/sdk/analytics/helpers/consuming_device_agent.h>
#include <nx/sdk/analytics/i_object_metadata_packet.h>

#include <nx/kit/json.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace object_streamer {

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
    struct Object
    {
        std::string typeId;
        nx::sdk::Uuid trackId;
        nx::sdk::analytics::Rect boundingBox;
        std::map<std::string, std::string> attributes;
        int frameNumberToGenerateObject = 0;
        int64_t timestampUs = -1;
    };

    enum class Issue
    {
        objectStreamIsNotAValidJson,
        objectStreamIsNotAJsonArray,
        objectItemIsNotAJsonObject,
        trackIdIsNotAString,
        trackIdIsNotAUuid,
        typeIdIsNotAString,
        frameNumberIsNotANumber,
        boundingBoxIsNotAJsonObject,
        topLeftXIsNotANumber,
        topLeftYIsNotANumber,
        widthIsNotANumber,
        heightIsNotANumber,
        objectIsOutOfBounds,
        attributesFieldIsNotAJsonObject,
        attributeValueIsNotAString,
        timestampIsNotANumber,
    };

    struct Issues
    {
        std::set<Issue> errors;
        std::set<Issue> warnings;
    };

private:
    static std::map<int, std::vector<Object>> parseObjectStreamFile(
        const std::string& filePath,
        Issues* outIssues);

    static bool parseCommonFields(
        const nx::kit::Json& objectDescription,
        Object* outObject,
        Issues* outIssues);

    static bool parseBoundingBox(
        const nx::kit::Json& objectDescription,
        nx::sdk::analytics::Rect* outBoundingBox,
        Issues* outIssues);

    static bool parseAttributes(
        const nx::kit::Json& objectDescription,
        std::map<std::string, std::string>* outAttributes,
        Issues* outIssues);

    static bool parseTimestamp(
        const nx::kit::Json& objectDescription,
        int64_t* outTimestamp,
        Issues* outIssues);

    static std::string issueToString(Issue issue);

    static std::string makePluginDiagnosticEventDescription(const std::set<Issue>& issues);

    std::vector<nx::sdk::Ptr<nx::sdk::analytics::IObjectMetadataPacket>> generateObjects(
        int frameNumber,
        int64_t frameTimestampUs,
        int64_t durationUs);

    void reportIssues(const Issues& issues);

private:
    std::map<int, std::vector<Object>> m_objectsByFrameNumber;
    int m_frameNumber = 0;
    int m_maxFrameNumber = 0;
    int64_t m_lastFrameTimestampUs = -1;
};

} // namespace object_streamer
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
