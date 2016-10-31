#include "user_roles_manager.h"

#include <core/resource/user_resource.h>

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
    ec2::ApiUserGroupDataList removed;
    ec2::ApiUserGroupDataList updated;
    {
        QnMutexLocker lk(&m_mutex);

        QSet<QnUuid> newRoles;
        for (const auto& role : userRoles)
        {
            newRoles << role.id;
            if (m_roles[role.id] != role)
            {
                m_roles[role.id] = role;
                updated.push_back(role);
            }
        }

        for (const QnUuid& id : m_roles.keys())
        {
            if (!newRoles.contains(id))
                removed.push_back(m_roles.take(id));
        }
    }

    for (auto role: removed)
        emit userRoleRemoved(role);
    for (auto role: updated)
        emit userRoleAddedOrUpdated(role);
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
    if (role.isNull())
        return;

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
    ec2::ApiUserGroupData role;
    {
        QnMutexLocker lk(&m_mutex);
        if (!m_roles.contains(id))
            return; /*< Perfectly valid outcome when we have deleted the role by ourselves. */

        role = m_roles.take(id);
    }

    emit userRoleRemoved(role);
}


const QList<Qn::UserRole>& QnUserRolesManager::predefinedRoles()
{
    static const QList<Qn::UserRole> predefinedRoleList({
        Qn::UserRole::Owner,
        Qn::UserRole::Administrator,
        Qn::UserRole::AdvancedViewer,
        Qn::UserRole::Viewer,
        Qn::UserRole::LiveViewer});

    return predefinedRoleList;
}

QString QnUserRolesManager::userRoleName(Qn::UserRole userRole)
{
    switch (userRole)
    {
        case Qn::UserRole::Owner:
            return tr("Owner");

        case Qn::UserRole::Administrator:
            return tr("Administrator");

        case Qn::UserRole::AdvancedViewer:
            return tr("Advanced Viewer");

        case Qn::UserRole::Viewer:
            return tr("Viewer");

        case Qn::UserRole::LiveViewer:
            return tr("Live Viewer");

        case Qn::UserRole::CustomUserGroup:
            return tr("Custom Role");

        case Qn::UserRole::CustomPermissions:
            return tr("Custom");
    }

    return QString();
}

QString QnUserRolesManager::userRoleDescription(Qn::UserRole userRole)
{
    switch (userRole)
    {
        case Qn::UserRole::Owner:
            return tr("Has access to whole system and can do everything.");

        case Qn::UserRole::Administrator:
            return tr("Has access to whole system and can manage it. Can create users.");

        case Qn::UserRole::AdvancedViewer:
            return tr("Can manage all cameras and bookmarks.");

        case Qn::UserRole::Viewer:
            return tr("Can view all cameras and export video.");

        case Qn::UserRole::LiveViewer:
            return tr("Can view live video from all cameras.");

        case Qn::UserRole::CustomUserGroup:
            return tr("Custom user role.");

        case Qn::UserRole::CustomPermissions:
            return tr("Custom permissions.");
    }

    return QString();
}

Qn::GlobalPermissions QnUserRolesManager::userRolePermissions(Qn::UserRole userRole)
{
    switch (userRole)
    {
        case Qn::UserRole::Owner:
        case Qn::UserRole::Administrator:
            return Qn::GlobalAdminPermissionSet;

        case Qn::UserRole::AdvancedViewer:
            return Qn::GlobalAdvancedViewerPermissionSet;

        case Qn::UserRole::Viewer:
            return Qn::GlobalViewerPermissionSet;

        case Qn::UserRole::LiveViewer:
            return Qn::GlobalLiveViewerPermissionSet;

        case Qn::UserRole::CustomPermissions:
            return Qn::GlobalCustomUserPermission;

        case Qn::UserRole::CustomUserGroup:
            return Qn::NoGlobalPermissions;
    }

    return Qn::NoGlobalPermissions;
}

QString QnUserRolesManager::userRoleName(const QnUserResourcePtr& user) const
{
    NX_ASSERT(user);
    if (!user)
        return QString();
    Qn::UserRole roleType = user->role();
    if (roleType == Qn::UserRole::CustomUserGroup)
        return qnUserRolesManager->userRole(user->userGroup()).name;

    return userRoleName(roleType);
}

ec2::ApiPredefinedRoleDataList QnUserRolesManager::getPredefinedRoles()
{
    static ec2::ApiPredefinedRoleDataList kPredefinedRoles;
    if (kPredefinedRoles.empty())
    {
        for (auto role : predefinedRoles())
        {
            kPredefinedRoles.emplace_back(
                userRoleName(role),
                userRolePermissions(role),
                role == Qn::UserRole::Owner);
        }
    }
    return kPredefinedRoles;
}

