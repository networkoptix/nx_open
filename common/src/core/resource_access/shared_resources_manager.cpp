#include "shared_resources_manager.h"

#include <nx_ec/data/api_access_rights_data.h>

#include <core/resource_access/providers/abstract_resource_access_provider.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>

#include <core/resource/user_resource.h>

QnSharedResourcesManager::QnSharedResourcesManager(QObject* parent):
    base_type(parent),
    m_mutex(QnMutex::NonRecursive),
    m_sharedResources()
{
    connect(qnResPool, &QnResourcePool::resourceAdded, this,
        &QnSharedResourcesManager::handleResourceAdded);
    connect(qnResPool, &QnResourcePool::resourceRemoved, this,
        &QnSharedResourcesManager::handleResourceRemoved);

    connect(qnUserRolesManager, &QnUserRolesManager::userRoleAddedOrUpdated, this,
        &QnSharedResourcesManager::handleRoleAddedOrUpdated);
    connect(qnUserRolesManager, &QnUserRolesManager::userRoleRemoved, this,
        &QnSharedResourcesManager::handleRoleRemoved);
}

QnSharedResourcesManager::~QnSharedResourcesManager()
{
}

void QnSharedResourcesManager::reset(const ec2::ApiAccessRightsDataList& accessibleResourcesList)
{
    QHash<QnUuid, QSet<QnUuid> > oldValues;
    {
        QnMutexLocker lk(&m_mutex);
        oldValues = m_sharedResources;
        m_sharedResources.clear();
        for (const auto& item : accessibleResourcesList)
        {
            auto& resources = m_sharedResources[item.userId];
            for (const auto& id : item.resourceIds)
                resources << id;
        }
    }

    for (const auto& subject : QnAbstractResourceAccessProvider::allSubjects())
    {
        auto newValues = sharedResources(subject);
        if (oldValues[subject.id()] != newValues)
            emit sharedResourcesChanged(subject, newValues);
    }
}

QSet<QnUuid> QnSharedResourcesManager::sharedResources(
    const QnResourceAccessSubject& subject) const
{
    NX_ASSERT(subject.isValid());
    if (!subject.isValid())
        return QSet<QnUuid>();

    QnMutexLocker lk(&m_mutex);
    return m_sharedResources[subject.effectiveId()];
}

void QnSharedResourcesManager::setSharedResources(const QnResourceAccessSubject& subject,
    const QSet<QnUuid>& resources)
{
    NX_ASSERT(subject.isValid());
    if (!subject.isValid())
        return;

    NX_ASSERT(subject.effectiveId() == subject.id() || resources.empty(),
        "Security check. We must not set custom accessible resources to user in custom role."
    );
    setSharedResourcesInternal(subject, resources);
}

void QnSharedResourcesManager::setSharedResourcesInternal(const QnResourceAccessSubject& subject,
    const QSet<QnUuid>& resources)
{
    {
        QnMutexLocker lk(&m_mutex);
        auto& value = m_sharedResources[subject.id()];
        if (value == resources)
            return;
        value = resources;
    }
    emit sharedResourcesChanged(subject, resources);
}

void QnSharedResourcesManager::handleResourceAdded(const QnResourcePtr& resource)
{
    if (auto user = resource.dynamicCast<QnUserResource>())
    {
        auto resources = sharedResources(user);
        if (!resources.isEmpty())
            emit sharedResourcesChanged(user, resources);
    }
}

void QnSharedResourcesManager::handleResourceRemoved(const QnResourcePtr& resource)
{
    if (QnUserResourcePtr user = resource.dynamicCast<QnUserResource>())
        handleSubjectRemoved(user);
}

void QnSharedResourcesManager::handleRoleAddedOrUpdated(const ec2::ApiUserGroupData& userRole)
{
    auto resources = sharedResources(userRole);
    if (!resources.isEmpty())
        emit sharedResourcesChanged(userRole, resources);
}

void QnSharedResourcesManager::handleRoleRemoved(const ec2::ApiUserGroupData& userRole)
{
    handleSubjectRemoved(userRole);
    for (auto subject : QnAbstractResourceAccessProvider::dependentSubjects(userRole))
        setSharedResourcesInternal(subject, QSet<QnUuid>());
}

void QnSharedResourcesManager::handleSubjectRemoved(const QnResourceAccessSubject& subject)
{
    auto id = subject.id();
    {
        QnMutexLocker lk(&m_mutex);
        if (!m_sharedResources.contains(id))
            return;
        m_sharedResources.remove(id);
    }
    emit sharedResourcesChanged(subject, QSet<QnUuid>());
}

