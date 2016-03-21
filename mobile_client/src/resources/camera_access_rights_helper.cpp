#include "camera_access_rights_helper.h"

#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <watchers/user_watcher.h>
#include <common/common_module.h>
#include <common/user_permissions.h>

namespace {
    bool userIsAdmin(const QnUserResourcePtr &user) {
        return user->isAdmin() || (user->getPermissions() & Qn::GlobalProtectedPermission);
    }
}

class QnCameraAccessRightsHelperPrivate : public QObject {
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

QnCameraAccessRightsHelper::QnCameraAccessRightsHelper(QObject *parent)
    : QObject(parent)
    , d_ptr(new QnCameraAccessRightsHelperPrivate(this))
{
    Q_D(QnCameraAccessRightsHelper);

    QnUserWatcher *userWatcher = qnCommon->instance<QnUserWatcher>();
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
    d->camera = qnResPool->getResourceById<QnVirtualCameraResource>(id);
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
    if (camera && user) {
        canViewArchive = userIsAdmin(user) || user->getPermissions() & Qn::GlobalViewArchivePermission;
    } else {
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
