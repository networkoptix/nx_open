// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/analytics/helpers/consuming_device_agent.h>
#include <nx/sdk/helpers/uuid_helper.h>

#include "engine.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace taxonomy_features {

class DeviceAgent: public nx::sdk::analytics::ConsumingDeviceAgent
{
public:
    DeviceAgent(Engine* engine, const nx::sdk::IDeviceInfo* deviceInfo);
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
    nx::sdk::Uuid trackIdByTrackIndex(int trackIndex);

    nx::sdk::Ptr<nx::sdk::analytics::IMetadataPacket> generateObjectMetadataPacket(
        int64_t frameTimestampUs);

private:
    struct Settings
    {
        std::atomic<bool> generateInstanceOfBaseObjectType{true};
        std::atomic<bool> generateInstanceOfDerivedObjectType{true};
        std::atomic<bool> generateInstanceOfDerivedObjectTypeWithOmittedAttributes{true};
        std::atomic<bool> generateInstanceOfHiddenDerivedObjectType{true};
        std::atomic<bool> generateInstanceOfHiddenDerivedObjectTypeWithOwnAttributes{true};
        std::atomic<bool> generateInstanceOfDerivedObjectTypeWithUnsupportedBase{true};
        std::atomic<bool> generateInstanceOfObjectTypeWithNumericAttibutes{true};
        std::atomic<bool> generateInstanceOfObjectTypeWithBooleanAttibutes{true};
        std::atomic<bool> generateInstanceOfObjectTypeWithIcon{true};
        std::atomic<bool> generateInstanceOfObjectTypeInheritedFromBaseTypeLibraryType{true};
        std::atomic<bool> generateInstanceOfObjectTypeUsingBaseTypeLibraryEnumType{true};
        std::atomic<bool> generateInstanceOfObjectTypeUsingBaseTypeLibraryColorType{true};
        std::atomic<bool> generateInstanceOfObjectTypeUsingBaseTypeLibraryObjectType{true};
        std::atomic<bool> generateInstanceOfOfBaseTypeLibraryObjectType{true};
        std::atomic<bool> generateInstanceOfObjectTypeDeclaredInEngineManifest{true};
        std::atomic<bool> generateInstanceOfLiveOnlyObjectType{true};
        std::atomic<bool> generateInstanceOfNonIndexableObjectType{true};
        std::atomic<bool> generateInstanceOfExtendedObjectType{true};
        std::atomic<bool> generateInstanceOfObjectTypeWithAttributeList{true};
    };

    Engine* const m_engine;

    int m_frameIndex = 0;
    std::vector<nx::sdk::Uuid> m_trackIds;
    Settings m_settings;
};

} // namespace taxonomy_features
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
