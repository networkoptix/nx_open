// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_roles_manager.h"

#include <set>
#include <unordered_map>
#include <vector>

#include <QtCore/QVector>
#include <QtCore/QList>
#include <QtCore/QSet>

#include <core/resource_management/resource_pool.h>
#include <core/resource/user_resource.h>
#include <nx/utils/log/log_main.h>
#include <nx/vms/common/system_context.h>

const QList<Qn::UserRole>& QnPredefinedUserRoles::enumValues()
{
    static const QList<Qn::UserRole> kList({
        Qn::UserRole::owner,
        Qn::UserRole::administrator,
        Qn::UserRole::advancedViewer,
        Qn::UserRole::viewer,
        Qn::UserRole::liveViewer,
    });
    return kList;
}

Qn::UserRole QnPredefinedUserRoles::enumValue(const QnUuid& id)
{
    static const std::unordered_map<QnUuid, Qn::UserRole> kValues =
        []()
        {
            std::unordered_map<QnUuid, Qn::UserRole> result;
            for (Qn::UserRole role: enumValues())
                result[QnPredefinedUserRoles::id(role)] = role;

            result[QnUuid()] = Qn::UserRole::customPermissions;
            return result;
        }();

    if (const auto it = kValues.find(id); it != kValues.end())
        return it->second;
    return Qn::UserRole::customUserRole;
}

QnUuid QnPredefinedUserRoles::id(Qn::UserRole userRole)
{
    static const std::unordered_map<Qn::UserRole, QnUuid> kIds =
        []()
        {
            std::unordered_map<Qn::UserRole, QnUuid> result;
            for (Qn::UserRole role: enumValues())
            {
                result[role] = QnUuid(QString("00000000-0000-0000-0000-1000%1")
                    .arg(int(role), 8, 16, QChar('0')));
            }
            return result;
        }();

    if (const auto it = kIds.find(userRole); it != kIds.end())
        return it->second;
    return QnUuid{};
}

QString QnPredefinedUserRoles::name(Qn::UserRole userRole)
{
    switch (userRole)
    {
        case Qn::UserRole::owner:
            return tr("Owner");

        case Qn::UserRole::administrator:
            return tr("Administrator");

        case Qn::UserRole::advancedViewer:
            return tr("Advanced Viewer");

        case Qn::UserRole::viewer:
            return tr("Viewer");

        case Qn::UserRole::liveViewer:
            return tr("Live Viewer");

        case Qn::UserRole::customUserRole:
            return tr("Custom Role");

        case Qn::UserRole::customPermissions:
            return tr("Custom");
    }

    return QString();
}

QString QnPredefinedUserRoles::description(Qn::UserRole userRole)
{
    switch (userRole)
    {
        case Qn::UserRole::owner:
            return tr("Has access to whole System and can do everything.");

        case Qn::UserRole::administrator:
            return tr("Has access to whole System and can manage it. Can create users.");

        case Qn::UserRole::advancedViewer:
            return tr("Can manage all cameras and bookmarks.");

        case Qn::UserRole::viewer:
            return tr("Can view all cameras and export video.");

        case Qn::UserRole::liveViewer:
            return tr("Can view live video from all cameras.");

        case Qn::UserRole::customUserRole:
            return tr("Custom user role.");

        case Qn::UserRole::customPermissions:
            return tr("Custom permissions.");
    }

    return QString();
}

GlobalPermissions QnPredefinedUserRoles::permissions(Qn::UserRole userRole)
{
    switch (userRole)
    {
        case Qn::UserRole::owner:
        case Qn::UserRole::administrator:
            return GlobalPermission::adminPermissions;

        case Qn::UserRole::advancedViewer:
            return GlobalPermission::advancedViewerPermissions;

        case Qn::UserRole::viewer:
            return GlobalPermission::viewerPermissions;

        case Qn::UserRole::liveViewer:
            return GlobalPermission::liveViewerPermissions;

        case Qn::UserRole::customPermissions:
            return GlobalPermission::customUser;

        case Qn::UserRole::customUserRole:
            return {};
    }

    return {};
}

std::optional<QnUuid> QnPredefinedUserRoles::presetId(GlobalPermissions permissions)
{
    if (permissions.testFlag(GlobalPermission::owner))
        return id(Qn::UserRole::owner);

    if (permissions.testFlag(GlobalPermission::admin))
        return id(Qn::UserRole::administrator);

    if (permissions.testFlag(GlobalPermission::customUser))
        return std::nullopt;

    if (permissions.testFlag(GlobalPermission::advancedViewerPermissions))
        return id(Qn::UserRole::advancedViewer);

    if (permissions.testFlag(GlobalPermission::viewerPermissions))
        return id(Qn::UserRole::viewer);

    if (permissions.testFlag(GlobalPermission::liveViewerPermissions))
        return id(Qn::UserRole::liveViewer);

    return std::nullopt;
}

QnPredefinedUserRoles::UserRoleDataList QnPredefinedUserRoles::list()
{
    static UserRoleDataList kPredefinedRoles =
        []()
        {
            UserRoleDataList roles;
            for (auto role: enumValues())
            {
                roles.emplace_back(UserRoleData::makePredefined(
                    id(role),
                    name(role),
                    permissions(role)));
            }
            return roles;
        }();
    return kPredefinedRoles;
}

std::optional<QnPredefinedUserRoles::UserRoleData> QnPredefinedUserRoles::get(const QnUuid& id)
{
    static const auto kPredefinedRolesById =
        []()
        {
            std::map<QnUuid, UserRoleData> result;
            for (auto role: QnPredefinedUserRoles::list())
                result[role.id] = role;
            return result;
        }();
    if (const auto it = kPredefinedRolesById.find(id); it != kPredefinedRolesById.end())
        return it->second;
    return std::nullopt;
}

const QList<QnUuid>& QnPredefinedUserRoles::adminIds()
{
    static const QList<QnUuid> kIds{id(Qn::UserRole::owner), id(Qn::UserRole::administrator)};
    return kIds;
}

QnUserRolesManager::QnUserRolesManager(nx::vms::common::SystemContext* context, QObject* parent):
    base_type(parent),
    nx::vms::common::SystemContextAware(context)
{
}

QnUserRolesManager::~QnUserRolesManager()
{
}

template<class IDList>
void QnUserRolesManager::usersAndRoles(const IDList& ids, QnUserResourceList& users, QList<QnUuid>& roles)
{
    users = resourcePool()->getResourcesByIds<QnUserResource>(ids);

    NX_MUTEX_LOCKER lk(&m_mutex);
    roles.clear();
    for (const auto& id: ids)
    {
        if (isValidRoleId(id))
            roles << id;
    }
}

// Instantiations for std::vector, std::set, QVector, QList and QSet:
template NX_VMS_COMMON_API void QnUserRolesManager::usersAndRoles(
    const std::vector<QnUuid>& ids, QnUserResourceList& users, QList<QnUuid>& roles);
template NX_VMS_COMMON_API void QnUserRolesManager::usersAndRoles(
    const std::set<QnUuid>& ids, QnUserResourceList& users, QList<QnUuid>& roles);
template NX_VMS_COMMON_API void QnUserRolesManager::usersAndRoles(
    const QVector<QnUuid>& ids, QnUserResourceList& users, QList<QnUuid>& roles);
template NX_VMS_COMMON_API void QnUserRolesManager::usersAndRoles(
    const QList<QnUuid>& ids, QnUserResourceList& users, QList<QnUuid>& roles);
template NX_VMS_COMMON_API void QnUserRolesManager::usersAndRoles(
    const QSet<QnUuid>& ids, QnUserResourceList& users, QList<QnUuid>& roles);

QnUserRolesManager::UserRoleDataList QnUserRolesManager::userRoles() const
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    UserRoleDataList result;
    result.reserve(m_roles.size());
    for (const auto& role: m_roles)
        result.push_back(role);
    return result;
}

void QnUserRolesManager::resetUserRoles(const UserRoleDataList& userRoles)
{
    UserRoleDataList removed;
    UserRoleDataList updated;
    {
        NX_MUTEX_LOCKER lk(&m_mutex);

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
    {
        NX_DEBUG(this, "%1 removed", role);
        emit userRoleRemoved(role);
    }
    for (auto role: updated)
    {
        NX_DEBUG(this, "%1 added or updated", role);
        emit userRoleAddedOrUpdated(role);
    }
}

bool QnUserRolesManager::hasRole(const QnUuid& id) const
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    return m_roles.contains(id);
}

bool QnUserRolesManager::hasRoles(const std::vector<QnUuid>& ids) const
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    for (const auto& id: ids)
    {
        if (!m_roles.contains(id))
            return false;
    }
    return true;
}

QnUserRolesManager::UserRoleData QnUserRolesManager::userRole(const QnUuid& id) const
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    return m_roles.value(id);
}

void QnUserRolesManager::addOrUpdateUserRole(const UserRoleData& role)
{
    if (!NX_ASSERT(!role.id.isNull()))
        return;

    {
        NX_MUTEX_LOCKER lk(&m_mutex);
        if (m_roles.value(role.id) == role)
            return;
        m_roles[role.id] = role;
    }

    NX_DEBUG(this, "%1 added or updated", role);
    emit userRoleAddedOrUpdated(role);
}

void QnUserRolesManager::removeUserRole(const QnUuid& id)
{
    NX_ASSERT(!id.isNull());
    UserRoleData role;
    {
        NX_MUTEX_LOCKER lk(&m_mutex);
        if (!m_roles.contains(id))
            return; /*< Perfectly valid outcome when we have deleted the role by ourselves. */

        role = m_roles.take(id);
    }

    NX_DEBUG(this, "%1 removed", role);
    emit userRoleRemoved(role);
}

QString QnUserRolesManager::userRoleName(const QnUuid& userRoleId)
{
    // TODO: Add support for translations for predefined roles.
    return userRole(userRoleId).name;
}

QString QnUserRolesManager::userRoleName(const QnUserResourcePtr& user) const
{
    NX_ASSERT(user);
    if (!user)
        return QString();
    Qn::UserRole userRole = user->userRole();
    if (userRole == Qn::UserRole::customUserRole)
    {
        QStringList names;
        for (const auto& role: user->userRoleIds())
            names << this->userRole(role).name;
        return names.join(", ");
    }

    return QnPredefinedUserRoles::name(userRole);
}

// This function is not thread-safe and should be called under external mutex lock.
bool QnUserRolesManager::isValidRoleId(const QnUuid& id) const
{
    switch (QnPredefinedUserRoles::enumValue(id))
    {
        case Qn::UserRole::customUserRole:
            return m_roles.contains(id); //< Existing custom role.

        case Qn::UserRole::customPermissions:
            return false; //< Null uuid.

        default:
            return true; //< Predefined role.
    }
}
