// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <set>
#include <functional>

#include <QtCore/QString>

#include <core/resource/resource_fwd.h>
#include <nx/vms/common/system_context_aware.h>
#include <nx/utils/thread/mutex.h>

namespace nx::vms::common {

/**
 * Notifies when one of the watched Resource properties is changed or removed.
 */
class PropertyWatcher: public QObject, public SystemContextAware
{
    Q_OBJECT

public:
    using ResourceFilter = std::function<bool(const QnResourcePtr&)>;

public:
    PropertyWatcher(
        SystemContext* systemContext,
        std::set<QString> properties,
        ResourceFilter resourceFilter = {},
        QObject* parent = nullptr);

signals:
    void propertyChanged();

private:
    void handlePropertyChanged(const QnUuid& resourceId, const QString& key);

private:
    std::set<QString> const m_propertiesToWatch;
    ResourceFilter const m_resourceFilter;
};

} // namespace nx::vms::common
