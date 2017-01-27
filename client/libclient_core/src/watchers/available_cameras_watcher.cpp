#include "available_cameras_watcher.h"
#include "available_cameras_watcher_p.h"

#include <core/resource_access/global_permissions_manager.h>
#include <core/resource/camera_resource.h>

class QnAvailableCamerasWatcherPrivate
{
    QnAvailableCamerasWatcher* const q_ptr;
    Q_DECLARE_PUBLIC(QnAvailableCamerasWatcher)

public:
    QnAvailableCamerasWatcherPrivate(QnAvailableCamerasWatcher* parent);

    void updateWatcher();

public:
    QnUserResourcePtr user;

    bool acceptAllCameras = false;
    bool useLayouts = false;

    QScopedPointer<detail::Watcher> watcher;
};

QnAvailableCamerasWatcher::QnAvailableCamerasWatcher(QObject* parent):
    base_type(parent),
    d_ptr(new QnAvailableCamerasWatcherPrivate(this))
{
    Q_D(QnAvailableCamerasWatcher);

    connect(qnGlobalPermissionsManager, &QnGlobalPermissionsManager::globalPermissionsChanged,
        this,
        [this, d](const QnResourceAccessSubject& subject, Qn::GlobalPermissions value)
        {
            if (!subject.isUser() || subject.user() != d->user)
                return;

            bool acceptAllCameras = value.testFlag(Qn::GlobalAdminPermission);
            if (d->acceptAllCameras != acceptAllCameras)
            {
                d->acceptAllCameras = acceptAllCameras;
                d->updateWatcher();
            }
        });

    d->updateWatcher();
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

    d->user = user;
    d->updateWatcher();
}

QnVirtualCameraResourceList QnAvailableCamerasWatcher::availableCameras() const
{
    Q_D(const QnAvailableCamerasWatcher);
    if (!d->watcher)
        return QnVirtualCameraResourceList();

    return d->watcher->cameras().values();
}

bool QnAvailableCamerasWatcher::useLayouts() const
{
    Q_D(const QnAvailableCamerasWatcher);
    return d->useLayouts;
}

void QnAvailableCamerasWatcher::setUseLayouts(bool useLayouts)
{
    Q_D(QnAvailableCamerasWatcher);
    if (this->useLayouts() == useLayouts)
        return;

    d->useLayouts = useLayouts;
    d->updateWatcher();
}

QnAvailableCamerasWatcherPrivate::QnAvailableCamerasWatcherPrivate(
    QnAvailableCamerasWatcher* parent)
    :
    q_ptr(parent)
{
}

void QnAvailableCamerasWatcherPrivate::updateWatcher()
{
    Q_Q(QnAvailableCamerasWatcher);

    if (watcher)
    {
        for (const auto& camera: watcher->cameras())
            emit q->cameraRemoved(camera);

        watcher.reset();
    }

    if (acceptAllCameras)
        watcher.reset(new detail::PermissionsBasedWatcher(QnUserResourcePtr()));
    else if (useLayouts)
        watcher.reset(new detail::LayoutBasedWatcher(user));
    else
        watcher.reset(new detail::PermissionsBasedWatcher(user));

    QObject::connect(watcher.data(), &detail::Watcher::cameraAdded,
        q, &QnAvailableCamerasWatcher::cameraAdded);
    QObject::connect(watcher.data(), &detail::Watcher::cameraRemoved,
        q, &QnAvailableCamerasWatcher::cameraRemoved);

    for (const auto& camera: watcher->cameras())
        emit q->cameraAdded(camera);
}
