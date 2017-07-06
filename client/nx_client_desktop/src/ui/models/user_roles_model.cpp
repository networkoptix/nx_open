#include "user_roles_model.h"

#include <algorithm>
#include <QtCore/QVector>
#include <QtCore/QScopedValueRollback>

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

    return hasCheckBoxes() ? 2 : 1;
}

void QnUserRolesModel::setCustomRoleStrings(const QString& name, const QString& description)
{
    Q_D(QnUserRolesModel);
    d->setCustomRoleStrings(name, description);
}

bool QnUserRolesModel::hasCheckBoxes() const
{
    Q_D(const QnUserRolesModel);
    return d->m_hasCheckBoxes;
}

void QnUserRolesModel::setHasCheckBoxes(bool value)
{
    Q_D(QnUserRolesModel);
    if (d->m_hasCheckBoxes == value)
        return;

    ScopedReset reset(this);
    d->m_hasCheckBoxes = value;
}

bool QnUserRolesModel::userCheckable() const
{
    Q_D(const QnUserRolesModel);
    return d->m_userCheckable;
}

void QnUserRolesModel::setUserCheckable(bool value)
{
    Q_D(QnUserRolesModel);
    d->m_userCheckable = value;
}

bool QnUserRolesModel::predefinedRoleIdsEnabled() const
{
    Q_D(const QnUserRolesModel);
    return d->m_predefinedRoleIdsEnabled;
}

void QnUserRolesModel::setPredefinedRoleIdsEnabled(bool value)
{
    Q_D(QnUserRolesModel);
    if (d->m_predefinedRoleIdsEnabled == value)
        return;

    d->m_predefinedRoleIdsEnabled = value;

    if (const int count = rowCount())
        emit dataChanged(index(0, 0), index(count - 1, columnCount() - 1));
}

Qt::ItemFlags QnUserRolesModel::flags(const QModelIndex& index) const
{
    if (index.model() != this || !hasIndex(index.row(), index.column(), index.parent()))
        return Qt::NoItemFlags;

    Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    if (userCheckable() && index.column() == CheckColumn)
        flags |= Qt::ItemIsUserCheckable;

    return flags;
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
            return index.column() == NameColumn
                ? roleModel.name
                : QString();

        /* Role description: */
        case Qt::ToolTipRole:
        case Qt::StatusTipRole:
        case Qt::AccessibleDescriptionRole:
            return index.column() == NameColumn
                ? roleModel.description
                : QString();

        /* Role check state: */
        case Qt::CheckStateRole:
            return index.column() == CheckColumn
                ? QVariant(d->m_checked.contains(index) ? Qt::Checked : Qt::Unchecked)
                : QVariant();

        /* Role uuid (for custom roles): */
        case Qn::UuidRole:
            return QVariant::fromValue(d->id(index.row(), d->m_predefinedRoleIdsEnabled));

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

bool QnUserRolesModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (index.model() != this || !hasIndex(index.row(), index.column(), index.parent()))
        return false;

    if (!hasCheckBoxes() || index.column() != CheckColumn || role != Qt::CheckStateRole)
        return false;

    const bool checked = value.toInt() == Qt::Checked;

    Q_D(QnUserRolesModel);
    if (checked)
        d->m_checked.insert(index);
    else
        d->m_checked.remove(index);

    emit dataChanged(
        index.sibling(index.row(), 0),
        index.sibling(index.row(), columnCount() - 1));

    return true;
}

QSet<QnUuid> QnUserRolesModel::checkedRoles() const
{
    QSet<QnUuid> result;

    Q_D(const QnUserRolesModel);
    for (const auto& index: d->m_checked)
        result.insert(d->id(index.row(), true));

    return result;
}

void QnUserRolesModel::setCheckedRoles(const QSet<QnUuid>& ids)
{
    Q_D(QnUserRolesModel);
    d->m_checked.clear();

    QHash<QnUuid, int> rowById;
    for (int row = 0; row < d->count(); ++row)
        rowById[d->id(row, true)] = row;

    for (const auto& id: ids)
    {
        const int row = rowById.value(id, -1);
        if (row >= 0)
            d->m_checked.insert(createIndex(row, CheckColumn));
    }

    emit dataChanged(
        createIndex(0, 0),
        createIndex(rowCount() - 1, columnCount() - 1));
}
