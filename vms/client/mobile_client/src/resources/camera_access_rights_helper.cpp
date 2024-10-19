// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_access_rights_helper.h"

#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/resource_access_subject.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/scoped_connections.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/core/watchers/user_watcher.h>

using namespace nx::vms::client::core;

struct QnCameraAccessRightsHelper::Private
{
    QnCameraAccessRightsHelper* const q;
    QnUserResourcePtr user;
    QnVirtualCameraResourcePtr camera;
    bool canViewArchive = false;
    bool canManagePtz = false;
    nx::utils::ScopedConnections connections;

public:
    Private(QnCameraAccessRightsHelper* owner):
        q(owner)
    {
    }

    void updateAccessRights();
    void setUser(const QnUserResourcePtr& value);

private:
    void setCanViewArchive(bool value);
    void setCanManagePtz(bool value);
};

QnCameraAccessRightsHelper::QnCameraAccessRightsHelper(QObject *parent):
    QObject(parent),
    d(new Private(this))
{
}

QnCameraAccessRightsHelper::~QnCameraAccessRightsHelper()
{
}

QnResource* QnCameraAccessRightsHelper::rawResource() const
{
    return d->camera.data();
}

void QnCameraAccessRightsHelper::setRawResource(QnResource* value)
{
    const auto camera = value
        ? value->toSharedPointer().dynamicCast<QnVirtualCameraResource>()
        : QnVirtualCameraResourcePtr();

    if (camera == d->camera)
        return;

    d->connections.reset();
    d->camera = camera;
    d->user.reset();

    if (d->camera)
    {
        if (auto systemContext = SystemContext::fromResource(d->camera); NX_ASSERT(systemContext))
        {
            auto userWatcher = systemContext->userWatcher();

            d->connections << connect(userWatcher,
                &UserWatcher::userChanged,
                this,
                [this](const QnUserResourcePtr& value) { d->setUser(value); });
            d->user = userWatcher->user();
        }
    }

    d->updateAccessRights();
}

bool QnCameraAccessRightsHelper::canViewArchive() const
{
    return d->canViewArchive;
}

bool QnCameraAccessRightsHelper::canManagePtz() const
{
    return d->canManagePtz;
}

void QnCameraAccessRightsHelper::Private::setCanViewArchive(bool value)
{
    if (canViewArchive == value)
        return;

    canViewArchive = value;
    emit q->canViewArchiveChanged();
}

void QnCameraAccessRightsHelper::Private::setCanManagePtz(bool value)
{
    if (canManagePtz == value)
        return;

    canManagePtz = value;
    emit q->canManagePtzChanged();
}

void QnCameraAccessRightsHelper::Private::updateAccessRights()
{
    bool hasViewArchivePermission = false;
    bool hasPtzPermission = false;

    if (camera)
    {
        if (auto systemContext = SystemContext::fromResource(camera); NX_ASSERT(systemContext))
        {
            hasViewArchivePermission = systemContext->resourceAccessManager()->hasPermission(
                user, camera, Qn::Permission::ViewFootagePermission);

            hasPtzPermission = systemContext->resourceAccessManager()->hasPermission(
                user, camera, Qn::Permission::WritePtzPermission);
        }
    }
    setCanViewArchive(hasViewArchivePermission);
    setCanManagePtz(hasPtzPermission);
}

void QnCameraAccessRightsHelper::Private::setUser(const QnUserResourcePtr& value)
{
    if (user == value)
        return;

    user = value;

    if (camera)
        updateAccessRights();
}
