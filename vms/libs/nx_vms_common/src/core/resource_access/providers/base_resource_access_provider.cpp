// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "base_resource_access_provider.h"

#include <core/resource/user_resource.h>
#include <core/resource_access/global_permissions_manager.h>
#include <core/resource_access/resource_access_filter.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/resource_access_subjects_cache.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>
#include <nx/utils/log/log_main.h>
#include <nx/vms/api/data/user_role_data.h>
#include <nx/vms/common/system_context.h>

namespace nx::core::access {

BaseResourceAccessProvider::BaseResourceAccessProvider(
    Mode mode,
    nx::vms::common::SystemContext* context,
    QObject* parent)
    :
    base_type(mode, parent),
    nx::vms::common::SystemContextAware(context),
    m_mutex(nx::Mutex::Recursive)
{
    if (mode == Mode::cached)
    {
        connect(resourcePool(), &QnResourcePool::resourceAdded, this,
            &BaseResourceAccessProvider::handleResourceAdded);
        connect(resourcePool(), &QnResourcePool::resourceRemoved, this,
            &BaseResourceAccessProvider::handleResourceRemoved);

        connect(m_context->userRolesManager(), &QnUserRolesManager::userRoleAddedOrUpdated, this,
            &BaseResourceAccessProvider::handleRoleAddedOrUpdated);
        connect(m_context->userRolesManager(), &QnUserRolesManager::userRoleRemoved, this,
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
    {
        NX_VERBOSE(this, "%1 -> %2 is not acceptable", subject, resource);
        return false;
    }

    if (mode() == Mode::direct)
    {
        if (!isSubjectEnabled(subject))
        {
            NX_VERBOSE(this, "%1 is disabled", subject);
            return false;
        }

        // Precalculate list of all parent roles for the optimization purposes.
        const auto effectiveIds = m_context->resourceAccessSubjectsCache()->subjectWithParents(
            subject);
        const auto result = calculateAccess(
            subject,
            resource,
            m_context->globalPermissionsManager()->globalPermissions(subject, &effectiveIds),
            effectiveIds);
        NX_VERBOSE(this, "%1 -> %2 is %3", subject, resource, result ? "accessible" : "inaccessible");
        return result;
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
    const auto result = (iterator == m_accessibleResources.end())
        ? false
        : iterator->contains(resource->getId());
    NX_TRACE(this, "%1 -> %2 is %3 from cache",
        subject, resource, result ? "accessible" : "inaccessible");

    return result;
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

    const auto resources = resourcePool()->getResources();
    const auto subjects = m_context->resourceAccessSubjectsCache()->allSubjects();

    NX_MUTEX_LOCKER lk(&m_mutex);
    for (const auto& subject: subjects)
    {
        if (!isSubjectEnabled(subject))
        {
            NX_VERBOSE(this, "%1 is disabled after update", subject);
            continue;
        }

        // Precalculate list of all parent roles for the optimization purposes.
        const std::vector<QnUuid> effectiveIds =
            m_context->resourceAccessSubjectsCache()->subjectWithParents(subject);

        const auto globalPermissions =
            m_context->globalPermissionsManager()->globalPermissions(subject, &effectiveIds);
        auto& accessible = m_accessibleResources[subject.id()];
        for (const auto& resource: resources)
        {
            if (calculateAccess(subject, resource, globalPermissions, effectiveIds))
                accessible.insert(resource->getId());
        }
        NX_VERBOSE(this, "%1 has acces to %2 after update", subject, nx::containerString(accessible));
    }
}

bool BaseResourceAccessProvider::acceptable(const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource) const
{
    return resource
        && resource->systemContext()
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

    const auto allSubjects = m_context->resourceAccessSubjectsCache()->allSubjects();
    NX_VERBOSE(this, "Updating access to %1 by %2", resource, nx::containerString(allSubjects));
    for (const auto& subject: allSubjects)
        updateAccess(subject, resource);
}

void BaseResourceAccessProvider::updateAccessBySubject(const QnResourceAccessSubject& subject)
{
    NX_ASSERT(mode() == Mode::cached);

    if (isUpdating())
        return;

    const auto resources = resourcePool()->getResources();
    NX_VERBOSE(this, "Updating access by %1 to %2", subject, nx::containerString(resources));
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

        // Precalculate list of all parent roles for the optimization purposes.
        const auto effectiveIds = m_context->resourceAccessSubjectsCache()->subjectWithParents(
            subject);
        newValue = isSubjectEnabled(subject)
            && calculateAccess(
                subject,
                resource,
                m_context->globalPermissionsManager()->globalPermissions(subject, &effectiveIds),
                effectiveIds);

        if (oldValue == newValue)
            return;

        if (newValue)
            accessible.insert(targetId);
        else
            accessible.remove(targetId);
    }

    NX_VERBOSE(this, "%1 access for %2 to %3", resource, subject, newValue);
    emit accessChanged(subject, resource, newValue ? baseSource() : Source::none);

    if (subject.isRole())
    {
        for (const auto& dependent: m_context->resourceAccessSubjectsCache()->allSubjectsInRole(subject.id()))
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
        connect(user.get(), &QnUserResource::enabledChanged, this,
            &BaseResourceAccessProvider::updateAccessBySubject);

        /* Changing of role means change of all user access rights. */
        connect(user.get(), &QnUserResource::userRolesChanged, this,
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
    const auto allSubjects = m_context->resourceAccessSubjectsCache()->allSubjects();
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

    for (const auto& subject: m_context->resourceAccessSubjectsCache()->allSubjectsInRole(userRole.id))
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
        resourceIds = m_accessibleResources.take(id);
    }

    const auto resources = resourcePool()->getResourcesByIds(resourceIds);
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
