#pragma once

#include <QtCore/QObject>

#include <nx/analytics/types.h>

#include <nx/vms/api/analytics/descriptors.h>
#include <nx/vms/api/analytics/engine_manifest.h>

#include <common/common_module_aware.h>
#include <core/resource/resource_fwd.h>

namespace nx::analytics {

class EngineDescriptorManager: public QObject, public /*mixin*/ QnCommonModuleAware
{
    using base_type = QObject;
    Q_OBJECT

public:
    explicit EngineDescriptorManager(QObject* parent = nullptr);

    std::optional<nx::vms::api::analytics::EngineDescriptor> descriptor(
        const EngineId& id) const;

    EngineDescriptorMap descriptors(const std::set<EngineId>& engineIds = {}) const;

    void updateFromEngineManifest(
        const PluginId& pluginId,
        const EngineId& engineId,
        const QString& engineName,
        const nx::vms::api::analytics::EngineManifest& manifest);

private:
    EngineDescriptorContainer m_engineDescriptorContainer;
};

} // namespace nx::analytics
