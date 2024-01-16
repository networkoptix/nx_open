// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_roles_model.h"

#include <QtCore/QScopedValueRollback>
#include <QtCore/QVector>

#include <client/client_globals.h>
#include <common/common_globals.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/resource_access_manager.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/string.h>
#include <nx/vms/api/data/user_group_data.h>
#include <ui/models/private/user_roles_model_p.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_context_aware.h>

#include "private/user_roles_model_p.h"

QnUserRolesModel::QnUserRolesModel(QObject* parent, DisplayRoleFlags flags):
    base_type(parent),
    d(new Private(this, flags))
{
}

QnUserRolesModel::~QnUserRolesModel()
{
    // Required here for forward-declared scoped pointer destruction.
}

void QnUserRolesModel::setUserRoles(const nx::vms::api::UserGroupDataList& roles)
{
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
    d->setCustomRoleStrings(name, description);
}

bool QnUserRolesModel::hasCheckBoxes() const
{
    return d->hasCheckBoxes;
}

void QnUserRolesModel::setHasCheckBoxes(bool value)
{
    if (d->hasCheckBoxes == value)
        return;

    ScopedReset reset(this);
    d->hasCheckBoxes = value;
}

Qt::ItemFlags QnUserRolesModel::flags(const QModelIndex& index) const
{
    if (index.model() != this || !hasIndex(index.row(), index.column(), index.parent()))
        return Qt::NoItemFlags;

    Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    if (index.column() == CheckColumn)
        flags |= Qt::ItemIsUserCheckable;

    return flags;
}

QVariant QnUserRolesModel::data(const QModelIndex& index, int role) const
{
    if (index.model() != this || !hasIndex(index.row(), index.column(), index.parent()))
        return QVariant();

    auto roleModel = d->roleByRow(index.row());

    switch (role)
    {
        // Role name.
        case Qt::DisplayRole:
        case Qt::AccessibleTextRole:
            return index.column() == NameColumn
                ? roleModel.name
                : QString();

        // Role description.
        case Qt::ToolTipRole:
        case Qt::StatusTipRole:
        case Qt::AccessibleDescriptionRole:
            return index.column() == NameColumn
                ? roleModel.description
                : QString();

        // Role check state.
        case Qt::CheckStateRole:
            return index.column() == CheckColumn
                ? QVariant(d->checked.contains(index) ? Qt::Checked : Qt::Unchecked)
                : QVariant();

        // Role uuid (for custom roles).
        case Qn::UuidRole:
            return QVariant::fromValue(d->id(index.row()));

        // Role permissions (for built-in roles).
        case Qn::GlobalPermissionsRole:
            return QVariant::fromValue(roleModel.permissions);

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

    if (checked)
        d->checked.insert(index);
    else
        d->checked.remove(index);

    emit dataChanged(
        index.sibling(index.row(), 0),
        index.sibling(index.row(), columnCount() - 1));

    return true;
}

QSet<QnUuid> QnUserRolesModel::checkedRoles() const
{
    QSet<QnUuid> result;

    for (const auto& index: d->checked)
        result.insert(d->id(index.row()));

    return result;
}

void QnUserRolesModel::setCheckedRoles(const QSet<QnUuid>& ids)
{
    d->checked.clear();

    QHash<QnUuid, int> rowById;
    for (int row = 0; row < d->count(); ++row)
        rowById[d->id(row)] = row;

    for (const auto& id: ids)
    {
        const int row = rowById.value(id, -1);
        if (row >= 0)
            d->checked.insert(createIndex(row, CheckColumn));
    }

    emit dataChanged(
        createIndex(0, 0),
        createIndex(rowCount() - 1, columnCount() - 1));
}
