// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <set>
#include <functional>

#include <QtCore/QObject>
#include <QtCore/QString>

#include <core/resource/resource_fwd.h>

#include <nx/utils/thread/mutex.h>

class QnResourcePool;

namespace nx::analytics::taxonomy {

class PropertyWatcher: public QObject
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
    void at_resourceAdded(const QnResourcePtr& resource);
    void at_resourceRemoved(const QnResourcePtr& resource);
    void at_propertyChanged(const QnResourcePtr& resource, const QString& propertyName);

private:
    mutable nx::Mutex m_mutex;

    QnResourcePool* m_resourcePool = nullptr;

    std::set<QString> m_propertiesToWatch;
    ResourceFilter m_resourceFilter;
};

} // namespace nx::analytics::taxonomy
