#include "shared_resources_manager.h"

#include <nx_ec/data/api_access_rights_data.h>

#include <core/resource_access/resource_access_subjects_cache.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>

#include <core/resource/user_resource.h>

namespace {

static const QSet<QnUuid> kEmpty;

}

QnSharedResourcesManager::QnSharedResourcesManager(QObject* parent):
    base_type(parent),
    QnCommonModuleAware(parent),
    m_mutex(QnMutex::NonRecursive),
    m_sharedResources()
{
    connect(resourcePool(), &QnResourcePool::resourceAdded, this,
        &QnSharedResourcesManager::handleResourceAdded);
    connect(resourcePool(), &QnResourcePool::resourceRemoved, this,
        &QnSharedResourcesManager::handleResourceRemoved);

    connect(userRolesManager(), &QnUserRolesManager::userRoleAddedOrUpdated, this,
        &QnSharedResourcesManager::handleRoleAddedOrUpdated);
    connect(userRolesManager(), &QnUserRolesManager::userRoleRemoved, this,
        &QnSharedResourcesManager::handleRoleRemoved);
}

QnSharedResourcesManager::~QnSharedResourcesManager()
{
}

void QnSharedResourcesManager::reset(const ec2::ApiAccessRightsDataList& accessibleResourcesList)
{
    QHash<QnUuid, QSet<QnUuid> > oldValuesMap;
    {
        QnMutexLocker lk(&m_mutex);
        oldValuesMap = m_sharedResources;
        m_sharedResources.clear();
        for (const auto& item: accessibleResourcesList)
        {
            auto& resources = m_sharedResources[item.userId];
            for (const auto& id: item.resourceIds)
                resources << id;
        }
    }

    for (const auto& subject: resourceAccessSubjectsCache()->allSubjects())
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
    QSet<QnUuid> oldValue;
    {
        QnMutexLocker lk(&m_mutex);
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

void QnSharedResourcesManager::handleRoleAddedOrUpdated(const ec2::ApiUserRoleData& userRole)
{
    auto resources = sharedResources(userRole);
    if (!resources.isEmpty())
        emit sharedResourcesChanged(userRole, kEmpty, resources);
}

void QnSharedResourcesManager::handleRoleRemoved(const ec2::ApiUserRoleData& userRole)
{
    handleSubjectRemoved(userRole);
    for (auto subject: resourceAccessSubjectsCache()->usersInRole(userRole.id))
        setSharedResourcesInternal(subject, kEmpty);
}

void QnSharedResourcesManager::handleSubjectRemoved(const QnResourceAccessSubject& subject)
{
    QSet<QnUuid> oldValue;
    auto id = subject.id();
    {
        QnMutexLocker lk(&m_mutex);
        if (!m_sharedResources.contains(id))
            return;
        oldValue = m_sharedResources.value(id);
        m_sharedResources.remove(id);
    }

    if (!oldValue.isEmpty())
        emit sharedResourcesChanged(subject, oldValue, kEmpty);
}

