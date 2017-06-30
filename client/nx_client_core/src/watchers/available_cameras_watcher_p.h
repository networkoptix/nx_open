#include "available_cameras_watcher.h"

#include <core/resource_access/helpers/layout_item_aggregator.h>

namespace detail {

class Watcher: public Connective<QObject>, public QnCommonModuleAware
{
    Q_OBJECT

    using base_type = Connective<QObject>;

public:
    Watcher(const QnUserResourcePtr& user, QnCommonModule* commonModule);

    QnUserResourcePtr user() const;

    virtual QHash<QnUuid, QnVirtualCameraResourcePtr> cameras() const = 0;

signals:
    void cameraAdded(const QnResourcePtr& resource);
    void cameraRemoved(const QnResourcePtr& resource);

private:
    QnUserResourcePtr m_user;
};

class LayoutBasedWatcher: public Watcher
{
public:
    LayoutBasedWatcher(const QnUserResourcePtr& user, QnCommonModule* commonModule);

    virtual QHash<QnUuid, QnVirtualCameraResourcePtr> cameras() const override;

private:
    void at_resourceAdded(const QnResourcePtr& resource);
    void at_resourceRemoved(const QnResourcePtr& resource);
    void at_layoutItemAdded(const QnUuid& id);
    void at_layoutItemRemoved(const QnUuid& id);

private:
    QnLayoutItemAggregator* m_itemAggregator;
    QHash<QnUuid, QnVirtualCameraResourcePtr> m_cameras;
};

class PermissionsBasedWatcher: public Watcher
{
public:
    PermissionsBasedWatcher(const QnUserResourcePtr& user, QnCommonModule* commonModule);

    virtual QHash<QnUuid, QnVirtualCameraResourcePtr> cameras() const override;

private:
    void addCamera(const QnResourcePtr& resource);
    void removeCamera(const QnResourcePtr& resource);

private:
    QHash<QnUuid, QnVirtualCameraResourcePtr> m_cameras;
};

} // namespace detail
