// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/vms/api/analytics/descriptors.h>
#include <nx/vms/api/analytics/engine_manifest.h>

#include <common/common_module_aware.h>
#include <core/resource/resource_fwd.h>

namespace nx::analytics {

class NX_VMS_COMMON_API EngineDescriptorManager:
    public QObject,
    public /*mixin*/ QnCommonModuleAware
{
    using base_type = QObject;
    Q_OBJECT

public:
    explicit EngineDescriptorManager(QObject* parent = nullptr);

    std::optional<nx::vms::api::analytics::EngineDescriptor> descriptor(
        const nx::vms::api::analytics::EngineId& id) const;

    nx::vms::api::analytics::EngineDescriptorMap descriptors(
        const std::set<nx::vms::api::analytics::EngineId>& engineIds = {}) const;
};

} // namespace nx::analytics
