// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_group_list_model.h"

#include <algorithm>

#include <QtCore/QCollator>
#include <QtCore/QStringList>

#include <client/client_globals.h>
#include <core/resource_management/user_roles_manager.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/format.h>
#include <nx/vms/client/desktop/style/skin.h>

namespace nx::vms::client::desktop {

// -----------------------------------------------------------------------------------------------
// UserGroupListModel::Private

struct UserGroupListModel::Private
{
    UserGroupListModel* const q;

    UserGroupDataList orderedGroups;
    QSet<QnUuid> checkedGroupIds;

    QStringList getParentGroupNames(const UserGroupData& group) const
    {
        QStringList result;
        for (const auto parentId: group.parentRoleIds)
        {
            const auto it = findGroup(parentId);
            if (NX_ASSERT(it != orderedGroups.cend()))
                result.push_back(it->name);
        }

        if (result.empty())
            return result;

        QCollator collator;
        collator.setCaseSensitivity(Qt::CaseInsensitive);
        collator.setNumericMode(true);

        std::sort(result.begin(), result.end(),
            [&collator](const QString& left, const QString& right)
            {
                return collator(left, right);
            });

        result.erase(std::unique(result.begin(), result.end()), result.end());
        return result;
    }

    QString getParentGroupNamesText(const UserGroupData& group) const
    {
        return getParentGroupNames(group).join(", ");
    }

    bool isChecked(const UserGroupData& group) const
    {
        return checkedGroupIds.contains(group.id);
    }

    static bool isBuiltIn(const UserGroupData& group)
    {
        // TODO: #vkutin Implement it properly when possible.
        return (bool) QnPredefinedUserRoles::get(group.id);
    }

    static bool hasOwnPermissions(const UserGroupData& group)
    {
        return group.permissions != 0;
    }

    QString sortKey(const QModelIndex& index) const
    {
        switch (index.column())
        {
            case GroupTypeColumn:
                return q->data(index, Qt::ToolTipRole).toString();

            case PermissionsColumn:
                return NX_ASSERT(q->checkIndex(index))
                    ? QString(Private::hasOwnPermissions(orderedGroups[index.row()]) ? "1" : "0")
                    : QString();

            default:
                return q->data(index, Qt::DisplayRole).toString();
        }
    }

    QString combinedSortKey(const QModelIndex& index) const
    {
        switch (index.column())
        {
            case GroupTypeColumn:
                return nx::format("%1\n%2",
                    sortKey(index.siblingAtColumn(GroupTypeColumn)),
                    sortKey(index.siblingAtColumn(NameColumn))).toQString();

            case NameColumn:
                return nx::format("%1\n%2",
                    sortKey(index.siblingAtColumn(NameColumn)),
                    sortKey(index.siblingAtColumn(GroupTypeColumn))).toQString();

            case DescriptionColumn:
                return nx::format("%1\n%2\n%3",
                    sortKey(index.siblingAtColumn(DescriptionColumn)),
                    sortKey(index.siblingAtColumn(GroupTypeColumn)),
                    sortKey(index.siblingAtColumn(NameColumn))).toQString();

            case ParentGroupsColumn:
                return nx::format("%1\n%2\n%3",
                    sortKey(index.siblingAtColumn(ParentGroupsColumn)),
                    sortKey(index.siblingAtColumn(GroupTypeColumn)),
                    sortKey(index.siblingAtColumn(NameColumn))).toQString();

            case PermissionsColumn:
                return nx::format("%1\n%2\n%3",
                    sortKey(index.siblingAtColumn(PermissionsColumn)),
                    sortKey(index.siblingAtColumn(GroupTypeColumn)),
                    sortKey(index.siblingAtColumn(NameColumn))).toQString();

            default:
                return QString();
        }
    }

    UserGroupDataList::iterator findPosition(const QnUuid& groupId)
    {
        return std::lower_bound(orderedGroups.begin(), orderedGroups.end(), groupId,
            [](const UserGroupData& left, const QnUuid& right) { return left.id < right; });
    }

    UserGroupDataList::const_iterator findPosition(const QnUuid& groupId) const
    {
        return std::lower_bound(orderedGroups.cbegin(), orderedGroups.cend(), groupId,
            [](const UserGroupData& left, const QnUuid& right) { return left.id < right; });
    }

    UserGroupDataList::iterator findGroup(const QnUuid& groupId)
    {
        const auto position = findPosition(groupId);
        return position != orderedGroups.end() && position->id == groupId
            ? position
            : orderedGroups.end();
    }

    UserGroupDataList::const_iterator findGroup(const QnUuid& groupId) const
    {
        const auto position = findPosition(groupId);
        return position != orderedGroups.cend() && position->id == groupId
            ? position
            : orderedGroups.cend();
    }
};

// -----------------------------------------------------------------------------------------------
// UserGroupListModel

UserGroupListModel::UserGroupListModel(QObject* parent):
    base_type(parent),
    d(new Private{.q = this})
{
}

UserGroupListModel::~UserGroupListModel()
{
    // Required here for forward-declared scoped pointer destruction.
}

int UserGroupListModel::rowCount(const QModelIndex& parent) const
{
    return NX_ASSERT(!parent.isValid()) ? d->orderedGroups.size() : 0;
}

int UserGroupListModel::columnCount(const QModelIndex& parent) const
{
    return NX_ASSERT(!parent.isValid()) ? ColumnCount : 0;
}

QVariant UserGroupListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || !NX_ASSERT(checkIndex(index)))
        return {};

    const auto& group = d->orderedGroups[index.row()];
    switch (role)
    {
        case Qn::UuidRole:
            return QVariant::fromValue(group.id);

        case Qt::DisplayRole:
        {
            switch (index.column())
            {
                case NameColumn:
                    return group.name;
                case DescriptionColumn:
                    return group.description;
                case ParentGroupsColumn:
                    return d->getParentGroupNamesText(group);
                default:
                    return {};
            }
        }

        case Qt::ToolTipRole:
        {
            switch (index.column())
            {
                case GroupTypeColumn:
                {
                    if (group.isLdap)
                        return tr("LDAP group");

                    return Private::isBuiltIn(group)
                        ? tr("Built-in group")
                        : tr("Custom group");
                }

                default:
                    return data(index, Qt::DisplayRole).toString();
            }
        }

        case Qn::DecorationPathRole:
        {
            switch (index.column())
            {
                case GroupTypeColumn:
                {
                    if (group.isLdap)
                        return {};

                    return Private::isBuiltIn(group)
                        ? QString("user_settings/group_built_in.svg")
                        : QString("user_settings/group_custom.svg");
                }

                case PermissionsColumn:
                {
                    if (Private::hasOwnPermissions(group))
                        return QString("text_buttons/ok.svg");

                    return {};
                }

                default:
                    return {};
            }
        }

        case Qt::DecorationRole:
        {
            const auto path = data(index, Qn::DecorationPathRole).toString();
            return path.isEmpty() ? QVariant() : QVariant::fromValue(qnSkin->icon(path));
        }

        case Qt::CheckStateRole:
        {
            if (index.column() == CheckBoxColumn)
                return d->checkedGroupIds.contains(group.id) ? Qt::Checked : Qt::Unchecked;
            return {};
        }

        case Qn::FilterKeyRole:
        {
            switch (index.column())
            {
                case NameColumn:
                case ParentGroupsColumn:
                    return data(index, Qt::DisplayRole);

                default:
                    return {};
            }
        }

        case Qn::SortKeyRole:
            return d->combinedSortKey(index);

        default:
            return {};
    }

    return {};
}

QVariant UserGroupListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Vertical)
        return {};

    if (!NX_ASSERT(section < ColumnCount))
        return {};

    if (role != Qt::DisplayRole)
        return base_type::headerData(section, orientation, role);

    switch (section)
    {
        case NameColumn:
            return tr("Name");
        case DescriptionColumn:
            return tr("Description");
        case ParentGroupsColumn:
            return tr("Member of");
        case PermissionsColumn:
            return tr("Permissions");
        default:
            return QString();
    }
}

bool UserGroupListModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!NX_ASSERT(checkIndex(index))
        || role != Qt::CheckStateRole
        || index.column() != CheckBoxColumn)
    {
        return base_type::setData(index, value, role);
    }

    const auto& group = d->orderedGroups[index.row()];
    const bool checked = value.toBool();

    if (d->isChecked(group) == checked)
        return false;

    if (checked)
        d->checkedGroupIds.insert(group.id);
    else
        d->checkedGroupIds.remove(group.id);

    emit dataChanged(index, index, {Qt::CheckStateRole});
    return true;
}

Qt::ItemFlags UserGroupListModel::flags(const QModelIndex& index) const
{
    if (!NX_ASSERT(checkIndex(index)))
        return Qt::NoItemFlags;

    Qt::ItemFlags flags = Qt::ItemIsSelectable | Qt::ItemIsEnabled;

    if (index.column() == CheckBoxColumn)
        flags |= Qt::ItemIsUserCheckable;

    return flags;
}

const UserGroupListModel::UserGroupDataList& UserGroupListModel::groups() const
{
    return d->orderedGroups;
}

QSet<QnUuid> UserGroupListModel::groupIds() const
{
    QSet<QnUuid> result;
    for (const auto& group: d->orderedGroups)
        result.insert(group.id);

    return result;
}

QSet<QnUuid> UserGroupListModel::checkedGroupIds() const
{
    return d->checkedGroupIds;
}

void UserGroupListModel::setCheckedGroupIds(const QSet<QnUuid>& value)
{
    d->checkedGroupIds = value & groupIds();
    NX_ASSERT(d->checkedGroupIds.size() == value.size());

    emit dataChanged(index(0, CheckBoxColumn), index(rowCount() - 1, CheckBoxColumn),
        {Qt::CheckStateRole});
}

void UserGroupListModel::reset(const UserGroupDataList& groups)
{
    const ScopedReset reset(this);
    d->checkedGroupIds.clear();
    d->orderedGroups = groups;

    std::sort(d->orderedGroups.begin(), d->orderedGroups.end(),
        [](const UserGroupData& left, const UserGroupData& right) { return left.id < right.id; });

    const auto newEnd = std::unique(d->orderedGroups.begin(), d->orderedGroups.end(),
        [](const UserGroupData& left, const UserGroupData& right) { return left.id == right.id; });

    if (!NX_ASSERT(newEnd == d->orderedGroups.end()))
        d->orderedGroups.erase(newEnd, d->orderedGroups.end());
}

bool UserGroupListModel::addOrUpdateGroup(const UserGroupData& group)
{
    const auto position = d->findPosition(group.id);
    if (position == d->orderedGroups.end() || position->id != group.id)
    {
        // Add.
        const int row = position - d->orderedGroups.begin();
        const ScopedInsertRows insertRows(this, row, row);
        d->orderedGroups.insert(position, group);
        return true;
    }

    // Update.
    *position = group;
    const int row = position - d->orderedGroups.begin();
    emit dataChanged(index(row, 0), index(row, ColumnCount - 1));
    return false;
}

bool UserGroupListModel::removeGroup(const QnUuid& groupId)
{
    const auto it = d->findGroup(groupId);
    if (it == d->orderedGroups.end())
        return false;

    const int row = it - d->orderedGroups.begin();

    const ScopedRemoveRows removeRows(this, row, row);
    d->orderedGroups.erase(it);
    d->checkedGroupIds.remove(groupId);
    return true;
}

} // namespace nx::vms::client::desktop
