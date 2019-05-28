#pragma once

#include <QtCore/QMap>
#include <QtCore/QSet>
#include <QtCore/QString>

#include "../resource_pool.h"

struct QnResourcePool::Private
{
    explicit Private(QnResourcePool* owner);

    void handleResourceAdded(const QnResourcePtr& resource);
    void handleResourceRemoved(const QnResourcePtr& resource);

    void updateIsIOModule(const QnVirtualCameraResourcePtr& camera);

    const QnResourcePool* q;
    QSet<QnVirtualCameraResourcePtr> ioModules;
    QSet<QnMediaServerResourcePtr> mediaServers;
    QMap<QString, QnResourcePtr> resourcesByUniqueId;
};
