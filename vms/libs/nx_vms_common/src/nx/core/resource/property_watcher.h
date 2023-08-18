// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>
#include <set>

#include <QtCore/QObject>
#include <QtCore/QString>

#include <core/resource/resource_fwd.h>
#include <nx/utils/thread/mutex.h>

class QnResourcePool;

namespace nx::core::resource {

class NX_VMS_COMMON_API PropertyWatcher: public QObject
{
    Q_OBJECT

public:
    using ResourceFilter = std::function<bool(const QnResourcePtr&)>;

public:
    PropertyWatcher(QnResourcePool* resourcePool);

    ~PropertyWatcher();

    void watch(std::set<QString> properties, ResourceFilter resourceFilter = nullptr);

signals:
    void propertyChanged();

private:
    void at_resourcesAdded(const QnResourceList& resource);
    void at_resourcesRemoved(const QnResourceList& resource);
    void at_propertyChanged(const QnResourcePtr& resource, const QString& propertyName);

private:
    mutable nx::Mutex m_mutex;

    QnResourcePool* m_resourcePool = nullptr;

    std::set<QString> m_propertiesToWatch;
    ResourceFilter m_resourceFilter;
};

} // namespace nx::core::resource
