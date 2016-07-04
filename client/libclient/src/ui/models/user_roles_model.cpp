#include "user_roles_model.h"

#include <algorithm>
#include <QtCore/QVector>

#include <common/common_globals.h>

#include <core/resource_management/resource_access_manager.h>

#include <nx/utils/string.h>

#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context_aware.h>

namespace
{
    struct RoleDescription
    {
        RoleDescription() {}

        RoleDescription(const QString& name,
            const QString& description,
            Qn::GlobalPermissions permissions,
            const QnUuid& roleUuid = QnUuid()) :
            name(name),
            description(description),
            permissions(permissions),
            roleUuid(roleUuid)
        {
        }

        explicit RoleDescription(Qn::GlobalPermissions permissions) :
            RoleDescription(
                QnWorkbenchAccessController::standardRoleName(permissions),
                QnWorkbenchAccessController::standardRoleDescription(permissions),
                permissions)
        {
        }

        explicit RoleDescription(const ec2::ApiUserGroupData& userRole) :
            RoleDescription(
                userRole.name,
                QnWorkbenchAccessController::customRoleDescription(),
                Qn::NoGlobalPermissions,
                userRole.id)
        {
        }

        QString name;
        QString description;
        Qn::GlobalPermissions permissions;
        QnUuid roleUuid;
    };

} // unnamed namespacee

class QnUserRolesModelPrivate : public Connective<QObject>, public QnWorkbenchContextAware
{
public:
    QnUserRolesModelPrivate(QnUserRolesModel* parent, bool standard, bool user, bool custom) :
        QnWorkbenchContextAware(parent),
        q_ptr(parent),
        standardRoles(standard ? allStandardRoles() : decltype(standardRoles)()),
        userRoles(user ? allUserRoles() : decltype(userRoles)()),
        customRoleEnabled(custom)
    {
        std::sort(userRoles.begin(), userRoles.end(), &QnUserRolesModelPrivate::lessRoleByName);
        updateRoles();
        if (user)
            watchUserRoleChanged(true);
    }

    void setStandardRoles(QList<Qn::GlobalPermissions> roles, bool needCorrection = false)
    {
        if (needCorrection)
        {
            auto newEnd = std::remove_if(roles.begin(), roles.end(),
                [](Qn::GlobalPermissions permissions)
                {
                    return !QnWorkbenchAccessController::standardRoles().contains(permissions);
                });

            roles.erase(newEnd, roles.end());
        }

        if (standardRoles == roles)
            return;

        Q_Q(QnUserRolesModel);
        q->beginResetModel();
        standardRoles = roles;
        updateRoles();
        q->endResetModel();
    }

    void setStandardRoles(bool enabled)
    {
        if (enabled)
            setStandardRoles(allStandardRoles());
        else
            setStandardRoles(QList<Qn::GlobalPermissions>());
    }

    void setUserRoles(ec2::ApiUserGroupDataList roles)
    {
        std::sort(roles.begin(), roles.end(), &QnUserRolesModelPrivate::lessRoleByName);

        if (userRoles == roles)
            return;

        Q_Q(QnUserRolesModel);
        q->beginResetModel();
        userRoles = roles;
        updateRoles();
        q->endResetModel();
    }

    void setUserRoles(const QList<QnUuid>& roles)
    {
        ec2::ApiUserGroupDataList correctRoles;
        ec2::ApiUserGroupDataList availableRoles = qnResourceAccessManager->userGroups();

        for (const auto& id : roles)
        {
            auto roleIterator = std::find_if(availableRoles.begin(), availableRoles.end(),
                [&id](const ec2::ApiUserGroupData& role)
                {
                    return role.id == id;
                });

            if (roleIterator != availableRoles.end())
                correctRoles.push_back(*roleIterator);
        }

        watchUserRoleChanged(true);
        setUserRoles(correctRoles);
    }

    void setUserRoles(bool enabled)
    {
        watchUserRoleChanged(enabled);
        if (enabled)
            setUserRoles(allUserRoles());
        else
            setUserRoles(ec2::ApiUserGroupDataList());
    }

    void enableCustomRole(bool enable)
    {
        if (customRoleEnabled == enable)
            return;

        Q_Q(QnUserRolesModel);
        q->beginResetModel();

        customRoleEnabled = enable;
        if (customRoleEnabled)
            roles << RoleDescription(Qn::NoGlobalPermissions);
        else
            roles.pop_back();

        q->endResetModel();
    }

    void updateRoles()
    {
        roles.clear();

        for (auto permissions : standardRoles)
            roles << RoleDescription(permissions);

        for (auto role : userRoles)
            roles << RoleDescription(role);

        if (customRoleEnabled)
            roles << RoleDescription(Qn::NoGlobalPermissions);
    }

    bool insertOrUpdateUserRole(const ec2::ApiUserGroupData& userRole)
    {
        Q_Q(QnUserRolesModel);
        auto roleIterator = std::find_if(userRoles.begin(), userRoles.end(),
            [&userRole](const ec2::ApiUserGroupData& role)
            {
                return role.id == userRole.id;
            });

        /* If added: */
        if (roleIterator == userRoles.end())
        {
            auto insertionPosition = std::upper_bound(userRoles.begin(), userRoles.end(),
                userRole, &QnUserRolesModelPrivate::lessRoleByName);

            int indexInFullList = insertionPosition - userRoles.begin() + standardRoles.size();
            q->beginInsertRows(QModelIndex(), indexInFullList, indexInFullList);
            userRoles.insert(insertionPosition, userRole);
            roles.insert(indexInFullList, RoleDescription(userRole));
            q->endInsertRows();
            return true;
        }

        /* If updated: */
        int sourceIndex = roleIterator - userRoles.begin();
        int indexInFullList = sourceIndex + standardRoles.size();

        if (roleIterator->name != userRole.name)
        {
            auto newPosition = std::upper_bound(userRoles.begin(), userRoles.end(),
                userRole, &QnUserRolesModelPrivate::lessRoleByName);

            int destinationIndex = newPosition - userRoles.begin();
            int destinationInFullList = destinationIndex + standardRoles.size();

            /* Update row order if sorting changes: */
            if (sourceIndex != destinationIndex && sourceIndex + 1 != destinationIndex)
            {
                q->beginMoveRows(QModelIndex(), indexInFullList, indexInFullList,
                    QModelIndex(), destinationInFullList);

                if (destinationIndex > sourceIndex)
                {
                    /* Role moved forward: */
                    newPosition = std::rotate(roleIterator, roleIterator + 1, newPosition);
                    --destinationInFullList;
                }
                else
                {
                    /* Role moved backward: */
                    using reverse = ec2::ApiUserGroupDataList::reverse_iterator;
                    std::rotate(reverse(roleIterator + 1), reverse(roleIterator), reverse(newPosition));
                }

                updateRoles();
                q->endMoveRows();

                /* Prepare new position for data change: */
                roleIterator = newPosition;
                indexInFullList = destinationInFullList;
            }
        }

        /* Data change: */
        *roleIterator = userRole;
        roles[indexInFullList] = RoleDescription(userRole);
        QModelIndex changed = q->index(indexInFullList, 0);
        emit q->dataChanged(changed, changed);
        return false;
    }

    bool removeUserRole(const QnUuid& id)
    {
        auto roleIterator = std::find_if(userRoles.begin(), userRoles.end(),
            [&id](const ec2::ApiUserGroupData& role)
            {
                return role.id == id;
            });

        if (roleIterator == userRoles.end())
            return false;

        Q_Q(QnUserRolesModel);
        int indexInFullList = roleIterator - userRoles.begin() + standardRoles.size();
        q->beginRemoveRows(QModelIndex(), indexInFullList, indexInFullList);
        userRoles.erase(roleIterator);
        roles.erase(roles.begin() + indexInFullList);
        q->endRemoveRows();
        return true;
    }

    void watchUserRoleChanged(bool watch)
    {
        if (watch)
        {
            addOrUpdateRoleConnection = connect(
                qnResourceAccessManager, &QnResourceAccessManager::userGroupAddedOrUpdated,
                this, &QnUserRolesModelPrivate::insertOrUpdateUserRole);

            removeRoleConnection = connect(
                qnResourceAccessManager, &QnResourceAccessManager::userGroupRemoved,
                this, &QnUserRolesModelPrivate::removeUserRole);
        }
        else
        {
            qnResourceAccessManager->disconnect(addOrUpdateRoleConnection);
            qnResourceAccessManager->disconnect(removeRoleConnection);
        }
    }

    static const QList<Qn::GlobalPermissions>& allStandardRoles()
    {
        return QnWorkbenchAccessController::standardRoles();
    }

    static ec2::ApiUserGroupDataList allUserRoles()
    {
        return qnResourceAccessManager->userGroups();
    }

    static bool lessRoleByName(const ec2::ApiUserGroupData& r1, const ec2::ApiUserGroupData& r2)
    {
        return nx::utils::naturalStringCompare(r1.name, r2.name) < 0;
    }

public:
    QnUserRolesModel* q_ptr;
    Q_DECLARE_PUBLIC(QnUserRolesModel);

    QList<Qn::GlobalPermissions> standardRoles;
    ec2::ApiUserGroupDataList userRoles;
    bool customRoleEnabled;

    QMetaObject::Connection addOrUpdateRoleConnection;
    QMetaObject::Connection removeRoleConnection;

    /* Roles present in the model: */
    QList<RoleDescription> roles;
};

/*
* QnUserRolesModel
*/

QnUserRolesModel::QnUserRolesModel(QObject* parent, bool standardRoles, bool userRoles, bool customRole) :
    base_type(parent),
    d_ptr(new QnUserRolesModelPrivate(this, standardRoles, userRoles, customRole))
{
}

QnUserRolesModel::~QnUserRolesModel()
{
}

void QnUserRolesModel::setStandardRoles(bool enabled)
{
    Q_D(QnUserRolesModel);
    d->setStandardRoles(enabled);
}

void QnUserRolesModel::setStandardRoles(const QList<Qn::GlobalPermissions>& standardRoles)
{
    Q_D(QnUserRolesModel);
    d->setStandardRoles(standardRoles, true);
}

void QnUserRolesModel::setCustomRole(bool enabled)
{
    Q_D(QnUserRolesModel);
    d->enableCustomRole(enabled);
}

void QnUserRolesModel::setUserRoles(bool enabled)
{
    Q_D(QnUserRolesModel);
    d->setUserRoles(enabled);
}

void QnUserRolesModel::setUserRoles(const QList<QnUuid>& roles)
{
    Q_D(QnUserRolesModel);
    d->setUserRoles(roles);
}

QModelIndex QnUserRolesModel::index(int row, int column, const QModelIndex& parent) const
{
    if (!hasIndex(row, column, parent))
        return QModelIndex();

    return createIndex(row, column);
}

QModelIndex QnUserRolesModel::parent(const QModelIndex& child) const
{
    Q_UNUSED(child);
    return QModelIndex();
}

int QnUserRolesModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;

    Q_D(const QnUserRolesModel);
    return d->roles.size();
}

int QnUserRolesModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;

    return 1;
}

QVariant QnUserRolesModel::data(const QModelIndex& index, int role) const
{
    if (index.model() != this || !hasIndex(index.row(), index.column(), index.parent()))
        return QVariant();

    Q_D(const QnUserRolesModel);
    switch (role)
    {
        /* Role name: */
        case Qt::DisplayRole:
        case Qt::AccessibleTextRole:
            return d->roles[index.row()].name;

        /* Role description: */
        case Qt::ToolTipRole:
        case Qt::AccessibleDescriptionRole:
            return d->roles[index.row()].description;

        /* Role uuid (for custom roles): */
        case Qn::UuidRole:
            return QVariant::fromValue(d->roles[index.row()].roleUuid);

        /* Role permissions (for built-in roles): */
        case Qn::GlobalPermissionsRole:
            return QVariant::fromValue(d->roles[index.row()].permissions);

        default:
            return QVariant();
    }
}
