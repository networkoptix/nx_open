// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "access_controller.h"

#include <QtCore/QPointer>

#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/resource_access_subject.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/range_adapters.h>
#include <nx/utils/thread/mutex.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/user_management/user_group_manager.h>

namespace nx::vms::client::core {

using namespace nx::vms::api;
using namespace nx::core::access;

class AccessController::Private: public QObject
{
    AccessController* const q;

public:
    explicit Private(AccessController* q);

    GlobalPermissions globalPermissions() const { return cachedGlobalPermissions; }
    Qn::Permissions permissions(const QnResourcePtr& targetResource) const;

    NotifierPtr createNotifier(const QnResourcePtr& targetResource);

    void updatePermissions(const QnResourceList& targetResources);
    void updateAllPermissions();

public:
    const QPointer<QnResourceAccessManager::Notifier> resourceAccessNotifier;
    QnUserResourcePtr user;

private:
    struct ResourceData
    {
        std::weak_ptr<Notifier> notifier;
        Qn::Permissions current;
    };

    struct ChangeData
    {
        NotifierPtr notifier;
        Qn::Permissions current;
        Qn::Permissions old;
    };

    QHash<QnResourcePtr, ResourceData> resourceData;
    GlobalPermissions cachedGlobalPermissions;
    mutable nx::Mutex resourceDataMutex;

private:
    void handleResourcesAdded(const QnResourceList& resources);
    void handleResourcesRemoved(const QnResourceList& resources);
    void handleAccessChanged(const QnUuid& subjectId);

    void emitChanges(const QHash<QnResourcePtr, ChangeData>& changes);
};

// ------------------------------------------------------------------------------------------------
// AccessController

AccessController::AccessController(
    SystemContext* systemContext,
    QObject* parent)
    :
    base_type(parent),
    SystemContextAware(systemContext),
    d(new Private(this))
{
    NX_CRITICAL(systemContext, "SystemContext must be created.");

    connect(systemContext, &QObject::destroyed, d.get(),
        []() { NX_CRITICAL(false, "Deinitialization order violation."); });
}

AccessController::~AccessController()
{
    // Required here for forward declared scoped pointer destruction.
}

QnUserResourcePtr AccessController::user() const
{
    return d->user;
}

void AccessController::setUser(const QnUserResourcePtr& value)
{
    const bool validUser = NX_ASSERT(!value || value->systemContext() == systemContext());
    const auto newUser = validUser ? value : QnUserResourcePtr{};

    if (d->user == newUser)
        return;

    NX_VERBOSE(this, "Changing user from %1 to %2", d->user, newUser);
    d->user = newUser;

    if (d->resourceAccessNotifier)
        d->resourceAccessNotifier->setSubjectId(newUser ? newUser->getId() : QnUuid{});

    d->updateAllPermissions();
    emit userChanged();
}

Qn::Permissions AccessController::permissions(const QnResourcePtr& targetResource) const
{
    return d->permissions(targetResource);
}

Qn::Permissions AccessController::permissions(const UserGroupData& targetUserGroup) const
{
    // Permissions towards user groups are not watched or cached.
    return calculatePermissions(targetUserGroup);
}

Qn::Permissions AccessController::calculatePermissions(const QnResourcePtr& targetResource) const
{
    return d->user && NX_ASSERT(targetResource)
        ? systemContext()->resourceAccessManager()->permissions(d->user, targetResource)
        : Qn::Permissions{};
}

Qn::Permissions AccessController::calculatePermissions(const UserGroupData& targetUserGroup) const
{
    return d->user
        ? systemContext()->resourceAccessManager()->permissions(d->user, targetUserGroup)
        : Qn::Permissions{};
}

bool AccessController::hasPermissions(
    const QnResourcePtr& targetResource, Qn::Permissions desired) const
{
    return (permissions(targetResource) & desired) == desired;
}

bool AccessController::hasPermissions(
    const UserGroupData& targetUserGroup, Qn::Permissions desired) const
{
    return (permissions(targetUserGroup) & desired) == desired;
}

bool AccessController::hasPermissions(const QnUuid& subjectId, Qn::Permissions desired) const
{
    if (const auto group = systemContext()->userGroupManager()->find(subjectId))
        return hasPermissions(*group, desired);

    const auto user = systemContext()->resourcePool()->getResourceById<QnUserResource>(subjectId);
    return user ? hasPermissions(user, desired) : false;
}

bool AccessController::hasAnyPermission(
    const QnResourcePtr& targetResource, Qn::Permissions desired) const
{
    return (permissions(targetResource) & desired) != Qn::NoPermissions;
}

GlobalPermissions AccessController::globalPermissions() const
{
    return d->globalPermissions();
}

GlobalPermissions AccessController::calculateGlobalPermissions() const
{
    return d->user
        ? systemContext()->resourceAccessManager()->globalPermissions(d->user)
        : GlobalPermissions{};
}

bool AccessController::isDeviceAccessRelevant(AccessRights requiredAccessRights) const
{
    if (!d->user)
        return false;

    const auto accessManager = systemContext()->resourceAccessManager();
    if (accessManager->hasAccessRights(d->user, kAllDevicesGroupId, requiredAccessRights))
        return true;

    const auto devices = systemContext()->resourcePool()->getAllCameras(
        /*parentId*/ QnUuid{}, /*ignoreDesktopCameras*/ true);

    return std::any_of(devices.begin(), devices.end(),
        [this, accessManager, requiredAccessRights](const QnVirtualCameraResourcePtr& device)
        {
            return accessManager->hasAccessRights(d->user, device, requiredAccessRights);
        });
}

bool AccessController::hasGlobalPermissions(GlobalPermissions desired) const
{
    return (globalPermissions() & desired) == desired;
}

bool AccessController::hasPowerUserPermissions() const
{
    return hasGlobalPermissions(GlobalPermission::powerUser);
}

AccessController::NotifierPtr AccessController::createNotifier(const QnResourcePtr& targetResource)
{
    return d->createNotifier(targetResource);
}

void AccessController::updatePermissions(const QnResourceList& targetResources)
{
    d->updatePermissions(targetResources);
}

// ------------------------------------------------------------------------------------------------
// AccessController::Private

AccessController::Private::Private(AccessController* q):
    q(q),
    resourceAccessNotifier(
        q->systemContext()->resourceAccessManager()->createNotifier(/*parent*/ this))
{
    const auto resourcePool = q->systemContext()->resourcePool();
    const auto resourceAccessManager = q->systemContext()->resourceAccessManager();

    connect(resourcePool, &QnResourcePool::resourcesAdded,
        this, &Private::handleResourcesAdded);

    connect(resourcePool, &QnResourcePool::resourcesRemoved,
        this, &Private::handleResourcesRemoved);

    connect(resourceAccessNotifier.get(), &QnResourceAccessManager::Notifier::resourceAccessChanged,
        this, &Private::handleAccessChanged);

    connect(resourceAccessManager, &QnResourceAccessManager::resourceAccessReset,
        this, &Private::updateAllPermissions);

    connect(resourceAccessManager, &QnResourceAccessManager::permissionsDependencyChanged,
        this, &Private::updatePermissions);
}

Qn::Permissions AccessController::Private::permissions(const QnResourcePtr& targetResource) const
{
    if (!targetResource)
        return {};

    {
        NX_MUTEX_LOCKER lk(&resourceDataMutex);
        const auto it = resourceData.find(targetResource);
        if (it != resourceData.end())
            return it->current;
    }

    return q->calculatePermissions(targetResource);
}

AccessController::NotifierPtr AccessController::Private::createNotifier(
    const QnResourcePtr& targetResource)
{
    static const auto dummyNotifier = std::make_shared<Notifier>();

    if (!NX_ASSERT(targetResource))
        return dummyNotifier;

    if (!NX_ASSERT(targetResource->resourcePool(),
        "Notifier for a resource not in a resource pool won't work"))
    {
        return dummyNotifier;
    }

    if (!NX_ASSERT(targetResource->systemContext() == q->systemContext(),
        "Notifier for a resource from a different system context won't work"))
    {
        return dummyNotifier;
    }

    NX_MUTEX_LOCKER lk(&resourceDataMutex);
    const auto data = resourceData.value(targetResource);

    if (const auto notifier = data.notifier.lock())
        return notifier;

    const auto notifierDeleter = nx::utils::guarded(this,
        [this, targetResource](Notifier* notifier)
        {
            NX_MUTEX_LOCKER lk(&resourceDataMutex);
            resourceData.remove(targetResource);
            delete notifier;
        });

    const std::shared_ptr<Notifier> notifier(new Notifier(), notifierDeleter);
    resourceData.insert(targetResource, {notifier, q->calculatePermissions(targetResource)});

    return notifier;
}

void AccessController::Private::updatePermissions(const QnResourceList& targetResources)
{
    QHash<QnResourcePtr, ChangeData> changes;
    {
        NX_MUTEX_LOCKER lk(&resourceDataMutex);
        for (const auto& targetResource: targetResources)
        {
            const auto it = resourceData.find(targetResource);
            if (it == resourceData.end())
                continue;

            const auto notifier = it->notifier.lock();
            if (!notifier)
                continue;

            const auto old = it->current;
            const auto current = q->calculatePermissions(targetResource);
            if (old == current)
                continue;

            it->current = current;
            changes.insert(targetResource, {notifier, current, old});
        }
    }

    NX_VERBOSE(q, "Permissions maybe changed for %1", targetResources);
    emit q->permissionsMaybeChanged(targetResources, AccessController::QPrivateSignal{});
    emitChanges(changes);
}

void AccessController::Private::updateAllPermissions()
{
    NX_VERBOSE(q, "Updating all permissions");

    QHash<QnResourcePtr, ChangeData> changes;
    GlobalPermissions oldGlobal, currentGlobal;
    {
        NX_MUTEX_LOCKER lk(&resourceDataMutex);

        oldGlobal = cachedGlobalPermissions;
        currentGlobal = q->calculateGlobalPermissions();
        cachedGlobalPermissions = currentGlobal;

        for (auto&& [targetResource, data]: nx::utils::keyValueRange(resourceData))
        {
            const auto notifier = data.notifier.lock();
            if (!notifier)
                continue;

            const auto old = data.current;
            const auto current = q->calculatePermissions(targetResource);
            if (old == current)
                continue;

            data.current = current;
            changes.insert(targetResource, {notifier, current, old});
        }
    }

    if (oldGlobal != currentGlobal)
    {
        NX_VERBOSE(q, "Global permissions changed from %1 to %2", oldGlobal, currentGlobal);

        emit q->globalPermissionsChanged(currentGlobal, oldGlobal,
            AccessController::QPrivateSignal{});
    }

    const auto administrativePermissions =
        GlobalPermission::administrator | GlobalPermission::powerUser;

    oldGlobal &= administrativePermissions;
    currentGlobal &= administrativePermissions;

    if (oldGlobal != currentGlobal)
    {
        NX_VERBOSE(q, "Administrative permissions changed from %1 to %2", oldGlobal, currentGlobal);

        emit q->administrativePermissionsChanged(currentGlobal, oldGlobal,
            AccessController::QPrivateSignal{});
    }

    NX_VERBOSE(q, "Permissions maybe changed for all resources");
    emit q->permissionsMaybeChanged({}, AccessController::QPrivateSignal{});
    emitChanges(changes);
}

void AccessController::Private::handleResourcesAdded(const QnResourceList& resources)
{
    NX_VERBOSE(q, "Resources added, permissions maybe changed for %1", resources);
    emit q->permissionsMaybeChanged(resources, AccessController::QPrivateSignal{});
}

void AccessController::Private::handleResourcesRemoved(const QnResourceList& resources)
{
    {
        NX_MUTEX_LOCKER lk(&resourceDataMutex);

        for (const auto& resource: resources)
            resourceData.remove(resource);
    }

    NX_VERBOSE(q, "Resources removed, permissions maybe changed for %1", resources);
    emit q->permissionsMaybeChanged(resources, AccessController::QPrivateSignal{});
}

void AccessController::Private::handleAccessChanged(const QnUuid& subjectId)
{
    if (user && subjectId == user->getId())
        updateAllPermissions();
}

void AccessController::Private::emitChanges(const QHash<QnResourcePtr, ChangeData>& changes)
{
    for (const auto& [targetResource, change]: nx::utils::constKeyValueRange(changes))
    {
        NX_VERBOSE(q, "Notifying about permissions change for %1 from %2 to %3",
            targetResource, change.old, change.current);

        emit change.notifier->permissionsChanged(targetResource, change.current, change.old,
            AccessController::QPrivateSignal{});
    }
}

} // namespace nx::vms::client::core
