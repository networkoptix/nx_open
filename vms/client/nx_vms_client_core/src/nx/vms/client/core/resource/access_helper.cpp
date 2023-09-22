// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "access_helper.h"

#include <QtQml/QtQml>

#include <common/common_globals.h>
#include <core/resource/resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/system_context.h>

namespace nx::vms::client::core {

struct AccessHelper::Private
{
    AccessHelper* const q;
    QnResourcePtr resource;
    AccessController::NotifierPtr notifier;
    Qn::Permissions permissions{};

    void setPermissions(Qn::Permissions value)
    {
        if (permissions == value)
            return;

        permissions = value;
        emit q->permissionsChanged();
    }
};

AccessHelper::AccessHelper(
    SystemContext* context,
    QObject* parent)
    :
    QObject(parent),
    SystemContextAware(context),
    d(new Private{.q = this})
{
}

AccessHelper::AccessHelper():
    SystemContextAware(SystemContext::fromQmlContext(this)),
    d(new Private{.q = this})
{
}

AccessHelper::~AccessHelper()
{
    // Required here for forward-declared scoped pointer destruction.
}

QnUuid AccessHelper::resourceId() const
{
    return d->resource ? d->resource->getId() : QnUuid{};
}

void AccessHelper::setResourceId(const QnUuid& value)
{
    setResource(resourcePool()->getResourceById(value));
}

QnResourcePtr AccessHelper::resource() const
{
    return d->resource;
}

void AccessHelper::setResource(const QnResourcePtr& value)
{
    if (d->resource == value)
        return;

    if (d->notifier)
    {
        d->notifier->disconnect(this);
        d->notifier.reset();
    }

    d->resource = value;
    if (d->resource)
    {
        QQmlEngine::setObjectOwnership(d->resource.get(), QQmlEngine::CppOwnership);
        d->notifier = accessController()->createNotifier(d->resource);

        connect(d->notifier.get(), &AccessController::Notifier::permissionsChanged, this,
            [this]() { d->setPermissions(accessController()->permissions(d->resource)); });
    }

    emit resourceChanged();

    d->setPermissions(accessController()->permissions(d->resource));
}

QnResource* AccessHelper::rawResource() const
{
    return d->resource.get();
}

void AccessHelper::setRawResource(QnResource* value)
{
    if (value)
    {
        const auto resourcePtr = value->toSharedPointer();
        NX_ASSERT(resourcePtr);
        setResource(resourcePtr);
    }
    else
    {
        setResource({});
    }
}

bool AccessHelper::canViewContent() const
{
    return d->permissions.testFlag(Qn::ViewContentPermission);
}

bool AccessHelper::canViewLive() const
{
    return d->permissions.testFlag(Qn::ViewLivePermission);
}

bool AccessHelper::canViewArchive() const
{
    return d->permissions.testFlag(Qn::ViewFootagePermission);
}

bool AccessHelper::canViewBookmarks() const
{
    return d->permissions.testFlag(Qn::ViewBookmarksPermission);
}

bool AccessHelper::canEditBookmarks() const
{
    return d->permissions.testFlag(Qn::ManageBookmarksPermission);
}

bool AccessHelper::canExportArchive() const
{
    return d->permissions.testFlag(Qn::ExportPermission);
}

bool AccessHelper::canUsePtz() const
{
    return d->permissions.testFlag(Qn::WritePtzPermission);
}

bool AccessHelper::canUseTwoWayAudio() const
{
    return d->permissions.testFlag(Qn::TwoWayAudioPermission);
}

bool AccessHelper::canUseSoftTriggers() const
{
    return d->permissions.testFlag(Qn::SoftTriggerPermission);
}

bool AccessHelper::canUseDeviceIO() const
{
    return d->permissions.testFlag(Qn::DeviceInputPermission);
}

void AccessHelper::registerQmlType()
{
    qmlRegisterType<AccessHelper>("nx.vms.client.core", 1, 0, "AccessHelper");
}

} // namespace nx::vms::client::core
