// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "available_cameras_watcher.h"

#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/resource_access_subject.h>

#include "available_cameras_watcher_p.h"

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

    const QScopedPointer<QnResourceAccessManager::Notifier> notifier;
    QScopedPointer<detail::Watcher> watcher;
};

QnAvailableCamerasWatcher::QnAvailableCamerasWatcher(
    nx::vms::common::SystemContext* systemContext,
    QObject* parent)
    :
    base_type(parent),
    nx::vms::common::SystemContextAware(systemContext),
    d_ptr(new QnAvailableCamerasWatcherPrivate(this))
{
    const auto handleAccessRightsChanged =
        [this]()
        {
            Q_D(QnAvailableCamerasWatcher);
            if (!d->user)
                return;

            const bool userHasAllMediaAccess = resourceAccessManager()->hasAccessRights(
                d->user, nx::vms::api::kAllDevicesGroupId, nx::vms::api::AccessRight::view);

            if (d->userHasAllMediaAccess != userHasAllMediaAccess)
            {
                d->userHasAllMediaAccess = userHasAllMediaAccess;
                d->updateWatcher();
            }
        };

    connect(d_ptr->notifier.get(), &QnResourceAccessManager::Notifier::resourceAccessChanged,
        this, handleAccessRightsChanged);

    connect(resourceAccessManager(), &QnResourceAccessManager::resourceAccessReset,
        this, handleAccessRightsChanged);
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
    d->notifier->setSubjectId(user ? user->getId() : nx::Uuid{});

    d->userHasAllMediaAccess = user && resourceAccessManager()->hasAccessRights(
        user, nx::vms::api::kAllDevicesGroupId, nx::vms::api::AccessRight::view);

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
    q_ptr(parent),
    notifier(parent->resourceAccessManager()->createNotifier())
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
        watcher.reset(new detail::LayoutBasedWatcher(user, q->systemContext()));
    else
        watcher.reset(new detail::PermissionsBasedWatcher(user, q->systemContext()));

    QObject::connect(watcher.data(), &detail::Watcher::cameraAdded,
        q, &QnAvailableCamerasWatcher::cameraAdded);
    QObject::connect(watcher.data(), &detail::Watcher::cameraRemoved,
        q, &QnAvailableCamerasWatcher::cameraRemoved);

    for (const auto& camera: watcher->cameras())
        emit q->cameraAdded(camera);
}
