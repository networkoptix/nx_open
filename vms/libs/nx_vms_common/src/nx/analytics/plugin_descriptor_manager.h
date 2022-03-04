// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <nx/vms/api/analytics/descriptors.h>
#include <nx/vms/api/analytics/plugin_manifest.h>

namespace nx::analytics {

namespace taxonomy { class DescriptorContainer; }

class NX_VMS_COMMON_API PluginDescriptorManager: public QObject
{
    using base_type = QObject;
    Q_OBJECT

public:
    explicit PluginDescriptorManager(
        taxonomy::DescriptorContainer* taxonomyDescriptorContainer,
        QObject* parent = nullptr);

    std::optional<nx::vms::api::analytics::PluginDescriptor> descriptor(
        const nx::vms::api::analytics::PluginId& pluginId) const;

    nx::vms::api::analytics::PluginDescriptorMap descriptors(
        const std::set<nx::vms::api::analytics::PluginId>& pluginIds = {}) const;

private:
    taxonomy::DescriptorContainer* const m_taxonomyDescriptorContainer;
};

} // namespace nx::analytics
