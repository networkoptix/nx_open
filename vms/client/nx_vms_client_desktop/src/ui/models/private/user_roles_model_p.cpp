// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_roles_model_p.h"

#include <core/resource/user_resource.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/resource_access_subject.h>
#include <nx/utils/string.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/user_management/user_group_manager.h>
#include <ui/workbench/workbench_context.h>

using namespace nx::vms::common;

using nx::vms::api::UserGroupData;
using nx::vms::api::UserGroupDataList;

namespace {

bool lessRoleByName(const UserGroupData& r1, const UserGroupData& r2)
{
    return nx::utils::naturalStringCompare(r1.name, r2.name, Qt::CaseInsensitive) < 0;
};

} // namespace

QnUserRolesModel::Private::Private(
    QnUserRolesModel* q,
    QnUserRolesModel::DisplayRoleFlags flags)
    :
    base_type(),
    QnWorkbenchContextAware(q),
    q(q),
    m_customRoleEnabled(flags.testFlag(QnUserRolesModel::CustomRoleFlag)),
    m_onlyAssignable(flags.testFlag(QnUserRolesModel::AssignableFlag))
{
    connect(q, &QAbstractItemModel::modelAboutToBeReset, this,
        [this]() { checked.clear(); });

    if (flags.testFlag(QnUserRolesModel::UserRoleFlag))
    {
        m_userRoles = systemContext()->userGroupManager()->groups();
        std::sort(m_userRoles.begin(), m_userRoles.end(), lessRoleByName);

        connect(systemContext()->userGroupManager(), &UserGroupManager::addedOrUpdated,
            this, &QnUserRolesModel::Private::updateUserRole);
        connect(systemContext()->userGroupManager(), &UserGroupManager::removed,
            this, &QnUserRolesModel::Private::removeUserRole);
    }
}

void QnUserRolesModel::Private::setUserRoles(UserGroupDataList value)
{
    std::sort(value.begin(), value.end(), lessRoleByName);
    if (m_userRoles == value)
        return;

    QnUserRolesModel::ScopedReset reset(q);
    m_userRoles = value;
}

bool QnUserRolesModel::Private::updateUserRole(const UserGroupData& userRole)
{
    auto roleIterator = std::find_if(m_userRoles.begin(), m_userRoles.end(),
        [&userRole](const UserGroupData& role)
        {
            return role.id == userRole.id;
        });

    // If added.
    if (roleIterator == m_userRoles.end())
    {
        const auto insertionPosition = std::upper_bound(m_userRoles.begin(), m_userRoles.end(),
            userRole, lessRoleByName);

        const int row = std::distance(m_userRoles.begin(), insertionPosition);

        QnUserRolesModel::ScopedInsertRows insertRows(q,  row, row);
        m_userRoles.insert(insertionPosition, userRole);
        return true;
    }

    // If updated.
    int sourceRow = std::distance(m_userRoles.begin(), roleIterator);

    if (roleIterator->name != userRole.name)
    {
        auto newPosition = std::upper_bound(m_userRoles.begin(), m_userRoles.end(),
            userRole, lessRoleByName);

        int destinationRow = std::distance(m_userRoles.begin(), newPosition);

        // Update row order if sorting changes.
        if (sourceRow != destinationRow && sourceRow + 1 != destinationRow)
        {
            QnUserRolesModel::ScopedMoveRows moveRows(q,
                QModelIndex(), sourceRow, sourceRow,
                QModelIndex(), destinationRow);

            if (destinationRow > sourceRow)
            {
                // Role moved forward.
                std::rotate(roleIterator, roleIterator + 1, newPosition);
                --newPosition;
                --destinationRow;
            }
            else
            {
                // Role moved backwards.
                using reverse = UserGroupDataList::reverse_iterator;
                std::rotate(reverse(roleIterator + 1), reverse(roleIterator), reverse(newPosition));
            }

            // Prepare new position for data change.
            roleIterator = newPosition;
            sourceRow = destinationRow;
        }
    }

    // Data change.
    *roleIterator = userRole;
    QModelIndex changed = q->index(sourceRow, 0);
    emit q->dataChanged(changed, changed);
    return false;
}

bool QnUserRolesModel::Private::removeUserRoleById(const nx::Uuid& roleId)
{
    const auto roleIterator = std::find_if(m_userRoles.begin(), m_userRoles.end(),
        [&roleId](const UserGroupData& role)
        {
            return role.id == roleId;
        });

    if (roleIterator == m_userRoles.end())
        return false;

    const int row = std::distance(m_userRoles.begin(), roleIterator);

    QnUserRolesModel::ScopedRemoveRows removeRows(q,  row, row);
    m_userRoles.erase(roleIterator);
    return true;
}

bool QnUserRolesModel::Private::removeUserRole(const UserGroupData& userRole)
{
    return removeUserRoleById(userRole.id);
}

UserGroupData QnUserRolesModel::Private::roleByRow(int row) const
{
    if (!NX_ASSERT(row >= 0 && row < count()))
        return {};

    if (row < m_userRoles.size())
        return m_userRoles[row];

    if (!NX_ASSERT(m_customRoleEnabled))
        return {};

    UserGroupData result;
    result.name = m_customRoleName.isEmpty()
        ? tr("Custom")
        : m_customRoleName;
    result.description = m_customRoleDescription.isEmpty()
        ? tr("Custom access rights")
        : m_customRoleDescription;
    return result;
}

int QnUserRolesModel::Private::count() const
{
    const auto total = (int) m_userRoles.size();
    return m_customRoleEnabled ? (total + 1) : total;
}

void QnUserRolesModel::Private::setCustomRoleStrings(const QString& name, const QString& description)
{
    if (m_customRoleName == name && m_customRoleDescription == description)
        return;

    m_customRoleName = name;
    m_customRoleDescription = description;

    if (!m_customRoleEnabled)
        return;

    QModelIndex customRoleIndex = q->index(count() - 1, 0);
    emit q->dataChanged(customRoleIndex, customRoleIndex);
}

nx::Uuid QnUserRolesModel::Private::id(int row) const
{
    return roleByRow(row).id;
}
