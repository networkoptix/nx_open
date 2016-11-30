#include "base_resource_access_provider.h"

#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/resource_access_filter.h>
#include <core/resource_access/resource_access_subjects_cache.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>

#include <core/resource/user_resource.h>

QnBaseResourceAccessProvider::QnBaseResourceAccessProvider(QObject* parent):
    base_type(parent),
    m_mutex(QnMutex::NonRecursive),
    m_accessibleResources()
{
    connect(qnResPool, &QnResourcePool::resourceAdded, this,
        &QnBaseResourceAccessProvider::handleResourceAdded);
    connect(qnResPool, &QnResourcePool::resourceRemoved, this,
        &QnBaseResourceAccessProvider::handleResourceRemoved);

    connect(qnUserRolesManager, &QnUserRolesManager::userRoleAddedOrUpdated, this,
        &QnBaseResourceAccessProvider::handleRoleAddedOrUpdated);
    connect(qnUserRolesManager, &QnUserRolesManager::userRoleRemoved, this,
        &QnBaseResourceAccessProvider::handleRoleRemoved);
}

QnBaseResourceAccessProvider::~QnBaseResourceAccessProvider()
{
}

bool QnBaseResourceAccessProvider::hasAccess(const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource) const
{
    NX_EXPECT(!isUpdating());

    if (!acceptable(subject, resource))
        return false;

    /* We can get cache miss in the following scenario:
     * * new user was added
     * * global permissions manager emits 'changed'
     * * access manager recalculates all permissions
     * * access provider checks 'has access' to this user itself
     */
    QnMutexLocker lk(&m_mutex);
    if (!m_accessibleResources.contains(subject.id()))
        return false;

    return m_accessibleResources[subject.id()].contains(resource->getId());
}

QnAbstractResourceAccessProvider::Source QnBaseResourceAccessProvider::accessibleVia(
    const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource,
    QnResourceList* providers) const
{
    NX_EXPECT(!isUpdating());

    if (!hasAccess(subject, resource))
        return Source::none;

    if (providers)
        fillProviders(subject, resource, *providers);

    return baseSource();
}

void QnBaseResourceAccessProvider::beforeUpdate()
{
    QnMutexLocker lk(&m_mutex);
    m_accessibleResources.clear();
}

void QnBaseResourceAccessProvider::afterUpdate()
{
    for (const auto& subject : qnResourceAccessSubjectsCache->allSubjects())
        updateAccessBySubject(subject);
}

bool QnBaseResourceAccessProvider::acceptable(const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource) const
{
    return resource && resource->resourcePool() && subject.isValid();
}

bool QnBaseResourceAccessProvider::isSubjectEnabled(const QnResourceAccessSubject& subject) const
{
    if (!subject.isValid())
        return false;

    return subject.user()
        ? subject.user()->isEnabled()
        : true; //< Roles are always enabled (at least for now)
}

void QnBaseResourceAccessProvider::updateAccessToResource(const QnResourcePtr& resource)
{
    if (isUpdating())
        return;

    for (const auto& subject : qnResourceAccessSubjectsCache->allSubjects())
        updateAccess(subject, resource);
}

void QnBaseResourceAccessProvider::updateAccessBySubject(const QnResourceAccessSubject& subject)
{
    if (isUpdating())
        return;

    for (const auto& resource : qnResPool->getResources())
        updateAccess(subject, resource);
}

void QnBaseResourceAccessProvider::updateAccess(const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource)
{
    if (isUpdating())
        return;

    /* We can get removed auto-generated layout here when switching videowall item control. */
    if (!acceptable(subject, resource))
        return;

    bool newValue = false;
    {
        QnMutexLocker lk(&m_mutex);
        auto& accessible = m_accessibleResources[subject.id()];
        auto targetId = resource->getId();

        bool oldValue = accessible.contains(targetId);
        newValue = isSubjectEnabled(subject) && calculateAccess(subject, resource);
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
        for (const auto& dependent: qnResourceAccessSubjectsCache->usersInRole(subject.role().id))
            updateAccess(dependent, resource);
    }
}

void QnBaseResourceAccessProvider::fillProviders(
    const QnResourceAccessSubject& /*subject*/,
    const QnResourcePtr& /*resource*/,
    QnResourceList& /*providers*/) const
{
}

void QnBaseResourceAccessProvider::handleResourceAdded(const QnResourcePtr& resource)
{
    updateAccessToResource(resource);

    if (QnUserResourcePtr user = resource.dynamicCast<QnUserResource>())
    {
        /* Disabled user should have no access to anything. */
        connect(user, &QnUserResource::enabledChanged, this,
            &QnBaseResourceAccessProvider::updateAccessBySubject);

        /* Changing of role means change of all user access rights. */
        connect(user, &QnUserResource::userRoleChanged, this,
            &QnBaseResourceAccessProvider::updateAccessBySubject);

        handleSubjectAdded(user);
    }
}

void QnBaseResourceAccessProvider::handleResourceRemoved(const QnResourcePtr& resource)
{
    disconnect(resource, nullptr, this, nullptr);

    if (isUpdating())
        return;

    auto resourceId = resource->getId();

    if (QnUserResourcePtr user = resource.dynamicCast<QnUserResource>())
        handleSubjectRemoved(user);

    for (const auto& subject : qnResourceAccessSubjectsCache->allSubjects())
    {
        if (subject.id() == resourceId)
            continue;

        {
            QnMutexLocker lk(&m_mutex);
            auto& accessible = m_accessibleResources[subject.id()];
            if (!accessible.contains(resourceId))
                continue;

            accessible.remove(resourceId);
        }
        emit accessChanged(subject, resource, Source::none);
    }
}

void QnBaseResourceAccessProvider::handleRoleAddedOrUpdated(
    const ec2::ApiUserRoleData& userRole)
{
    /* We have no certain way to check if user role was already added. */
    handleSubjectAdded(userRole);
}

void QnBaseResourceAccessProvider::handleRoleRemoved(const ec2::ApiUserRoleData& userRole)
{
    if (isUpdating())
        return;

    handleSubjectRemoved(userRole);
    for (auto subject : qnResourceAccessSubjectsCache->usersInRole(userRole.id))
        updateAccessBySubject(subject);
}

void QnBaseResourceAccessProvider::handleSubjectAdded(const QnResourceAccessSubject& subject)
{
    updateAccessBySubject(subject);
}

void QnBaseResourceAccessProvider::handleSubjectRemoved(const QnResourceAccessSubject& subject)
{
    if (isUpdating())
        return;

    auto id = subject.id();

    QSet<QnUuid> resourceIds;
    {
        QnMutexLocker lk(&m_mutex);
        NX_ASSERT(m_accessibleResources.contains(id));
        resourceIds = m_accessibleResources.take(id);
    }

    const auto resources = qnResPool->getResources(resourceIds);
    for (const auto& targetResource : resources)
        emit accessChanged(subject, targetResource, Source::none);
}

QSet<QnUuid> QnBaseResourceAccessProvider::accessible(const QnResourceAccessSubject& subject) const
{
    QnMutexLocker lk(&m_mutex);
    return m_accessibleResources.value(subject.id());
}

bool QnBaseResourceAccessProvider::isLayout(const QnResourcePtr& resource) const
{
    return QnResourceAccessFilter::isShareableLayout(resource);
}

bool QnBaseResourceAccessProvider::isMediaResource(const QnResourcePtr& resource) const
{
    return QnResourceAccessFilter::isShareableMedia(resource);
}
