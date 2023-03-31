// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <unordered_map>

#include <QtCore/QMap>
#include <QtCore/QSet>
#include <QtCore/QString>

#include "../resource_pool.h"

struct QnResourcePool::Private
{
    explicit Private(QnResourcePool* owner, nx::vms::common::SystemContext* systemContext);

    void handleResourceAdded(const QnResourcePtr& resource);
    void handleResourceRemoved(const QnResourcePtr& resource);

    void updateIsIOModule(const QnVirtualCameraResourcePtr& camera);
    void removeUser(const QString& name, const QnUserResourcePtr& user);

    struct SameNameUsers
    {
        QnUserResourcePtr main() const { return m_main; }
        bool empty() const { return m_byPriority.empty(); }

        void add(QnUserResourcePtr user);
        void remove(const QnUserResourcePtr& user);
        void selectMainUser();

    private:
        QnUserResourcePtr m_main;
        std::map<size_t, std::list<QnUserResourcePtr>> m_byPriority;
    };

public:
    const QnResourcePool* const q;
    nx::vms::common::SystemContext* const systemContext;
    QSet<QnVirtualCameraResourcePtr> ioModules;
    std::atomic_bool hasIoModules{false};
    QSet<QnMediaServerResourcePtr> mediaServers;
    QSet<QnStorageResourcePtr> storages;
    QMap<QString, QnNetworkResourcePtr> resourcesByPhysicalId;
    std::unordered_map<QString, SameNameUsers> usersByName;
};
