// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <QtCore/QObject>

#include <nx/vms/api/analytics/descriptors.h>
#include <nx/vms/api/analytics/plugin_manifest.h>

#include <common/common_module_aware.h>
#include <core/resource/resource_fwd.h>

namespace nx::analytics {

class NX_VMS_COMMON_API PluginDescriptorManager:
    public QObject,
    public /*mixin*/ QnCommonModuleAware
{
    using base_type = QObject;
    Q_OBJECT

public:
    explicit PluginDescriptorManager(QObject* parent = nullptr);

    std::optional<nx::vms::api::analytics::PluginDescriptor> descriptor(
        const nx::vms::api::analytics::PluginId& pluginId) const;

    nx::vms::api::analytics::PluginDescriptorMap descriptors(
        const std::set<nx::vms::api::analytics::PluginId>& pluginIds = {}) const;
};

} // namespace nx::analytics
