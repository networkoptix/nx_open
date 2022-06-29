// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "shared_resources_manager.h"

#include <core/resource/user_resource.h>
#include <core/resource_access/resource_access_subjects_cache.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>
#include <nx/vms/api/data/access_rights_data.h>
#include <nx/vms/common/system_context.h>

namespace {

static const QSet<QnUuid> kEmpty;

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
        &QnSharedResourcesManager::handleResourceAdded);
    connect(resourcePool(), &QnResourcePool::resourceRemoved, this,
        &QnSharedResourcesManager::handleResourceRemoved);

    connect(m_context->userRolesManager(), &QnUserRolesManager::userRoleAddedOrUpdated, this,
        &QnSharedResourcesManager::handleRoleAddedOrUpdated);
    connect(m_context->userRolesManager(), &QnUserRolesManager::userRoleRemoved, this,
        &QnSharedResourcesManager::handleRoleRemoved);
}

QnSharedResourcesManager::~QnSharedResourcesManager()
{
}

void QnSharedResourcesManager::reset(const vms::api::AccessRightsDataList& accessibleResourcesList)
{
    QHash<QnUuid, QSet<QnUuid> > oldValuesMap;
    {
        NX_MUTEX_LOCKER lk(&m_mutex);
        oldValuesMap = m_sharedResources;
        m_sharedResources.clear();
        for (const auto& item: accessibleResourcesList)
        {
            auto& resources = m_sharedResources[item.userId];
            for (const auto& id: item.resourceIds)
                resources << id;
        }
    }

    for (const auto& subject: m_context->resourceAccessSubjectsCache()->allSubjects())
    {
        auto oldValues = oldValuesMap.value(subject.id());
        auto newValues = sharedResources(subject);
        if (oldValues != newValues)
            emit sharedResourcesChanged(subject, oldValues, newValues);
    }
}

QSet<QnUuid> QnSharedResourcesManager::sharedResources(
    const QnResourceAccessSubject& subject) const
{
    NX_ASSERT(subject.isValid());
    if (!subject.isValid())
        return QSet<QnUuid>();

    NX_MUTEX_LOCKER lk(&m_mutex);
    QSet<QnUuid> result;
    for (const auto& id: m_context->resourceAccessSubjectsCache()->subjectWithParents(subject))
        result.unite(m_sharedResources[id]);
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

void QnSharedResourcesManager::setSharedResources(const QnResourceAccessSubject& subject,
    const QSet<QnUuid>& resources)
{
    NX_ASSERT(subject.isValid());
    if (!subject.isValid())
        return;

    setSharedResourcesInternal(subject, resources);
}

void QnSharedResourcesManager::setSharedResourcesById(const QnUuid& subjectId,
    const QSet<QnUuid>& resources)
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    m_sharedResources.insert(subjectId, resources);
}

void QnSharedResourcesManager::setSharedResourcesInternal(const QnResourceAccessSubject& subject,
    const QSet<QnUuid>& resources)
{
    QSet<QnUuid> oldValue;
    {
        NX_MUTEX_LOCKER lk(&m_mutex);
        auto& value = m_sharedResources[subject.id()];
        if (value == resources)
            return;
        oldValue = value;
        value = resources;
    }
    emit sharedResourcesChanged(subject, oldValue, resources);
}

void QnSharedResourcesManager::handleResourceAdded(const QnResourcePtr& resource)
{
    if (auto user = resource.dynamicCast<QnUserResource>())
    {
        auto resources = sharedResources(user);
        if (!resources.isEmpty())
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
    auto resources = sharedResources(userRole);
    if (!resources.isEmpty())
        emit sharedResourcesChanged(userRole, kEmpty, resources);
}

void QnSharedResourcesManager::handleRoleRemoved(const nx::vms::api::UserRoleData& userRole)
{
    handleSubjectRemoved(userRole);
    for (auto subject: m_context->resourceAccessSubjectsCache()->allSubjectsInRole(userRole.id))
        setSharedResourcesInternal(subject, kEmpty);
}

void QnSharedResourcesManager::handleSubjectRemoved(const QnResourceAccessSubject& subject)
{
    QSet<QnUuid> oldValue;
    auto id = subject.id();
    {
        NX_MUTEX_LOCKER lk(&m_mutex);
        if (!m_sharedResources.contains(id))
            return;
        oldValue = m_sharedResources.value(id);
        m_sharedResources.remove(id);
    }

    if (!oldValue.isEmpty())
        emit sharedResourcesChanged(subject, oldValue, kEmpty);
}
