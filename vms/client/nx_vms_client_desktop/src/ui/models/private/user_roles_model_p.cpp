// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_roles_model_p.h"

#include <core/resource/user_resource.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/resource_access_subject.h>
#include <core/resource_management/user_roles_manager.h>
#include <nx/utils/string.h>
#include <nx/vms/client/desktop/system_context.h>
#include <ui/workbench/workbench_context.h>

namespace {

bool lessRoleByName(const nx::vms::api::UserRoleData& r1, const nx::vms::api::UserRoleData& r2)
{
    return nx::utils::naturalStringCompare(r1.name, r2.name, Qt::CaseInsensitive) < 0;
};

QList<Qn::UserRole> allStandardRoles()
{
    return QnUserRolesManager::predefinedRoles();
}

} // namespace

RoleDescription::RoleDescription(const Qn::UserRole roleType):
    roleType(roleType),
    name(QnUserRolesManager::userRoleName(roleType)),
    description(QnUserRolesManager::userRoleDescription(roleType)),
    permissions(QnUserRolesManager::userRolePermissions(roleType)),
    roleUuid(QnUuid())
{
}

RoleDescription::RoleDescription(const nx::vms::api::UserRoleData& userRole):
    roleType(Qn::UserRole::customUserRole),
    name(userRole.name),
    description(QnUserRolesManager::userRoleDescription(roleType)),
    permissions(QnUserRolesManager::userRolePermissions(roleType)),
    roleUuid(userRole.id)
{
}

QnUserRolesModelPrivate::QnUserRolesModelPrivate(
    QnUserRolesModel* parent,
    QnUserRolesModel::DisplayRoleFlags flags)
    :
    base_type(),
    QnWorkbenchContextAware(parent),
    q_ptr(parent),
    m_customRoleEnabled(flags.testFlag(QnUserRolesModel::CustomRoleFlag)),
    m_onlyAssignable(flags.testFlag(QnUserRolesModel::AssignableFlag))
{
    connect(context(), &QnWorkbenchContext::userChanged, this,
        &QnUserRolesModelPrivate::updateStandardRoles);
    updateStandardRoles();

    connect(parent, &QAbstractItemModel::modelAboutToBeReset, this,
        [this]() { m_checked.clear(); });

    if (flags.testFlag(QnUserRolesModel::UserRoleFlag))
    {
        m_userRoles = userRolesManager()->userRoles();
        std::sort(m_userRoles.begin(), m_userRoles.end(), lessRoleByName);

        connect(userRolesManager(), &QnUserRolesManager::userRoleAddedOrUpdated, this,
            &QnUserRolesModelPrivate::updateUserRole);
        connect(userRolesManager(), &QnUserRolesManager::userRoleRemoved, this,
            &QnUserRolesModelPrivate::removeUserRole);
    }
}

int QnUserRolesModelPrivate::rowForUser(const QnUserResourcePtr& user) const
{
    /* Rows order: standard, user-defined, custom. */

    int defaultRow = m_customRoleEnabled
        ? count() - 1
        : -1;

    auto role = user->userRole();
    switch (role)
    {
        case Qn::UserRole::customUserRole:
        {
            auto roleIterator = std::find_if(m_userRoles.begin(), m_userRoles.end(),
                [roleId = user->firstRoleId()](const UserRoleData& role)
                {
                    return role.id == roleId;
                });

            if (roleIterator == m_userRoles.end())
                return defaultRow;

            return std::distance(m_userRoles.begin(), roleIterator) + m_standardRoles.size();
        }

        case Qn::UserRole::customPermissions:
            return defaultRow;

        default:
            break;
    }

    return m_standardRoles.indexOf(role);
}

int QnUserRolesModelPrivate::rowForRole(Qn::UserRole role) const
{
    return m_standardRoles.indexOf(role);
}

void QnUserRolesModelPrivate::setUserRoles(UserRoleDataList value)
{
    std::sort(value.begin(), value.end(), lessRoleByName);
    if (m_userRoles == value)
        return;

    Q_Q(QnUserRolesModel);
    QnUserRolesModel::ScopedReset reset(q);
    m_userRoles = value;
}

void QnUserRolesModelPrivate::updateStandardRoles()
{
    QList<Qn::UserRole> available;
    if (m_onlyAssignable)
    {
        if (auto user = context()->user())
        {
            for (auto role: allStandardRoles())
            {
                if (resourceAccessManager()->canCreateUser(user, role))
                    available << role;
            }
        }
    }
    else
    {
        available = allStandardRoles();
    }

    if (available == m_standardRoles)
        return;

    Q_Q(QnUserRolesModel);
    QnUserRolesModel::ScopedReset reset(q);
    m_standardRoles = available;
}

bool QnUserRolesModelPrivate::updateUserRole(const UserRoleData& userRole)
{
    Q_Q(QnUserRolesModel);
    auto roleIterator = std::find_if(m_userRoles.begin(), m_userRoles.end(),
        [&userRole](const UserRoleData& role)
        {
            return role.id == userRole.id;
        });

    auto indexOffset = m_standardRoles.size();

    /* If added: */
    if (roleIterator == m_userRoles.end())
    {
        auto insertionPosition = std::upper_bound(m_userRoles.begin(), m_userRoles.end(),
            userRole, lessRoleByName);

        int row = std::distance(m_userRoles.begin(), insertionPosition) + indexOffset;

        QnUserRolesModel::ScopedInsertRows insertRows(q,  row, row);
        m_userRoles.insert(insertionPosition, userRole);
        return true;
    }

    /* If updated: */
    int sourceIndex = std::distance(m_userRoles.begin(), roleIterator);
    int sourceRow = sourceIndex + indexOffset;

    if (roleIterator->name != userRole.name)
    {
        auto newPosition = std::upper_bound(m_userRoles.begin(), m_userRoles.end(),
            userRole, lessRoleByName);

        int destinationIndex = std::distance(m_userRoles.begin(), newPosition);
        int destinationRow = destinationIndex + indexOffset;

        /* Update row order if sorting changes: */
        if (sourceIndex != destinationIndex && sourceIndex + 1 != destinationIndex)
        {
            QnUserRolesModel::ScopedMoveRows moveRows(q,
                QModelIndex(), sourceRow, sourceRow,
                QModelIndex(), destinationRow);

            if (destinationIndex > sourceIndex)
            {
                /* Role moved forward: */
                std::rotate(roleIterator, roleIterator + 1, newPosition);
                --newPosition;
                --destinationRow;
            }
            else
            {
                /* Role moved backward: */
                using reverse = UserRoleDataList::reverse_iterator;
                std::rotate(reverse(roleIterator + 1), reverse(roleIterator), reverse(newPosition));
            }

            /* Prepare new position for data change: */
            roleIterator = newPosition;
            sourceRow = destinationRow;
        }
    }

    /* Data change: */
    *roleIterator = userRole;
    QModelIndex changed = q->index(sourceRow, 0);
    emit q->dataChanged(changed, changed);
    return false;
}

bool QnUserRolesModelPrivate::removeUserRoleById(const QnUuid& roleId)
{
    auto roleIterator = std::find_if(m_userRoles.begin(), m_userRoles.end(),
        [&roleId](const UserRoleData& role)
        {
            return role.id == roleId;
        });

    if (roleIterator == m_userRoles.end())
        return false;

    int row = std::distance(m_userRoles.begin(), roleIterator) + m_standardRoles.size();

    Q_Q(QnUserRolesModel);
    QnUserRolesModel::ScopedRemoveRows removeRows(q,  row, row);
    m_userRoles.erase(roleIterator);
    return true;
}

bool QnUserRolesModelPrivate::removeUserRole(const UserRoleData& userRole)
{
    return removeUserRoleById(userRole.id);
}

RoleDescription QnUserRolesModelPrivate::roleByRow(int row) const
{
    NX_ASSERT(row >= 0 && row < count());
    if (row < 0)
        return RoleDescription();

    if (row < m_standardRoles.size())
        return RoleDescription(m_standardRoles[row]);

    row -= m_standardRoles.size();
    if (row < m_userRoles.size())
        return RoleDescription(m_userRoles[row]);

    NX_ASSERT(m_customRoleEnabled);
    if (m_customRoleEnabled)
    {
        auto result = RoleDescription(Qn::UserRole::customPermissions);
        if (!m_customRoleName.isEmpty())
            result.name = m_customRoleName;
        if (!m_customRoleDescription.isEmpty())
            result.description = m_customRoleDescription;
        return result;
    }

    return RoleDescription();
}

int QnUserRolesModelPrivate::count() const
{
    int total = m_standardRoles.size() + (int)m_userRoles.size();
    if (m_customRoleEnabled)
        ++total;
    return total;
}

void QnUserRolesModelPrivate::setCustomRoleStrings(const QString& name, const QString& description)
{
    if (m_customRoleName == name && m_customRoleDescription == description)
        return;

    m_customRoleName = name;
    m_customRoleDescription = description;

    if (!m_customRoleEnabled)
        return;

    Q_Q(QnUserRolesModel);
    QModelIndex customRoleIndex = q->index(count() - 1, 0);
    emit q->dataChanged(customRoleIndex, customRoleIndex);
}

QnUuid QnUserRolesModelPrivate::id(int row, bool predefinedRoleIdsEnabled) const
{
    const auto role = roleByRow(row);
    return role.roleType != Qn::UserRole::customUserRole && predefinedRoleIdsEnabled
        ? QnUserRolesManager::predefinedRoleId(role.roleType)
        : role.roleUuid;
}
