// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <QtCore/QObject>

#include <nx/core/resource/property_watcher.h>
#include <nx/utils/thread/mutex.h>
#include <nx/vms/api/analytics/descriptors.h>
#include <nx/vms/common/system_context_aware.h>

namespace nx::analytics::taxonomy {

class NX_VMS_COMMON_API DescriptorContainer:
    public QObject,
    public nx::vms::common::SystemContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    DescriptorContainer(nx::vms::common::SystemContext* context, QObject* parent = nullptr);

    nx::vms::api::analytics::Descriptors descriptors(const nx::Uuid& serverId = nx::Uuid());

    // Server-only method.
    void updateDescriptors(nx::vms::api::analytics::Descriptors descriptors);

signals:
    void descriptorsUpdated();

private:
    void at_descriptorsUpdated();

private:
    mutable nx::Mutex m_mutex;

    nx::core::resource::PropertyWatcher m_propertyWatcher;
    std::optional<nx::vms::api::analytics::Descriptors> m_cachedDescriptors;
};

} // namespace nx::analytics::taxonomy
