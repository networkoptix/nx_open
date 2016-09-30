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
    connect(qnResPool, &QnResourcePool::resourceRemoved, this,
        &QnSharedResourcesManager::handleResourceRemoved);

    connect(qnUserRolesManager, &QnUserRolesManager::userRoleRemoved, this,
        &QnSharedResourcesManager::handleRoleRemoved);
}

QnSharedResourcesManager::~QnSharedResourcesManager()
{
}

void QnSharedResourcesManager::reset(const ec2::ApiAccessRightsDataList& accessibleResourcesList)
{
    QnMutexLocker lk(&m_mutex);
    m_sharedResources.clear();
    for (const auto& item : accessibleResourcesList)
    {
        QSet<QnUuid>& resources = m_sharedResources[item.userId];
        for (const auto& id : item.resourceIds)
            resources << id;
    }
    //TODO: #GDM possibly we should send correct signals, shouldn't we?
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

void QnSharedResourcesManager::handleResourceRemoved(const QnResourcePtr& resource)
{
    if (QnUserResourcePtr user = resource.dynamicCast<QnUserResource>())
        handleSubjectRemoved(user);
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

