#pragma once

#include <nx/analytics/types.h>

#include <nx/vms/api/analytics/descriptors.h>
#include <nx/vms/api/analytics/engine_manifest.h>
#include <nx/vms/api/analytics/device_agent_manifest.h>

#include <common/common_module_aware.h>
#include <core/resource/resource_fwd.h>

namespace nx::analytics {

class ObjectTypeDescriptorManager : public QnCommonModuleAware
{
    using base_type = QnCommonModuleAware;

public:
    ObjectTypeDescriptorManager(QnCommonModule* commonModule);

    std::optional<nx::vms::api::analytics::ObjectTypeDescriptor> descriptor(
        const ObjectTypeId& id) const;

    ObjectTypeDescriptorMap descriptors(
        const std::set<ObjectTypeId>& eventTypeIds = {}) const;

    ScopedObjectTypeIds supportedObjectTypeIds(const QnVirtualCameraResourcePtr& device) const;

    ScopedObjectTypeIds supportedObjectTypeIdsUnion(
        const QnVirtualCameraResourceList& devices) const;

    ScopedObjectTypeIds supportedObjectTypeIdsIntersection(
        const QnVirtualCameraResourceList& devices) const;

    ScopedObjectTypeIds compatibleObjectTypeIds(const QnVirtualCameraResourcePtr& device) const;

    ScopedObjectTypeIds compatibleObjectTypeIdsUnion(
        const QnVirtualCameraResourceList& devices) const;

    ScopedObjectTypeIds compatibleObjectTypeIdsIntersection(
        const QnVirtualCameraResourceList& devices) const;

    void updateFromEngineManifest(
        const PluginId& pluginId,
        const EngineId& engineId,
        const QString& engineName,
        const nx::vms::api::analytics::EngineManifest& manifest);

    void updateFromDeviceAgentManifest(
        const DeviceId& deviceId,
        const EngineId& engineId,
        const nx::vms::api::analytics::DeviceAgentManifest& manifest);

private:
    GroupId objectTypeGroupForEngine(
        const EngineId& engineId, const ObjectTypeId& eventTypeId) const;

private:
    ObjectTypeDescriptorContainer m_objectTypeDescriptorContainer;
    EngineDescriptorContainer m_engineDescriptorContainer;
    GroupDescriptorContainer m_groupDescriptorContainer;
    DeviceDescriptorContainer m_deviceDescriptorContainer;
};

} // namespace nx::analytics
