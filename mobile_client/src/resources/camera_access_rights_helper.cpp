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

class QnCameraAccessRughtsHelperPrivate : public QObject {
    QnCameraAccessRughtsHelper * const q_ptr;
    Q_DECLARE_PUBLIC(QnCameraAccessRughtsHelper)

public:
    QnCameraAccessRughtsHelperPrivate(QnCameraAccessRughtsHelper *q);

    void updateAccessRights();
    void at_userWatcher_userChanged(const QnUserResourcePtr &newUser);

public:
    QnUserResourcePtr user;
    QnVirtualCameraResourcePtr camera;
    bool canViewArchive;
};

QnCameraAccessRughtsHelper::QnCameraAccessRughtsHelper(QObject *parent)
    : QObject(parent)
    , d_ptr(new QnCameraAccessRughtsHelperPrivate(this))
{
    Q_D(QnCameraAccessRughtsHelper);

    QnUserWatcher *userWatcher = qnCommon->instance<QnUserWatcher>();
    connect(userWatcher, &QnUserWatcher::userChanged, d, &QnCameraAccessRughtsHelperPrivate::at_userWatcher_userChanged);
    d->user = userWatcher->user();
}

QnCameraAccessRughtsHelper::~QnCameraAccessRughtsHelper()
{
}

QString QnCameraAccessRughtsHelper::resourceId() const
{
    Q_D(const QnCameraAccessRughtsHelper);
    if (d->camera)
        return d->camera->getId().toString();

    return QString();
}

void QnCameraAccessRughtsHelper::setResourceId(const QString &id)
{
    if (id == resourceId())
        return;

    Q_D(QnCameraAccessRughtsHelper);
    d->camera = qnResPool->getResourceById<QnVirtualCameraResource>(id);
    d->updateAccessRights();
}

bool QnCameraAccessRughtsHelper::canViewArchive() const
{
    Q_D(const QnCameraAccessRughtsHelper);
    return d->canViewArchive;
}


QnCameraAccessRughtsHelperPrivate::QnCameraAccessRughtsHelperPrivate(QnCameraAccessRughtsHelper *q)
    : q_ptr(q)
    , canViewArchive(false)
{
}

void QnCameraAccessRughtsHelperPrivate::updateAccessRights()
{
    if (camera && user) {
        canViewArchive = userIsAdmin(user) || user->getPermissions() & Qn::GlobalViewArchivePermission;
    } else {
        canViewArchive = false;
    }

    Q_Q(QnCameraAccessRughtsHelper);
    emit q->canViewArchiveChanged();
}

void QnCameraAccessRughtsHelperPrivate::at_userWatcher_userChanged(const QnUserResourcePtr &newUser)
{
    if (user == newUser)
        return;

    user = newUser;

    if (camera)
        updateAccessRights();
}
