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

    bool userHasAllMediaAccess = false;
    bool compatibilityMode = false;

    QScopedPointer<detail::Watcher> watcher;
};

QnAvailableCamerasWatcher::QnAvailableCamerasWatcher(QObject* parent):
    base_type(parent),
    QnCommonModuleAware(parent),
    d_ptr(new QnAvailableCamerasWatcherPrivate(this))
{
    Q_D(QnAvailableCamerasWatcher);

    connect(globalPermissionsManager(), &QnGlobalPermissionsManager::globalPermissionsChanged,
        this,
        [this, d](const QnResourceAccessSubject& subject, Qn::GlobalPermissions value)
        {
            if (!subject.isUser() || subject.user() != d->user)
                return;

            const bool userHasAllMediaAccess = value.testFlag(Qn::GlobalAccessAllMediaPermission);
            if (d->userHasAllMediaAccess != userHasAllMediaAccess)
            {
                d->userHasAllMediaAccess = userHasAllMediaAccess;
                d->updateWatcher();
            }
        });
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
    d->userHasAllMediaAccess = user &&
        globalPermissionsManager()->hasGlobalPermission(user, Qn::GlobalAccessAllMediaPermission);
    d->updateWatcher();
}

QnVirtualCameraResourceList QnAvailableCamerasWatcher::availableCameras() const
{
    Q_D(const QnAvailableCamerasWatcher);
    if (!d->watcher)
        return QnVirtualCameraResourceList();

    return d->watcher->cameras().values();
}

bool QnAvailableCamerasWatcher::compatibilityMode() const
{
    Q_D(const QnAvailableCamerasWatcher);
    return d->compatibilityMode;
}

void QnAvailableCamerasWatcher::setCompatiblityMode(bool compatibilityMode)
{
    Q_D(QnAvailableCamerasWatcher);
    if (d->compatibilityMode == compatibilityMode)
        return;

    d->compatibilityMode = compatibilityMode;
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
        const auto cameras = watcher->cameras();
        watcher.reset();

        for (const auto& camera: cameras)
            emit q->cameraRemoved(camera);
    }

    if (!user)
        return;

    if (compatibilityMode && !userHasAllMediaAccess)
        watcher.reset(new detail::LayoutBasedWatcher(user, q->commonModule()));
    else
        watcher.reset(new detail::PermissionsBasedWatcher(user, q->commonModule()));

    QObject::connect(watcher.data(), &detail::Watcher::cameraAdded,
        q, &QnAvailableCamerasWatcher::cameraAdded);
    QObject::connect(watcher.data(), &detail::Watcher::cameraRemoved,
        q, &QnAvailableCamerasWatcher::cameraRemoved);

    for (const auto& camera: watcher->cameras())
        emit q->cameraAdded(camera);
}
