#include "available_cameras_watcher.h"

#include <utils/common/counter_hash.h>

namespace detail {

class Watcher: public Connective<QObject>
{
    Q_OBJECT

    using base_type = Connective<QObject>;

public:
    explicit Watcher(const QnUserResourcePtr& user);

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
    LayoutBasedWatcher(const QnUserResourcePtr& user);

    virtual QHash<QnUuid, QnVirtualCameraResourcePtr> cameras() const override;

private:
    void at_resourceAdded(const QnResourcePtr& resource);
    void at_resourceRemoved(const QnResourcePtr& resource);

    void addLayout(const QnLayoutResourcePtr& layout);
    void removeLayout(const QnLayoutResourcePtr& layout);
    void at_layoutItemAdded(const QnLayoutResourcePtr& resource, const QnLayoutItemData& item);
    void at_layoutItemRemoved(const QnLayoutResourcePtr& resource, const QnLayoutItemData& item);

private:
    QHash<QnUuid, QnVirtualCameraResourcePtr> m_cameras;
    QnCounterHash<QnUuid> m_camerasCounter;
};

class PermissionsBasedWatcher: public Watcher
{
public:
    PermissionsBasedWatcher(const QnUserResourcePtr& user);

    virtual QHash<QnUuid, QnVirtualCameraResourcePtr> cameras() const override;

private:
    void at_resourceAdded(const QnResourcePtr& resource);
    void at_resourceRemoved(const QnResourcePtr& resource);

private:
    QHash<QnUuid, QnVirtualCameraResourcePtr> m_cameras;
};

} // namespace detail
