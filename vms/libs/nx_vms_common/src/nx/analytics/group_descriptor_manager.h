// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/vms/api/analytics/descriptors.h>
#include <nx/vms/api/analytics/engine_manifest.h>
#include <nx/vms/api/analytics/device_agent_manifest.h>

#include <common/common_module_aware.h>
#include <core/resource/resource_fwd.h>

namespace nx::analytics {

class NX_VMS_COMMON_API GroupDescriptorManager:
    public QObject,
    public /*mixin*/ QnCommonModuleAware
{
    using base_type = QObject;
    Q_OBJECT

public:
    explicit GroupDescriptorManager(QObject* parent = nullptr);

    std::optional<nx::vms::api::analytics::GroupDescriptor> descriptor(
        const nx::vms::api::analytics::GroupId& id) const;

    nx::vms::api::analytics::GroupDescriptorMap descriptors(
        const std::set<nx::vms::api::analytics::GroupId>& groupIds = {}) const;
};

} // namespace nx::analytics
