#include "base_resource_access_provider.h"

#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/resource_access_filter.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>

#include <core/resource/user_resource.h>

QnBaseResourceAccessProvider::QnBaseResourceAccessProvider(QObject* parent):
    base_type(parent),
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
    if (!acceptable(subject, resource))
        return false;

    /* We can get cache miss in the following scenario:
     * * new user was added
     * * global permissions manager emits 'changed'
     * * access manager recalculates all permissions
     * * access provider checks 'has access' to this user itself
     */
    if (!m_accessibleResources.contains(subject.id()))
        return false;

    return m_accessibleResources[subject.id()].contains(resource->getId());
}

QnAbstractResourceAccessProvider::Source QnBaseResourceAccessProvider::accessibleVia(
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
    for (const auto& subject : allSubjects())
        updateAccess(subject, resource);
}

void QnBaseResourceAccessProvider::updateAccessBySubject(const QnResourceAccessSubject& subject)
{
    for (const auto& resource : qnResPool->getResources())
        updateAccess(subject, resource);
}

void QnBaseResourceAccessProvider::updateAccess(const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource)
{
    /* We can get removed auto-generated layout here when switching videowall item control. */
    if (!acceptable(subject, resource))
        return;

    auto& accessible = m_accessibleResources[subject.id()];
    auto targetId = resource->getId();

    bool oldValue = accessible.contains(targetId);
    bool newValue = isSubjectEnabled(subject) && calculateAccess(subject, resource);
    if (oldValue == newValue)
        return;

    if (newValue)
        accessible.insert(targetId);
    else
        accessible.remove(targetId);

    emit accessChanged(subject, resource, newValue ? baseSource() : Source::none);

    for (const auto& dependent: dependentSubjects(subject))
        updateAccess(dependent, resource);
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
        connect(user, &QnUserResource::userGroupChanged, this,
            &QnBaseResourceAccessProvider::updateAccessBySubject);

        updateAccessBySubject(user);
    }
}

void QnBaseResourceAccessProvider::handleResourceRemoved(const QnResourcePtr& resource)
{
    disconnect(resource, nullptr, this, nullptr);

    auto resourceId = resource->getId();

    if (QnUserResourcePtr user = resource.dynamicCast<QnUserResource>())
        handleSubjectRemoved(user);

    for (const auto& subject : allSubjects())
    {
        if (subject.id() == resourceId)
            continue;

        auto& accessible = m_accessibleResources[subject.id()];
        if (!accessible.contains(resourceId))
            continue;

        accessible.remove(resourceId);
        emit accessChanged(subject, resource, Source::none);
    }
}

void QnBaseResourceAccessProvider::handleRoleAddedOrUpdated(
    const ec2::ApiUserGroupData& userRole)
{
    updateAccessBySubject(userRole);
}

void QnBaseResourceAccessProvider::handleRoleRemoved(const ec2::ApiUserGroupData& userRole)
{
    handleSubjectRemoved(userRole);
    for (auto subject : QnAbstractResourceAccessProvider::dependentSubjects(userRole))
        updateAccessBySubject(subject);
}

void QnBaseResourceAccessProvider::handleSubjectRemoved(const QnResourceAccessSubject& subject)
{
    auto id = subject.id();

    NX_ASSERT(m_accessibleResources.contains(id));

    auto resources = qnResPool->getResources(m_accessibleResources[id].values());
    m_accessibleResources.remove(id);
    for (const auto& targetResource : resources)
        emit accessChanged(subject, targetResource, Source::none);
}

QSet<QnUuid> QnBaseResourceAccessProvider::accessible(const QnResourceAccessSubject& subject) const
{
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
