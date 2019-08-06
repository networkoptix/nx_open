#pragma once

#include <memory>

#include <QtCore/QObject>

#include <nx/analytics/types.h>

#include <nx/vms/api/analytics/descriptors.h>
#include <nx/vms/api/analytics/plugin_manifest.h>

#include <common/common_module_aware.h>
#include <core/resource/resource_fwd.h>

namespace nx::analytics {

class PluginDescriptorManager: public QObject, public /*mixin*/ QnCommonModuleAware
{
    using base_type = QObject;
    Q_OBJECT

public:
    explicit PluginDescriptorManager(QObject* parent = nullptr);

    std::optional<nx::vms::api::analytics::PluginDescriptor> descriptor(
        const PluginId& pluginId) const;

    PluginDescriptorMap descriptors(const std::set<PluginId>& pluginIds = {}) const;

    void updateFromPluginManifest(const nx::vms::api::analytics::PluginManifest& manifest);

private:
    std::unique_ptr<PluginDescriptorContainer> m_pluginDescriptorContainer;
};

} // namespace nx::analytics
