#include "camera_access_rights_helper.h"

#include <mobile_client/mobile_client_common_module_aware.h>

#include <nx/utils/log/assert.h>
#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_access/resource_access_manager.h>
#include <watchers/user_watcher.h>
#include <common/common_module.h>

class QnCameraAccessRightsHelperPrivate : public QObject, public QnConnectionContextAware
{
    QnCameraAccessRightsHelper * const q_ptr;
    Q_DECLARE_PUBLIC(QnCameraAccessRightsHelper)

public:
    QnCameraAccessRightsHelperPrivate(QnCameraAccessRightsHelper *q);

    void updateAccessRights();
    void at_userWatcher_userChanged(const QnUserResourcePtr &newUser);

public:
    QnUserResourcePtr user;
    QnVirtualCameraResourcePtr camera;
    bool canViewArchive;
};

QnCameraAccessRightsHelper::QnCameraAccessRightsHelper(QObject *parent):
    QObject(parent),
    d_ptr(new QnCameraAccessRightsHelperPrivate(this))
{
    Q_D(QnCameraAccessRightsHelper);

    QnUserWatcher *userWatcher = commonModule()->instance<QnUserWatcher>();
    connect(userWatcher, &QnUserWatcher::userChanged, d, &QnCameraAccessRightsHelperPrivate::at_userWatcher_userChanged);
    d->user = userWatcher->user();
}

QnCameraAccessRightsHelper::~QnCameraAccessRightsHelper()
{
}

QString QnCameraAccessRightsHelper::resourceId() const
{
    Q_D(const QnCameraAccessRightsHelper);
    if (d->camera)
        return d->camera->getId().toString();

    return QString();
}

void QnCameraAccessRightsHelper::setResourceId(const QString &id)
{
    if (id == resourceId())
        return;

    Q_D(QnCameraAccessRightsHelper);
    d->camera = resourcePool()->getResourceById<QnVirtualCameraResource>(QnUuid(id));
    d->updateAccessRights();
}

bool QnCameraAccessRightsHelper::canViewArchive() const
{
    Q_D(const QnCameraAccessRightsHelper);
    return d->canViewArchive;
}


QnCameraAccessRightsHelperPrivate::QnCameraAccessRightsHelperPrivate(QnCameraAccessRightsHelper *q)
    : q_ptr(q)
    , canViewArchive(false)
{
}

void QnCameraAccessRightsHelperPrivate::updateAccessRights()
{
    if (camera && user)
    {
        canViewArchive = resourceAccessManager()->hasGlobalPermission(user, Qn::GlobalViewArchivePermission);
    }
    else
    {
        canViewArchive = false;
    }

    Q_Q(QnCameraAccessRightsHelper);
    emit q->canViewArchiveChanged();
}

void QnCameraAccessRightsHelperPrivate::at_userWatcher_userChanged(const QnUserResourcePtr &newUser)
{
    if (user == newUser)
        return;

    user = newUser;

    if (camera)
        updateAccessRights();
}
