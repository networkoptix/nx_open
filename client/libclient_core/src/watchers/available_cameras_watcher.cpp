#include "available_cameras_watcher.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_access_manager.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/camera_resource.h>
#include <utils/common/connective.h>

class QnAvailableCamerasWatcherPrivate : public Connective<QObject>
{
    using base_type = Connective<QObject>;

    QnAvailableCamerasWatcher * const q_ptr;
    Q_DECLARE_PUBLIC(QnAvailableCamerasWatcher)

public:
    QnAvailableCamerasWatcherPrivate(QnAvailableCamerasWatcher* parent);

    void initialize();
    void addLayout(const QnLayoutResourcePtr& layout);
    void removeLayout(const QnLayoutResourcePtr& layout);
    void addCameraId(const QnUuid& layoutId, const QnUuid& cameraId);
    void addCameraId(const QnUuid& cameraId);
    void removeCameraId(const QnUuid& layoutId, const QnUuid& cameraId);
    void removeCameraId(const QnUuid& cameraId);
    void addCameraResource(const QnVirtualCameraResourcePtr& camera);
    void removeCameraResource(const QnVirtualCameraResourcePtr& camera);

    void at_resourceAdded(const QnResourcePtr& resource);
    void at_resourceRemoved(const QnResourcePtr& resource);

public:
    QnUserResourcePtr user;

    using LayoutIdSet = QSet<QnUuid>;
    struct CameraInfo
    {
        QnVirtualCameraResourcePtr camera;
        LayoutIdSet layouts;
    };

    QHash<QnUuid, CameraInfo> cameraInfoById;
    QSet<QnUuid> userLayouts;

    bool acceptAllCameras = true;
    bool useLayouts = true;
};

QnAvailableCamerasWatcher::QnAvailableCamerasWatcher(QObject* parent) :
    base_type(parent),
    d_ptr(new QnAvailableCamerasWatcherPrivate(this))
{
}

QnAvailableCamerasWatcher::~QnAvailableCamerasWatcher()
{
}

QnUserResourcePtr QnAvailableCamerasWatcher::user() const
{
    Q_D(const QnAvailableCamerasWatcher);
    return d->user;
}

void QnAvailableCamerasWatcher::setUser(const QnUserResourcePtr& user)
{
    Q_D(QnAvailableCamerasWatcher);
    if (d->user == user)
        return;

    if (d->user)
        disconnect(d->user, nullptr, this, nullptr);

    d->user = user;

    if (d->user)
    {
        connect(d->user, &QnUserResource::permissionsChanged, this, [this]()
        {
            Q_D(QnAvailableCamerasWatcher);

            bool acceptAllCameras = qnResourceAccessManager->hasGlobalPermission(d->user, Qn::GlobalAdminPermission);
            if (d->acceptAllCameras != acceptAllCameras)
                d->initialize();
        });
    }

    d->initialize();
}

bool QnAvailableCamerasWatcher::isCameraAvailable(const QnUuid& cameraId) const
{
    Q_D(const QnAvailableCamerasWatcher);
    return d->cameraInfoById.contains(cameraId);
}

QnVirtualCameraResourceList QnAvailableCamerasWatcher::availableCameras() const
{
    Q_D(const QnAvailableCamerasWatcher);
    QnVirtualCameraResourceList result;

    for (const auto& info: d->cameraInfoById)
    {
        if (info.camera)
            result.append(info.camera);
    }

    return result;
}

bool QnAvailableCamerasWatcher::useLayouts() const
{
    Q_D(const QnAvailableCamerasWatcher);
    return d->useLayouts;
}

void QnAvailableCamerasWatcher::setUseLayouts(bool useLayouts)
{
    Q_D(QnAvailableCamerasWatcher);
    if (d->useLayouts == useLayouts)
        return;

    d->useLayouts = useLayouts;
    d->initialize();
}


QnAvailableCamerasWatcherPrivate::QnAvailableCamerasWatcherPrivate(QnAvailableCamerasWatcher* parent) :
    base_type(parent),
    q_ptr(parent)
{
    connect(qnResPool, &QnResourcePool::resourceAdded,
            this, &QnAvailableCamerasWatcherPrivate::at_resourceAdded);
    connect(qnResPool, &QnResourcePool::resourceRemoved,
            this, &QnAvailableCamerasWatcherPrivate::at_resourceRemoved);
}

void QnAvailableCamerasWatcherPrivate::initialize()
{
    for (const auto& layoutId: userLayouts)
    {
        if (const auto layout = qnResPool->getResourceById<QnLayoutResource>(layoutId))
            disconnect(layout, nullptr, this, nullptr);
    }

    userLayouts.clear();

    Q_Q(QnAvailableCamerasWatcher);

    for (const auto& info: cameraInfoById)
    {
        if (info.camera)
            emit q->cameraRemoved(info.camera);
    }

    cameraInfoById.clear();

    if (!user)
        return;

    acceptAllCameras = !useLayouts || qnResourceAccessManager->hasGlobalPermission(user, Qn::GlobalAdminPermission);

    if (acceptAllCameras)
    {
        for (const auto& camera: qnResPool->getAllCameras(QnResourcePtr(), true))
            addCameraId(camera->getId());
    }
    else
    {
        for (const auto& layout: qnResPool->getResourcesByParentId(user->getId()).filtered<QnLayoutResource>())
            addLayout(layout);
    }
}

void QnAvailableCamerasWatcherPrivate::addLayout(const QnLayoutResourcePtr& layout)
{
    const auto layoutId = layout->getId();

    if (userLayouts.contains(layoutId))
        return;

    userLayouts.insert(layoutId);

    connect(layout, &QnLayoutResource::itemAdded,
            this, [this](const QnLayoutResourcePtr& resource, const QnLayoutItemData& item)
    {
        addCameraId(resource->getId(), item.resource.id);
    });
    connect(layout, &QnLayoutResource::itemRemoved,
            this, [this](const QnLayoutResourcePtr& resource, const QnLayoutItemData& item)
    {
        removeCameraId(resource->getId(), item.resource.id);
    });

    for (const auto& item: layout->getItems())
        addCameraId(layoutId, item.resource.id);
}

void QnAvailableCamerasWatcherPrivate::removeLayout(const QnLayoutResourcePtr& layout)
{
    const auto layoutId = layout->getId();

    if (!userLayouts.remove(layoutId))
        return;

    disconnect(layout, nullptr, this, nullptr);

    for (const auto& item: layout->getItems())
        removeCameraId(layoutId, item.resource.id);
}

void QnAvailableCamerasWatcherPrivate::addCameraId(const QnUuid& layoutId, const QnUuid& cameraId)
{
    const auto camera = qnResPool->getResourceById<QnVirtualCameraResource>(cameraId);

    if (!camera)
        return;

    auto info = cameraInfoById[cameraId];

    bool newItem = info.layouts.isEmpty();

    info.layouts.insert(layoutId);

    if (newItem)
    {
        if (const auto camera = qnResPool->getResourceById<QnVirtualCameraResource>(cameraId))
            addCameraResource(camera);
    }
}

void QnAvailableCamerasWatcherPrivate::addCameraId(const QnUuid& cameraId)
{
    addCameraId(QnUuid(), cameraId);
}

void QnAvailableCamerasWatcherPrivate::removeCameraId(const QnUuid& layoutId, const QnUuid& cameraId)
{
    auto it = cameraInfoById.find(cameraId);
    if (it == cameraInfoById.end())
        return;

    it->layouts.remove(layoutId);

    if (it->layouts.isEmpty())
    {
        if (it->camera)
            removeCameraResource(it->camera);

        cameraInfoById.erase(it);
    }
}

void QnAvailableCamerasWatcherPrivate::removeCameraId(const QnUuid& cameraId)
{
    removeCameraId(QnUuid(), cameraId);
}

void QnAvailableCamerasWatcherPrivate::addCameraResource(const QnVirtualCameraResourcePtr& camera)
{
    auto it = cameraInfoById.find(camera->getId());

    if (it == cameraInfoById.end())
        return;

    if (!it->camera)
    {
        it->camera = camera;

        Q_Q(QnAvailableCamerasWatcher);
        emit q->cameraAdded(camera);
    }
}

void QnAvailableCamerasWatcherPrivate::removeCameraResource(const QnVirtualCameraResourcePtr& camera)
{
    auto it = cameraInfoById.find(camera->getId());

    if (it == cameraInfoById.end())
        return;

    if (it->camera)
    {
        QnVirtualCameraResourcePtr camera = it->camera;

        it->camera.clear();

        Q_Q(QnAvailableCamerasWatcher);
        emit q->cameraRemoved(camera);
    }
}

void QnAvailableCamerasWatcherPrivate::at_resourceAdded(const QnResourcePtr& resource)
{
    if (const auto camera = resource.dynamicCast<QnVirtualCameraResource>())
    {
        if (acceptAllCameras)
            addCameraId(camera->getId());

        addCameraResource(camera);

        return;
    }

    if (const auto layout = resource.dynamicCast<QnLayoutResource>())
    {
        if (!user)
            return;

        connect(layout, &QnLayoutResource::parentIdChanged,
                this, [this](const QnResourcePtr& resource)
        {
            const auto layout = resource.dynamicCast<QnLayoutResource>();
            if (!layout)
                return;

            if (!user)
                return;

            if (layout->getParentId() == user->getId())
                addLayout(layout);
            else
                removeLayout(layout);
        });

        if (layout->getParentId() != user->getId())
            return;

        addLayout(layout);
    }
}

void QnAvailableCamerasWatcherPrivate::at_resourceRemoved(const QnResourcePtr& resource)
{
    if (const auto camera = resource.dynamicCast<QnVirtualCameraResource>())
    {
        if (acceptAllCameras)
            removeCameraId(camera->getId());

        removeCameraResource(camera);

        return;
    }

    if (const auto layout = resource.dynamicCast<QnLayoutResource>())
        removeLayout(layout);
}
