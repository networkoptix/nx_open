#include "user_roles_model.h"

#include <algorithm>
#include <QtCore/QVector>

#include <common/common_globals.h>

#include <core/resource_management/resource_access_manager.h>
#include <core/resource/user_resource.h>

#include <nx/utils/string.h>
#include <nx/utils/raii_guard.h>

#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context_aware.h>
#include <ui/workbench/workbench_context.h>

namespace
{
    struct RoleDescription
    {
        RoleDescription() {}

        RoleDescription(const Qn::UserRole roleType,
            const QString& name = QString(),
            const QnUuid& roleUuid = QnUuid()) :
                roleType(roleType),
                name(name.isEmpty() ? QnResourceAccessManager::userRoleName(roleType) : name),
                description(QnResourceAccessManager::userRoleDescription(roleType)),
                permissions(QnResourceAccessManager::userRolePermissions(roleType)),
                roleUuid(roleUuid)
        {
        }

        explicit RoleDescription(const ec2::ApiUserGroupData& userRole) :
            RoleDescription(
                Qn::UserRole::CustomUserGroup,
                userRole.name,
                userRole.id)
        {
        }

        Qn::UserRole roleType;
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
        firstUserRoleIndex(0),
        customRoleEnabled(custom)
    {
        std::sort(userRoles.begin(), userRoles.end(), &QnUserRolesModelPrivate::lessRoleByName);
        updateRoles();

        connect(context(), &QnWorkbenchContext::userChanged, this, &QnUserRolesModelPrivate::updateRoles);

        if (user)
            watchUserRoleChanged(true);
    }

    void setStandardRoles(const QList<Qn::UserRole>& roles)
    {
        if (standardRoles == roles)
            return;

        Q_Q(QnUserRolesModel);
        QnUserRolesModel::ScopedReset reset(q);
        standardRoles = roles;
        updateRoles();
    }

    void setStandardRoles(bool enabled)
    {
        if (enabled)
            setStandardRoles(allStandardRoles());
        else
            setStandardRoles(QList<Qn::UserRole>());
    }

    void setUserRoles(ec2::ApiUserGroupDataList roles, bool watchServerChanges)
    {
        watchUserRoleChanged(watchServerChanges);

        std::sort(roles.begin(), roles.end(), &QnUserRolesModelPrivate::lessRoleByName);
        if (userRoles == roles)
            return;

        Q_Q(QnUserRolesModel);
        QnUserRolesModel::ScopedReset reset(q);
        userRoles = roles;
        updateRoles();
    }

    void setUserRoles(bool enabled)
    {
        if (enabled)
            setUserRoles(allUserRoles(), true);
        else
            setUserRoles(ec2::ApiUserGroupDataList(), false);
    }

    void enableCustomRole(bool enable)
    {
        if (customRoleEnabled == enable)
            return;

        Q_Q(QnUserRolesModel);
        QnUserRolesModel::ScopedReset reset(q);

        customRoleEnabled = enable;
        if (customRoleEnabled)
            roles << RoleDescription(Qn::UserRole::CustomPermissions);
        else
            roles.pop_back();
    }

    void updateRoles()
    {
        Q_Q(QnUserRolesModel);
        QnUserRolesModel::ScopedReset reset(q);

        roles.clear();
        firstUserRoleIndex = 0;

        if (!context()->user())
            return;

        for (auto role : standardRoles)
        {
            if (qnResourceAccessManager->canCreateUser(context()->user(), qnResourceAccessManager->userRolePermissions(role), false))
                roles << RoleDescription(role);
        }

        firstUserRoleIndex = roles.size();

        for (auto role : userRoles)
            roles << RoleDescription(role);

        if (customRoleEnabled)
            roles << RoleDescription(Qn::UserRole::CustomPermissions);
    }

    bool updateUserRole(const ec2::ApiUserGroupData& userRole)
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

            int indexInFullList = insertionPosition - userRoles.begin() + firstUserRoleIndex;

            QnUserRolesModel::ScopedInsertRows insertRows(q, QModelIndex(), indexInFullList, indexInFullList);
            userRoles.insert(insertionPosition, userRole);
            roles.insert(indexInFullList, RoleDescription(userRole));
            return true;
        }

        /* If updated: */
        int sourceIndex = roleIterator - userRoles.begin();
        int indexInFullList = sourceIndex + firstUserRoleIndex;

        if (roleIterator->name != userRole.name)
        {
            auto newPosition = std::upper_bound(userRoles.begin(), userRoles.end(),
                userRole, &QnUserRolesModelPrivate::lessRoleByName);

            int destinationIndex = newPosition - userRoles.begin();
            int destinationInFullList = destinationIndex + firstUserRoleIndex;

            /* Update row order if sorting changes: */
            if (sourceIndex != destinationIndex && sourceIndex + 1 != destinationIndex)
            {
                QnUserRolesModel::ScopedMoveRows moveRows(q,
                    QModelIndex(), indexInFullList, indexInFullList,
                    QModelIndex(), destinationInFullList);

                if (destinationIndex > sourceIndex)
                {
                    /* Role moved forward: */
                    std::rotate(roleIterator, roleIterator + 1, newPosition);
                    --newPosition;
                    --destinationInFullList;
                }
                else
                {
                    /* Role moved backward: */
                    using reverse = ec2::ApiUserGroupDataList::reverse_iterator;
                    std::rotate(reverse(roleIterator + 1), reverse(roleIterator), reverse(newPosition));
                }

                updateRoles();

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
        int indexInFullList = roleIterator - userRoles.begin() + firstUserRoleIndex;

        QnUserRolesModel::ScopedRemoveRows removeRows(q, QModelIndex(), indexInFullList, indexInFullList);
        userRoles.erase(roleIterator);
        roles.erase(roles.begin() + indexInFullList);
        return true;
    }

    void watchUserRoleChanged(bool watch)
    {
        if (watch)
        {
            addOrUpdateRoleConnection = connect(
                qnResourceAccessManager, &QnResourceAccessManager::userGroupAddedOrUpdated,
                this, &QnUserRolesModelPrivate::updateUserRole);

            removeRoleConnection = connect(
                qnResourceAccessManager, &QnResourceAccessManager::userGroupRemoved,
                this, &QnUserRolesModelPrivate::removeUserRole);
        }
        else
        {
            QObject::disconnect(addOrUpdateRoleConnection);
            QObject::disconnect(removeRoleConnection);
        }
    }

    static const QList<Qn::UserRole>& allStandardRoles()
    {
        static QList<Qn::UserRole> roles;
        if (roles.empty())
        {
            for (auto role : QnResourceAccessManager::predefinedRoles())
                if (role != Qn::UserRole::Owner)
                    roles << role;
        }

        return roles;
    }

    static ec2::ApiUserGroupDataList allUserRoles()
    {
        return qnResourceAccessManager->userGroups();
    }

    static bool lessRoleByName(const ec2::ApiUserGroupData& r1, const ec2::ApiUserGroupData& r2)
    {
        return nx::utils::naturalStringCompare(r1.name, r2.name, Qt::CaseInsensitive) < 0;
    }

public:
    QnUserRolesModel* q_ptr;
    Q_DECLARE_PUBLIC(QnUserRolesModel);

    QList<Qn::UserRole> standardRoles;
    ec2::ApiUserGroupDataList userRoles;

    int firstUserRoleIndex;
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

void QnUserRolesModel::setUserRoles(const ec2::ApiUserGroupDataList& roles)
{
    Q_D(QnUserRolesModel);
    d->setUserRoles(roles, true);
}

bool QnUserRolesModel::updateUserRole(const ec2::ApiUserGroupData& role)
{
    Q_D(QnUserRolesModel);
    return d->updateUserRole(role);
}

bool QnUserRolesModel::removeUserRole(const QnUuid& id)
{
    Q_D(QnUserRolesModel);
    return d->removeUserRole(id);
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

        /* Role type: */
        case Qn::UserRoleRole:
            return QVariant::fromValue(d->roles[index.row()].roleType);

        default:
            return QVariant();
    }
}
