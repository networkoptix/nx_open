#include "user_roles_manager.h"

QnUserRolesManager::QnUserRolesManager(QObject* parent):
    base_type(parent)
{
}

QnUserRolesManager::~QnUserRolesManager()
{
}

ec2::ApiUserGroupDataList QnUserRolesManager::userRoles() const
{
    QnMutexLocker lk(&m_mutex);
    ec2::ApiUserGroupDataList result;
    result.reserve(m_roles.size());
    for (const auto& group : m_roles)
        result.push_back(group);
    return result;
}

void QnUserRolesManager::resetUserRoles(const ec2::ApiUserGroupDataList& userRoles)
{
    QnMutexLocker lk(&m_mutex);
    m_roles.clear();
    for (const auto& role : userRoles)
        m_roles.insert(role.id, role);
}


bool QnUserRolesManager::hasRole(const QnUuid& id) const
{
    QnMutexLocker lk(&m_mutex);
    return m_roles.contains(id);
}

ec2::ApiUserGroupData QnUserRolesManager::userRole(const QnUuid& id) const
{
    QnMutexLocker lk(&m_mutex);
    return m_roles.value(id);
}

void QnUserRolesManager::addOrUpdateUserRole(const ec2::ApiUserGroupData& role)
{
    NX_ASSERT(!role.isNull());
    {
        QnMutexLocker lk(&m_mutex);
        if (m_roles.value(role.id) == role)
            return;
        m_roles[role.id] = role;
    }

    emit userRoleAddedOrUpdated(role);
}

void QnUserRolesManager::removeUserRole(const QnUuid& id)
{
    NX_ASSERT(!id.isNull());
    {
        QnMutexLocker lk(&m_mutex);
        NX_ASSERT(m_roles.contains(id));
        if (!m_roles.contains(id))
            return;
        m_roles.remove(id);
    }

    emit userRoleRemoved(id);
}


