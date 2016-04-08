#include "available_cameras_watcher.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_access_manager.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/camera_resource.h>
#include <utils/common/connective.h>

class QnAvailableCamerasWatcherPrivate : public Connective<QObject> {
    typedef Connective<QObject> base_type;

public:
    QnAvailableCamerasWatcherPrivate(QnAvailableCamerasWatcher *parent);

    void initialize();
    void addLayout(const QnLayoutResourcePtr &layout);
    void removeLayout(const QnLayoutResourcePtr &layout);
    void addCameraId(const QnUuid &layoutId, const QnUuid &cameraId);
    void addCameraId(const QnUuid &cameraId);
    void removeCameraId(const QnUuid &layoutId, const QnUuid &cameraId);
    void removeCameraId(const QnUuid &cameraId);
    void addCameraResource(const QnVirtualCameraResourcePtr &camera);
    void removeCameraResource(const QnVirtualCameraResourcePtr &camera);

public:
    QnAvailableCamerasWatcher * const q_ptr;
    Q_DECLARE_PUBLIC(QnAvailableCamerasWatcher)

    QnUserResourcePtr user;

    typedef QSet<QnUuid> LayoutIdSet;
    struct CameraInfo {
        QnVirtualCameraResourcePtr camera;
        LayoutIdSet layouts;
    };

    QHash<QnUuid, CameraInfo> cameraInfoById;
    QSet<QnUuid> userLayouts;

    bool acceptAllCameras;
};

QnAvailableCamerasWatcher::QnAvailableCamerasWatcher(QObject *parent)
    : base_type(parent),
      d_ptr(new QnAvailableCamerasWatcherPrivate(this))
{
}

QnAvailableCamerasWatcher::~QnAvailableCamerasWatcher() {
}

QnUserResourcePtr QnAvailableCamerasWatcher::user() const {
    Q_D(const QnAvailableCamerasWatcher);
    return d->user;
}

void QnAvailableCamerasWatcher::setUser(const QnUserResourcePtr &user)
{
    Q_D(QnAvailableCamerasWatcher);
    if (d->user == user)
        return;

    if (d->user)
        disconnect(d->user, nullptr, this, nullptr);

    d->user = user;

    if (d->user) {
        connect(d->user, &QnUserResource::permissionsChanged, this, [this](){
            Q_D(QnAvailableCamerasWatcher);

            bool acceptAllCameras = qnResourceAccessManager->hasGlobalPermission(d->user, Qn::GlobalAdminPermission);
            if (d->acceptAllCameras != acceptAllCameras)
                d->initialize();
        });
    }

    d->initialize();
}

bool QnAvailableCamerasWatcher::isCameraAvailable(const QnUuid &cameraId) const {
    Q_D(const QnAvailableCamerasWatcher);
    return d->cameraInfoById.contains(cameraId);
}

QnVirtualCameraResourceList QnAvailableCamerasWatcher::availableCameras() const {
    Q_D(const QnAvailableCamerasWatcher);
    QnVirtualCameraResourceList result;

    for (const auto &info: d->cameraInfoById) {
        if (info.camera)
            result.append(info.camera);
    }

    return result;
}


QnAvailableCamerasWatcherPrivate::QnAvailableCamerasWatcherPrivate(QnAvailableCamerasWatcher *parent)
    : base_type(parent),
      q_ptr(parent),
      acceptAllCameras(true)
{

    connect(qnResPool, &QnResourcePool::resourceAdded, this, [this](const QnResourcePtr &resource) {
        if (QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>()) {
            if (acceptAllCameras)
                addCameraId(camera->getId());

            addCameraResource(camera);

            return;
        }

        if (QnLayoutResourcePtr layout = resource.dynamicCast<QnLayoutResource>()) {
            if (!user)
                return;

            connect(layout, &QnLayoutResource::parentIdChanged, this, [this](const QnResourcePtr &resource) {
                QnLayoutResourcePtr layout = resource.dynamicCast<QnLayoutResource>();
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
    });

    connect(qnResPool, &QnResourcePool::resourceRemoved, this, [this](const QnResourcePtr &resource) {
        if (QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>()) {
            if (acceptAllCameras)
                removeCameraId(camera->getId());

            removeCameraResource(camera);

            return;
        }

        if (QnLayoutResourcePtr layout = resource.dynamicCast<QnLayoutResource>()) {
            removeLayout(layout);
        }
    });
}

void QnAvailableCamerasWatcherPrivate::initialize() {
    for (const QnUuid &layoutId: userLayouts) {
        QnLayoutResourcePtr layout = qnResPool->getResourceById<QnLayoutResource>(layoutId);
        if (!layout)
            continue;

        disconnect(layout, nullptr, this, nullptr);
    }

    userLayouts.clear();

    Q_Q(QnAvailableCamerasWatcher);

    for (auto it = cameraInfoById.begin(); it != cameraInfoById.end(); ++it) {
        if (it->camera)
            emit q->cameraRemoved(it->camera);
    }

    cameraInfoById.clear();

    if (!user)
        return;

    acceptAllCameras = qnResourceAccessManager->hasGlobalPermission(user, Qn::GlobalAdminPermission);

    if (acceptAllCameras) {
        for (const QnVirtualCameraResourcePtr &camera: qnResPool->getAllCameras(QnResourcePtr(), true))
            addCameraId(camera->getId());
    } else {
        for (const QnLayoutResourcePtr &layout: qnResPool->getResourcesByParentId(user->getId()).filtered<QnLayoutResource>())
            addLayout(layout);
    }
}

void QnAvailableCamerasWatcherPrivate::addLayout(const QnLayoutResourcePtr &layout) {
    const QnUuid layoutId = layout->getId();

    if (userLayouts.contains(layoutId))
        return;

    userLayouts.insert(layoutId);

    connect(layout, &QnLayoutResource::itemAdded, this, [this](const QnLayoutResourcePtr &resource, const QnLayoutItemData &item) {
        addCameraId(resource->getId(), item.resource.id);
    });
    connect(layout, &QnLayoutResource::itemRemoved, this, [this](const QnLayoutResourcePtr &resource, const QnLayoutItemData &item) {
        removeCameraId(resource->getId(), item.resource.id);
    });

    for (const QnLayoutItemData &item: layout->getItems())
        addCameraId(layoutId, item.resource.id);
}

void QnAvailableCamerasWatcherPrivate::removeLayout(const QnLayoutResourcePtr &layout) {
    const QnUuid layoutId = layout->getId();

    if (!userLayouts.remove(layoutId))
        return;

    disconnect(layout, nullptr, this, nullptr);

    for (const QnLayoutItemData &item: layout->getItems())
        removeCameraId(layoutId, item.resource.id);
}

void QnAvailableCamerasWatcherPrivate::addCameraId(const QnUuid &layoutId, const QnUuid &cameraId) {
    QnResourcePtr camera = qnResPool->getResourceById<QnVirtualCameraResource>(cameraId);

    if (!camera)
        return;

    auto &info = cameraInfoById[cameraId];

    bool newItem = info.layouts.isEmpty();

    info.layouts.insert(layoutId);

    if (newItem) {
        if (QnVirtualCameraResourcePtr camera = qnResPool->getResourceById<QnVirtualCameraResource>(cameraId))
            addCameraResource(camera);
    }
}

void QnAvailableCamerasWatcherPrivate::addCameraId(const QnUuid &cameraId) {
    addCameraId(QnUuid(), cameraId);
}

void QnAvailableCamerasWatcherPrivate::removeCameraId(const QnUuid &layoutId, const QnUuid &cameraId) {
    auto it = cameraInfoById.find(cameraId);
    if (it == cameraInfoById.end())
        return;

    it->layouts.remove(layoutId);

    if (it->layouts.isEmpty()) {
        if (it->camera)
            removeCameraResource(it->camera);

        cameraInfoById.erase(it);
    }
}

void QnAvailableCamerasWatcherPrivate::removeCameraId(const QnUuid &cameraId) {
    removeCameraId(QnUuid(), cameraId);
}

void QnAvailableCamerasWatcherPrivate::addCameraResource(const QnVirtualCameraResourcePtr &camera) {
    auto it = cameraInfoById.find(camera->getId());

    if (it == cameraInfoById.end())
        return;

    if (!it->camera) {
        it->camera = camera;

        Q_Q(QnAvailableCamerasWatcher);
        emit q->cameraAdded(camera);
    }
}

void QnAvailableCamerasWatcherPrivate::removeCameraResource(const QnVirtualCameraResourcePtr &camera) {
    auto it = cameraInfoById.find(camera->getId());

    if (it == cameraInfoById.end())
        return;

    if (it->camera) {
        QnVirtualCameraResourcePtr camera = it->camera;

        it->camera.clear();

        Q_Q(QnAvailableCamerasWatcher);
        emit q->cameraRemoved(camera);
    }
}
