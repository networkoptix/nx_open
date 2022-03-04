// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <nx/vms/api/analytics/descriptors.h>
#include <nx/vms/api/analytics/device_agent_manifest.h>
#include <nx/vms/api/analytics/engine_manifest.h>

namespace nx::analytics {

namespace taxonomy { class DescriptorContainer; }

class NX_VMS_COMMON_API GroupDescriptorManager:
    public QObject
{
    using base_type = QObject;
    Q_OBJECT

public:
    explicit GroupDescriptorManager(
        taxonomy::DescriptorContainer* taxonomyDescriptorContainer,
        QObject* parent = nullptr);

    std::optional<nx::vms::api::analytics::GroupDescriptor> descriptor(
        const nx::vms::api::analytics::GroupId& id) const;

    nx::vms::api::analytics::GroupDescriptorMap descriptors(
        const std::set<nx::vms::api::analytics::GroupId>& groupIds = {}) const;

private:
    taxonomy::DescriptorContainer* const m_taxonomyDescriptorContainer;
};

} // namespace nx::analytics
