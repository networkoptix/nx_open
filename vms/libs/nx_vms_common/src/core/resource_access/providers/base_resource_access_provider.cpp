// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "base_resource_access_provider.h"

#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/resource_access_filter.h>
#include <core/resource_access/resource_access_subjects_cache.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>

#include <core/resource/user_resource.h>
#include <common/common_module.h>
#include <core/resource_access/global_permissions_manager.h>
#include <nx/vms/api/data/user_role_data.h>

namespace nx::core::access {

BaseResourceAccessProvider::BaseResourceAccessProvider(Mode mode, QObject* parent):
    base_type(mode, parent),
    QnCommonModuleAware(parent),
    m_mutex(nx::Mutex::Recursive),
    m_accessibleResources()
{
    if (mode == Mode::cached)
    {
        connect(commonModule()->resourcePool(), &QnResourcePool::resourceAdded, this,
            &BaseResourceAccessProvider::handleResourceAdded);
        connect(commonModule()->resourcePool(), &QnResourcePool::resourceRemoved, this,
            &BaseResourceAccessProvider::handleResourceRemoved);

        connect(userRolesManager(), &QnUserRolesManager::userRoleAddedOrUpdated, this,
            &BaseResourceAccessProvider::handleRoleAddedOrUpdated);
        connect(userRolesManager(), &QnUserRolesManager::userRoleRemoved, this,
            &BaseResourceAccessProvider::handleRoleRemoved);
    }
}

BaseResourceAccessProvider::~BaseResourceAccessProvider()
{
}

bool BaseResourceAccessProvider::hasAccess(const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource) const
{
    if (!acceptable(subject, resource))
        return false;

    if (mode() == Mode::direct)
    {
        return isSubjectEnabled(subject)
            && calculateAccess(subject, resource,
                globalPermissionsManager()->globalPermissions(subject));
    }

    /**
     * We can get cache miss in the following scenarios:
     * - New user was added.
     * - Global permissions manager emits 'changed'.
     * - Access manager recalculates all permissions.
     * - Access provider checks 'has access' to this user itself.
     */
    NX_MUTEX_LOCKER lk(&m_mutex);

    const auto iterator = m_accessibleResources.constFind(subject.id());
    return iterator == m_accessibleResources.end()
        ? false
        : iterator->contains(resource->getId());
}

QSet<QnUuid> BaseResourceAccessProvider::accessibleResources(const QnResourceAccessSubject& subject) const
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    return m_accessibleResources.value(subject.id());
}

Source BaseResourceAccessProvider::accessibleVia(
    const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource,
    QnResourceList* providers) const
{
    if (!hasAccess(subject, resource))
        return Source::none;

    if (providers)
        fillProviders(subject, resource, *providers);

    return baseSource();
}

void BaseResourceAccessProvider::beforeUpdate()
{
    if (mode() == Mode::direct)
        return;

    NX_MUTEX_LOCKER lk(&m_mutex);
    m_accessibleResources.clear();
}

void BaseResourceAccessProvider::afterUpdate()
{
    if (mode() == Mode::direct)
        return;

    const auto resources = commonModule()->resourcePool()->getResources();
    const auto subjects = resourceAccessSubjectsCache()->allSubjects();

    NX_MUTEX_LOCKER lk(&m_mutex);
    for (const auto& subject: subjects)
    {
        if (!isSubjectEnabled(subject))
            continue;
        const auto globalPermissions = globalPermissionsManager()->globalPermissions(subject);
        auto& accessible = m_accessibleResources[subject.id()];
        for (const auto& resource: resources)
        {
            if (calculateAccess(subject, resource, globalPermissions))
                accessible.insert(resource->getId());
        }
    }
}

bool BaseResourceAccessProvider::acceptable(const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource) const
{
    return resource
        && resource->resourcePool()
        && !resource->hasFlags(Qn::removed)
        && subject.isValid();
}

bool BaseResourceAccessProvider::isSubjectEnabled(const QnResourceAccessSubject& subject) const
{
    if (!subject.isValid())
        return false;

    return subject.user()
        ? subject.user()->isEnabled() && subject.user()->resourcePool()
        : true; //< Roles are always enabled (at least for now)
}

void BaseResourceAccessProvider::updateAccessToResource(const QnResourcePtr& resource)
{
    NX_ASSERT(mode() == Mode::cached);

    if (isUpdating())
        return;

    const auto allSubjects = resourceAccessSubjectsCache()->allSubjects();
    for (const auto& subject: allSubjects)
        updateAccess(subject, resource);
}

void BaseResourceAccessProvider::updateAccessBySubject(const QnResourceAccessSubject& subject)
{
    NX_ASSERT(mode() == Mode::cached);

    if (isUpdating())
        return;

    const auto resources = commonModule()->resourcePool()->getResources();
    for (const auto& resource: resources)
        updateAccess(subject, resource);
}

void BaseResourceAccessProvider::updateAccess(const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource)
{
    NX_ASSERT(mode() == Mode::cached);

    if (isUpdating())
        return;

    // We can get removed auto-generated layout here when switching videowall item control.
    if (!acceptable(subject, resource))
        return;

    bool newValue = false;
    {
        NX_MUTEX_LOCKER lk(&m_mutex);
        auto& accessible = m_accessibleResources[subject.id()];
        auto targetId = resource->getId();

        bool oldValue = accessible.contains(targetId);
        newValue = isSubjectEnabled(subject) && calculateAccess(
            subject, resource, globalPermissionsManager()->globalPermissions(subject));
        if (oldValue == newValue)
            return;

        if (newValue)
            accessible.insert(targetId);
        else
            accessible.remove(targetId);
    }

    emit accessChanged(subject, resource, newValue ? baseSource() : Source::none);

    if (subject.isRole())
    {
        const auto usersInRole = resourceAccessSubjectsCache()->usersInRole(subject.effectiveId());
        for (const auto& dependent: usersInRole)
            updateAccess(dependent, resource);
    }
}

void BaseResourceAccessProvider::fillProviders(
    const QnResourceAccessSubject& /*subject*/,
    const QnResourcePtr& /*resource*/,
    QnResourceList& /*providers*/) const
{
}

void BaseResourceAccessProvider::handleResourceAdded(const QnResourcePtr& resource)
{
    NX_ASSERT(mode() == Mode::cached);

    updateAccessToResource(resource);

    if (QnUserResourcePtr user = resource.dynamicCast<QnUserResource>())
    {
        /* Disabled user should have no access to anything. */
        connect(user, &QnUserResource::enabledChanged, this,
            &BaseResourceAccessProvider::updateAccessBySubject);

        /* Changing of role means change of all user access rights. */
        connect(user, &QnUserResource::userRoleChanged, this,
            &BaseResourceAccessProvider::updateAccessBySubject);

        handleSubjectAdded(user);
    }
}

void BaseResourceAccessProvider::handleResourceRemoved(const QnResourcePtr& resource)
{
    NX_ASSERT(mode() == Mode::cached);

    resource->disconnect(this);

    if (QnUserResourcePtr user = resource.dynamicCast<QnUserResource>())
        handleSubjectRemoved(user);

    if (isUpdating())
        return;

    const auto resourceId = resource->getId();
    const auto allSubjects = resourceAccessSubjectsCache()->allSubjects();
    for (const auto& subject: allSubjects)
    {
        if (subject.id() == resourceId)
            continue;

        {
            NX_MUTEX_LOCKER lk(&m_mutex);
            auto& accessible = m_accessibleResources[subject.id()];
            if (!accessible.contains(resourceId))
                continue;

            accessible.remove(resourceId);
        }
        emit accessChanged(subject, resource, Source::none);
    }
}

void BaseResourceAccessProvider::handleRoleAddedOrUpdated(
    const nx::vms::api::UserRoleData& userRole)
{
    NX_ASSERT(mode() == Mode::cached);

    /* We have no certain way to check if user role was already added. */
    handleSubjectAdded(userRole);
}

void BaseResourceAccessProvider::handleRoleRemoved(const nx::vms::api::UserRoleData& userRole)
{
    NX_ASSERT(mode() == Mode::cached);
    handleSubjectRemoved(userRole);

    if (isUpdating())
        return;

    const auto usersInRole = resourceAccessSubjectsCache()->usersInRole(userRole.id);
    for (const auto& subject: usersInRole)
        updateAccessBySubject(subject);
}

void BaseResourceAccessProvider::handleSubjectAdded(const QnResourceAccessSubject& subject)
{
    NX_ASSERT(mode() == Mode::cached);

    updateAccessBySubject(subject);
}

void BaseResourceAccessProvider::handleSubjectRemoved(const QnResourceAccessSubject& subject)
{
    NX_ASSERT(mode() == Mode::cached);

    if (isUpdating())
        return;

    auto id = subject.id();

    QSet<QnUuid> resourceIds;
    {
        NX_MUTEX_LOCKER lk(&m_mutex);
        NX_ASSERT(m_accessibleResources.contains(id));
        resourceIds = m_accessibleResources.take(id);
    }

    const auto resources = commonModule()->resourcePool()->getResourcesByIds(resourceIds);
    for (const auto& targetResource: resources)
        emit accessChanged(subject, targetResource, Source::none);
}

QSet<QnUuid> BaseResourceAccessProvider::accessible(const QnResourceAccessSubject& subject) const
{
    NX_ASSERT(mode() == Mode::cached);

    NX_MUTEX_LOCKER lk(&m_mutex);
    return m_accessibleResources.value(subject.id());
}

bool BaseResourceAccessProvider::isLayout(const QnResourcePtr& resource) const
{
    return QnResourceAccessFilter::isShareableLayout(resource);
}

bool BaseResourceAccessProvider::isMediaResource(const QnResourcePtr& resource) const
{
    return QnResourceAccessFilter::isShareableMedia(resource);
}

} // namespace nx::core::access
