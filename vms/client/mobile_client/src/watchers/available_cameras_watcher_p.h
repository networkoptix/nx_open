// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <core/resource_access/helpers/layout_item_aggregator.h>

#include "available_cameras_watcher.h"

namespace detail {

class Watcher: public QObject, public nx::vms::common::SystemContextAware
{
    Q_OBJECT

    using base_type = QObject;

public:
    Watcher(const QnUserResourcePtr& user, nx::vms::common::SystemContext* systemContext);

    QnUserResourcePtr user() const;

    virtual QHash<nx::Uuid, QnVirtualCameraResourcePtr> cameras() const = 0;

signals:
    void cameraAdded(const QnResourcePtr& resource);
    void cameraRemoved(const QnResourcePtr& resource);

private:
    QnUserResourcePtr m_user;
};

class LayoutBasedWatcher: public Watcher
{
public:
    LayoutBasedWatcher(
        const QnUserResourcePtr& user,
        nx::vms::common::SystemContext* systemContext);

    virtual QHash<nx::Uuid, QnVirtualCameraResourcePtr> cameras() const override;

private:
    void at_resourceAdded(const QnResourcePtr& resource);
    void at_resourceRemoved(const QnResourcePtr& resource);
    void at_layoutItemAdded(const nx::Uuid& id);
    void at_layoutItemRemoved(const nx::Uuid& id);

private:
    QnLayoutItemAggregator* m_itemAggregator;
    QHash<nx::Uuid, QnVirtualCameraResourcePtr> m_cameras;
};

class PermissionsBasedWatcher: public Watcher
{
    Q_OBJECT

public:
    PermissionsBasedWatcher(
        const QnUserResourcePtr& user,
        nx::vms::common::SystemContext* systemContext);

    virtual QHash<nx::Uuid, QnVirtualCameraResourcePtr> cameras() const override;

private:
    void addCamera(const QnResourcePtr& resource);
    void removeCamera(const QnResourcePtr& resource);

private:
    QHash<nx::Uuid, QnVirtualCameraResourcePtr> m_cameras;
};

} // namespace detail
