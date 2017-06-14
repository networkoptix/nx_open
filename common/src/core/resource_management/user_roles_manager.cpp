#include "user_roles_manager.h"

#include <core/resource/user_resource.h>

namespace Qn {

static uint qHash(UserRole role)
{
    return uint(role);
}

} // namespace Qn

namespace {

QnUuid predefinedRoleUuid(Qn::UserRole role)
{
    return int(role) < 0
        ? QnUuid()
        : QnUuid(lit("00000000-0000-0000-0000-1000%1").arg(int(role), 8, 16, QChar(L'0')));
}

} // namespace

QnUserRolesManager::QnUserRolesManager(QObject* parent):
    base_type(parent),
    QnCommonModuleAware(parent)
{
}

QnUserRolesManager::~QnUserRolesManager()
{
}

ec2::ApiUserRoleDataList QnUserRolesManager::userRoles() const
{
    QnMutexLocker lk(&m_mutex);
    ec2::ApiUserRoleDataList result;
    result.reserve(m_roles.size());
    for (const auto& role: m_roles)
        result.push_back(role);
    return result;
}

void QnUserRolesManager::resetUserRoles(const ec2::ApiUserRoleDataList& userRoles)
{
    ec2::ApiUserRoleDataList removed;
    ec2::ApiUserRoleDataList updated;
    {
        QnMutexLocker lk(&m_mutex);

        QSet<QnUuid> newRoles;
        for (const auto& role: userRoles)
        {
            newRoles << role.id;
            if (m_roles[role.id] != role)
            {
                m_roles[role.id] = role;
                updated.push_back(role);
            }
        }

        for (const QnUuid& id: m_roles.keys())
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

ec2::ApiUserRoleData QnUserRolesManager::userRole(const QnUuid& id) const
{
    QnMutexLocker lk(&m_mutex);
    return m_roles.value(id);
}

QnUuid QnUserRolesManager::unifiedUserRoleId(const QnUserResourcePtr& user)
{
    if (!user)
        return QnUuid();

    const auto userRole = user->userRole();
    return userRole == Qn::UserRole::CustomUserRole
        ? user->userRoleId()
        : predefinedRoleId(userRole);
}

void QnUserRolesManager::addOrUpdateUserRole(const ec2::ApiUserRoleData& role)
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
    ec2::ApiUserRoleData role;
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

QnUuid QnUserRolesManager::predefinedRoleId(Qn::UserRole userRole)
{
    static const QHash<Qn::UserRole, QnUuid> predefinedRoleIds =
        []()
        {
            QHash<Qn::UserRole, QnUuid> result;
            for (Qn::UserRole role: QnUserRolesManager::predefinedRoles())
                result[role] = predefinedRoleUuid(role);
            return result;
        }();

    return predefinedRoleIds[userRole];
}

Qn::UserRole QnUserRolesManager::predefinedRole(const QnUuid& id)
{
    static const QHash<QnUuid, Qn::UserRole> predefinedRolesById =
        []()
        {
            QHash<QnUuid, Qn::UserRole> result;
            for (Qn::UserRole role: QnUserRolesManager::predefinedRoles())
                result[QnUserRolesManager::predefinedRoleId(role)] = role;

            result[QnUuid()] = Qn::UserRole::CustomPermissions;
            return result;
        }();

    return predefinedRolesById.value(id, Qn::UserRole::CustomUserRole);
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

        case Qn::UserRole::CustomUserRole:
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
            return tr("Has access to whole System and can do everything.");

        case Qn::UserRole::Administrator:
            return tr("Has access to whole System and can manage it. Can create users.");

        case Qn::UserRole::AdvancedViewer:
            return tr("Can manage all cameras and bookmarks.");

        case Qn::UserRole::Viewer:
            return tr("Can view all cameras and export video.");

        case Qn::UserRole::LiveViewer:
            return tr("Can view live video from all cameras.");

        case Qn::UserRole::CustomUserRole:
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

        case Qn::UserRole::CustomUserRole:
            return Qn::NoGlobalPermissions;
    }

    return Qn::NoGlobalPermissions;
}

QString QnUserRolesManager::userRoleName(const QnUserResourcePtr& user) const
{
    NX_ASSERT(user);
    if (!user)
        return QString();
    Qn::UserRole userRole = user->userRole();
    if (userRole == Qn::UserRole::CustomUserRole)
        return this->userRole(user->userRoleId()).name;

    return userRoleName(userRole);
}

ec2::ApiPredefinedRoleDataList QnUserRolesManager::getPredefinedRoles()
{
    static ec2::ApiPredefinedRoleDataList kPredefinedRoles;
    if (kPredefinedRoles.empty())
    {
        for (auto role: predefinedRoles())
        {
            kPredefinedRoles.emplace_back(
                userRoleName(role),
                userRolePermissions(role),
                role == Qn::UserRole::Owner);
        }
    }
    return kPredefinedRoles;
}

