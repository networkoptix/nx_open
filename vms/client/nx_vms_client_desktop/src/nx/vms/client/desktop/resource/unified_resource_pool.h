// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>

namespace nx::vms::client::desktop {

/**
 * Unified interface to access all available Resource Pools. Listens for System Contexts creation
 * and destruction. Emits resourcesAdded and resourcesRemoved when a Context is added or removed
 * correspondingly.
 */
class UnifiedResourcePool: public QObject
{
    Q_OBJECT

public:
    UnifiedResourcePool(QObject* parent = nullptr);

    using ResourceFilter = std::function<bool (const QnResourcePtr& resource)>;
    QnResourceList resources(ResourceFilter filter = {}) const;
    QnResourcePtr resource(const QnUuid& resourceId, const QnUuid& localSystemId) const;

signals:
    /**
     * Emitted whenever any new Resource is added to any of the Resource Pools.
     */
    void resourcesAdded(const QnResourceList& resources);

    /**
     * Emitted whenever any resource is removed from the pool.
     */
    void resourcesRemoved(const QnResourceList& resources);
};

} // namespace nx::vms::client::desktop
