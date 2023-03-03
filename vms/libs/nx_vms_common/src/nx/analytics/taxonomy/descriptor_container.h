// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <QtCore/QObject>

#include <nx/utils/thread/mutex.h>
#include <nx/vms/api/analytics/descriptors.h>
#include <nx/vms/common/system_context_aware.h>

namespace nx::vms::common { class PropertyWatcher; }

namespace nx::analytics::taxonomy {

class NX_VMS_COMMON_API DescriptorContainer:
    public QObject,
    public nx::vms::common::SystemContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    DescriptorContainer(nx::vms::common::SystemContext* context, QObject* parent = nullptr);
    virtual ~DescriptorContainer() override;

    nx::vms::api::analytics::Descriptors descriptors(const QnUuid& serverId = QnUuid());

    // Server-only method.
    void updateDescriptors(nx::vms::api::analytics::Descriptors descriptors);

signals:
    void descriptorsUpdated();

private:
    void at_descriptorsUpdated();

private:
    mutable nx::Mutex m_mutex;

    std::unique_ptr<nx::vms::common::PropertyWatcher> m_propertyWatcher;
    std::optional<nx::vms::api::analytics::Descriptors> m_cachedDescriptors;
};

} // namespace nx::analytics::taxonomy
