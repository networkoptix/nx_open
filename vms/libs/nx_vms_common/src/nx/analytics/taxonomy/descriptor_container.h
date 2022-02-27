// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <QtCore/QObject>

#include <nx/vms/api/analytics/descriptors.h>
#include <nx/analytics/taxonomy/property_watcher.h>

#include <nx/utils/thread/mutex.h>
#include <common/common_module_aware.h>
#include <utils/common/connective.h>

namespace nx::analytics::taxonomy {

class NX_VMS_COMMON_API DescriptorContainer: public Connective<QObject>, public QnCommonModuleAware
{
    Q_OBJECT
public:
    DescriptorContainer(QnCommonModule* commonModule);

    nx::vms::api::analytics::Descriptors descriptors(const QnUuid& serverId = QnUuid());

    void updateDescriptors(nx::vms::api::analytics::Descriptors descriptors);

signals:
    void descriptorsUpdated();

private:
    void at_descriptorsUpdated();

private:
    mutable nx::Mutex m_mutex;

    PropertyWatcher m_propertyWatcher;
    std::optional<nx::vms::api::analytics::Descriptors> m_cachedDescriptors;
};

} // namespace nx::analytics::taxonomy
