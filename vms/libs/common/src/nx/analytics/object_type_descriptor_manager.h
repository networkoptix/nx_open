#pragma once

#include <QtCore/QObject>

#include <nx/analytics/types.h>

#include <nx/vms/api/analytics/descriptors.h>
#include <nx/vms/api/analytics/engine_manifest.h>
#include <nx/vms/api/analytics/device_agent_manifest.h>

#include <common/common_module_aware.h>
#include <core/resource/resource_fwd.h>

namespace nx::analytics {

class ObjectTypeDescriptorManager : public QObject, public /*mixin*/ QnCommonModuleAware
{
    using base_type = QObject;
    Q_OBJECT

public:
    explicit ObjectTypeDescriptorManager(QObject* parent = nullptr);

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
    std::unique_ptr<ObjectTypeDescriptorContainer> m_objectTypeDescriptorContainer;
    std::unique_ptr<EngineDescriptorContainer> m_engineDescriptorContainer;
    std::unique_ptr<GroupDescriptorContainer> m_groupDescriptorContainer;
};

} // namespace nx::analytics
