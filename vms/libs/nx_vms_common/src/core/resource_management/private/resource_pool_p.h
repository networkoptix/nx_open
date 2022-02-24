// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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
    std::atomic_bool hasIoModules{false};
    QSet<QnMediaServerResourcePtr> mediaServers;
    QMap<QString, QnNetworkResourcePtr> resourcesByPhysicalId;
};
