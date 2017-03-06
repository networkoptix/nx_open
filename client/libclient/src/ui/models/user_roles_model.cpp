#include "user_roles_model.h"

#include <algorithm>
#include <QtCore/QVector>

#include <common/common_globals.h>

#include <core/resource_management/user_roles_manager.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource/user_resource.h>

#include <nx_ec/data/api_user_role_data.h>

#include <nx/utils/string.h>
#include <nx/utils/raii_guard.h>

#include <ui/models/private/user_roles_model_p.h>

#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context_aware.h>
#include <ui/workbench/workbench_context.h>

/*
* QnUserRolesModel
*/

QnUserRolesModel::QnUserRolesModel(QObject* parent, DisplayRoleFlags flags) :
    base_type(parent),
    d_ptr(new QnUserRolesModelPrivate(this, flags))
{
}

QnUserRolesModel::~QnUserRolesModel()
{
}

int QnUserRolesModel::rowForUser(const QnUserResourcePtr& user) const
{
    Q_D(const QnUserRolesModel);
    return d->rowForUser(user);
}

int QnUserRolesModel::rowForRole(Qn::UserRole role) const
{
    Q_D(const QnUserRolesModel);
    return d->rowForRole(role);
}

void QnUserRolesModel::setUserRoles(const ec2::ApiUserRoleDataList& roles)
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
    return d->count();
}

int QnUserRolesModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;

    return 1;
}

void QnUserRolesModel::setCustomRoleStrings(const QString& name, const QString& description)
{
    Q_D(QnUserRolesModel);
    d->setCustomRoleStrings(name, description);
}

QVariant QnUserRolesModel::data(const QModelIndex& index, int role) const
{
    if (index.model() != this || !hasIndex(index.row(), index.column(), index.parent()))
        return QVariant();


    Q_D(const QnUserRolesModel);

    auto roleModel = d->roleByRow(index.row());

    switch (role)
    {
        /* Role name: */
        case Qt::DisplayRole:
        case Qt::AccessibleTextRole:
            return roleModel.name;

        /* Role description: */
        case Qt::ToolTipRole:
        case Qt::AccessibleDescriptionRole:
            return roleModel.description;

        /* Role uuid (for custom roles): */
        case Qn::UuidRole:
            return QVariant::fromValue(roleModel.roleUuid);

        /* Role permissions (for built-in roles): */
        case Qn::GlobalPermissionsRole:
            return QVariant::fromValue(roleModel.permissions);

        /* Role type: */
        case Qn::UserRoleRole:
            return QVariant::fromValue(roleModel.roleType);

        default:
            return QVariant();
    }
}
