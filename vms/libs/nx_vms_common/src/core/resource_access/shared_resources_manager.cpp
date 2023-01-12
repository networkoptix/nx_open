// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "shared_resources_manager.h"

#include <core/resource/user_resource.h>
#include <core/resource_access/resource_access_subjects_cache.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>
#include <nx/vms/api/data/access_rights_data.h>
#include <nx/vms/common/system_context.h>

namespace {

using AccessRightsMap = std::map<QnUuid, nx::vms::api::AccessRights>;

static const AccessRightsMap kEmpty;

AccessRightsMap combine(AccessRightsMap&& left, AccessRightsMap&& right)
{
    auto result(left);
    result.insert(std::move_iterator(right.begin()), std::move_iterator(right.end()));
    return result;
}

} // namespace

using namespace nx;

QnSharedResourcesManager::QnSharedResourcesManager(
    nx::vms::common::SystemContext* context,
    QObject* parent)
    :
    base_type(parent),
    nx::vms::common::SystemContextAware(context),
    m_mutex(nx::Mutex::NonRecursive),
    m_sharedResources()
{
    connect(resourcePool(), &QnResourcePool::resourceAdded, this,
        &QnSharedResourcesManager::handleResourceAdded, Qt::DirectConnection);
    connect(resourcePool(), &QnResourcePool::resourceRemoved, this,
        &QnSharedResourcesManager::handleResourceRemoved, Qt::DirectConnection);

    connect(m_context->userRolesManager(), &QnUserRolesManager::userRoleAddedOrUpdated, this,
        &QnSharedResourcesManager::handleRoleAddedOrUpdated, Qt::DirectConnection);
    connect(m_context->userRolesManager(), &QnUserRolesManager::userRoleRemoved, this,
        &QnSharedResourcesManager::handleRoleRemoved, Qt::DirectConnection);
}

QnSharedResourcesManager::~QnSharedResourcesManager()
{
}

void QnSharedResourcesManager::reset(const vms::api::AccessRightsDataList& accessibleResourcesList)
{
    decltype(m_sharedResources) oldValuesMap;
    {
        NX_MUTEX_LOCKER lk(&m_mutex);
        oldValuesMap = m_sharedResources;
        m_sharedResources.clear();
        for (const auto& item: accessibleResourcesList)
        {
            m_sharedResources[item.userId].insert(
                item.resourceRights.begin(), item.resourceRights.end());
        }
    }

    for (const auto& subject: m_context->resourceAccessSubjectsCache()->allSubjects())
    {
        auto oldValues = oldValuesMap.value(subject.id());
        auto newValues = sharedResourceRights(subject);
        if (oldValues != newValues)
            emit sharedResourcesChanged(subject, oldValues, newValues);
    }
}

AccessRightsMap QnSharedResourcesManager::sharedResourceRights(
    const QnResourceAccessSubject& subject) const
{
    return combine(directAccessRights(subject), inheritedAccessRights(subject));
}

AccessRightsMap QnSharedResourcesManager::directAccessRights(
    const QnResourceAccessSubject& subject) const
{
    NX_ASSERT(subject.isValid());
    if (!subject.isValid())
        return {};

    NX_MUTEX_LOCKER lk(&m_mutex);
    return m_sharedResources[subject.id()];
}

AccessRightsMap QnSharedResourcesManager::inheritedAccessRights(
    const QnResourceAccessSubject& subject) const
{
    NX_ASSERT(subject.isValid());
    if (!subject.isValid())
        return {};

    AccessRightsMap result;

    NX_MUTEX_LOCKER lk(&m_mutex);
    for (const auto& id: m_context->resourceAccessSubjectsCache()->subjectWithParents(subject))
    {
        if (id == subject.id())
            continue;

        const auto& resources = m_sharedResources[id];
        result.insert(resources.begin(), resources.end());
    }
    return result;
}

QSet<QnUuid> QnSharedResourcesManager::sharedResources(
    const QnResourceAccessSubject& subject) const
{
    QSet<QnUuid> result;
    for (const auto& [id, _]: sharedResourceRights(subject))
        result.insert(id);
    return result;
}

bool QnSharedResourcesManager::hasSharedResource(
    const QnResourceAccessSubject& subject, const QnUuid& resourceId) const
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    for (const auto& effectiveId:
        m_context->resourceAccessSubjectsCache()->subjectWithParents(subject))
    {
        if (m_sharedResources[effectiveId].contains(resourceId))
            return true;
    }
    return false;
}

void QnSharedResourcesManager::setSharedResourceRights(
    const QnResourceAccessSubject& subject,
    const AccessRightsMap& resourceRights)
{
    setSharedResourceRightsInternal(subject, resourceRights);
}

void QnSharedResourcesManager::setSharedResourceRights(
    const nx::vms::api::AccessRightsData& accessRights)
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    m_sharedResources.insert(accessRights.userId, accessRights.resourceRights);
}

void QnSharedResourcesManager::setSharedResources(
    const QnResourceAccessSubject& subject,
    const QSet<QnUuid>& resources,
    nx::vms::api::AccessRights accessRights)
{
    AccessRightsMap resourceAccessRights;
    for (const auto& r: resources)
        resourceAccessRights[r] = accessRights;
    setSharedResourceRights(subject, resourceAccessRights);
}

void QnSharedResourcesManager::setSharedResourceRightsInternal(
    const QnResourceAccessSubject& subject,
    const AccessRightsMap& resourceRights)
{
    NX_ASSERT(subject.isValid());
    if (!subject.isValid())
        return;

    NX_MUTEX_LOCKER lk(&m_mutex);
    auto& value = m_sharedResources[subject.id()];
    if (value == resourceRights)
        return;

    AccessRightsMap oldOwnRights = value;
    value = resourceRights;

    lk.unlock();

    emit sharedResourcesChanged(subject,
        combine(std::move(oldOwnRights), inheritedAccessRights(subject)),
        sharedResourceRights(subject));
}

void QnSharedResourcesManager::handleResourceAdded(const QnResourcePtr& resource)
{
    if (auto user = resource.dynamicCast<QnUserResource>())
    {
        auto resources = sharedResourceRights(user);
        if (!resources.empty())
            emit sharedResourcesChanged(user, kEmpty, resources);
    }
}

void QnSharedResourcesManager::handleResourceRemoved(const QnResourcePtr& resource)
{
    if (QnUserResourcePtr user = resource.dynamicCast<QnUserResource>())
        handleSubjectRemoved(user);
}

void QnSharedResourcesManager::handleRoleAddedOrUpdated(const nx::vms::api::UserRoleData& userRole)
{
    auto resources = sharedResourceRights(userRole);
    if (!resources.empty())
        emit sharedResourcesChanged(userRole, kEmpty, resources);
}

void QnSharedResourcesManager::handleRoleRemoved(const nx::vms::api::UserRoleData& userRole)
{
    handleSubjectRemoved(userRole);
    for (auto subject: m_context->resourceAccessSubjectsCache()->allSubjectsInRole(userRole.id))
        setSharedResourceRightsInternal(subject, kEmpty);
}

void QnSharedResourcesManager::handleSubjectRemoved(const QnResourceAccessSubject& subject)
{
    const auto oldValue = sharedResourceRights(subject);

    NX_MUTEX_LOCKER lk(&m_mutex);
    m_sharedResources.remove(subject.id());
    lk.unlock();

    if (!oldValue.empty())
        emit sharedResourcesChanged(subject, oldValue, kEmpty);
}
