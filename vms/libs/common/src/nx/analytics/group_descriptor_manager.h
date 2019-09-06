#pragma once

#include <QtCore/QObject>

#include <nx/analytics/types.h>

#include <nx/vms/api/analytics/descriptors.h>
#include <nx/vms/api/analytics/engine_manifest.h>
#include <nx/vms/api/analytics/device_agent_manifest.h>

#include <common/common_module_aware.h>
#include <core/resource/resource_fwd.h>

namespace nx::analytics {

class GroupDescriptorManager: public QObject, public /*mixin*/ QnCommonModuleAware
{
    using base_type = QObject;
    Q_OBJECT

public:
    explicit GroupDescriptorManager(QObject* parent = nullptr);

    std::optional<nx::vms::api::analytics::GroupDescriptor> descriptor(
        const GroupId& id) const;

    GroupDescriptorMap descriptors(const std::set<GroupId>& groupIds = {}) const;

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
    std::unique_ptr<GroupDescriptorContainer> m_groupDescriptorContainer;
};

} // namespace nx::analytics
