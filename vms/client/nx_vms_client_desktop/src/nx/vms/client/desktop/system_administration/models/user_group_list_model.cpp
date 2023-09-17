// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_group_list_model.h"

#include <algorithm>

#include <QtCore/QCollator>
#include <QtCore/QStringList>

#include <client/client_globals.h>
#include <core/resource_access/access_rights_manager.h>
#include <core/resource_access/resource_access_subject_hierarchy.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/format.h>
#include <nx/utils/qt_helpers.h>
#include <nx/vms/api/data/ldap.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/skin/color_substitutions.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/system_administration/watchers/non_editable_users_and_groups.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/utils/ldap_status_watcher.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/common/system_settings.h>
#include <nx/vms/common/user_management/predefined_user_groups.h>
#include <nx/vms/common/user_management/user_group_manager.h>

#include "private/non_unique_name_tracker.h"

namespace nx::vms::client::desktop {

namespace {

static const QColor kLight10Color = "#A5B7C0";
static const core::SvgIconColorer::IconSubstitutions kEnabledUncheckedIconSubstitutions = {
    {QIcon::Selected, {{kLight10Color, "light4"}}}};
static const core::SvgIconColorer::IconSubstitutions kEnabledCheckedIconSubstitutions = {
    {QIcon::Normal, {{kLight10Color, "light2"}}},
    {QIcon::Selected, {{kLight10Color, "light4"}}}};
static const core::SvgIconColorer::IconSubstitutions kDisabledUncheckedIconSubstitutions = {
    {QIcon::Normal, {{kLight10Color, "dark16"}}},
    {QIcon::Selected, {{kLight10Color, "light13"}}}};
static const core::SvgIconColorer::IconSubstitutions kDisabledCheckedIconSubstitutions = {
    {QIcon::Normal, {{kLight10Color, "light13"}}},
    {QIcon::Selected, {{kLight10Color, "light10"}}}};

} // namespace

// -----------------------------------------------------------------------------------------------
// UserGroupListModel::Private

struct UserGroupListModel::Private
{
    UserGroupListModel* const q;
    QString syncId;

    UserGroupDataList orderedGroups;
    QSet<QnUuid> checkedGroupIds;
    QList<QSet<QnUuid>> cycledGroupSets;
    QSet<QnUuid> cycledGroups;
    bool ldapServerOnline = true;
    QSet<QnUuid> notFoundGroups;
    NonUniqueNameTracker nonUniqueNameTracker;

    Private(UserGroupListModel* q): q(q), syncId(q->globalSettings()->ldap().syncId())
    {
        connect(q->systemContext()->nonEditableUsersAndGroups(),
            &NonEditableUsersAndGroups::groupModified,
            q,
            [q, this](const QnUuid &id)
            {
                const int row = q->groupRow(id);
                if (row == -1)
                    return;

                if (q->systemContext()->nonEditableUsersAndGroups()->containsGroup(id))
                    checkedGroupIds.remove(id);

                emit q->dataChanged(
                    q->index(row, CheckBoxColumn),
                    q->index(row, CheckBoxColumn),
                    {Qn::DisabledRole, Qt::ToolTipRole});
            });

        connect(q->globalSettings(), &common::SystemSettings::ldapSettingsChanged, q,
            [this]()
            {
                syncId = this->q->globalSettings()->ldap().syncId();
                updateLdapGroupsNotFound();
            });

        connect(q->systemContext()->ldapStatusWatcher(), &LdapStatusWatcher::statusChanged, q,
            [this, q]()
            {
                if (const auto status = q->systemContext()->ldapStatusWatcher()->status())
                    ldapServerOnline = (status->state == api::LdapStatus::State::online);

                emit q->dataChanged(
                    q->index(0, GroupWarningColumn),
                    q->index(q->rowCount() - 1, GroupWarningColumn),
                    {Qn::DecorationPathRole, Qt::DecorationRole, Qt::ToolTipRole});
            });
    }

    QStringList getParentGroupNames(const UserGroupData& group) const
    {
        QStringList result;
        for (const auto parentId: group.parentGroupIds)
        {
            const auto parentGroup = q->systemContext()->userGroupManager()->find(parentId);
            if (NX_ASSERT(parentGroup))
                result.push_back(parentGroup->name);
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

    bool ldapGroupNotFound(const UserGroupData& group) const
    {
        return !group.externalId.dn.isEmpty() && group.externalId.syncId != syncId;
    }

    bool isChecked(const UserGroupData& group) const
    {
        return checkedGroupIds.contains(group.id);
    }

    static bool isBuiltIn(const UserGroupData& group)
    {
        return common::PredefinedUserGroups::contains(group.id);
    }

    bool hasOwnPermissions(const UserGroupData& group) const
    {
        return group.permissions != api::GlobalPermissions{}
            || !q->systemContext()->accessRightsManager()->ownResourceAccessMap(group.id).isEmpty();
    }

    QString sortKey(const QModelIndex& index) const
    {
        switch (index.column())
        {
            case GroupWarningColumn:
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
        if (!q->checkIndex(index))
            return {};

        const auto groupId = orderedGroups[index.row()].id;
        const int ldapDefaultKey = groupId == nx::vms::api::kDefaultLdapGroupId ? 0 : 1;

        const auto groupsPreSortKey =
            [&]() -> QString
            {
                if (common::PredefinedUserGroups::contains(groupId))
                    return groupId.toString();

                return QString::number(ldapDefaultKey);
            };

        switch (index.column())
        {
            case GroupWarningColumn:
                return nx::format("%1\n%2",
                    sortKey(index.siblingAtColumn(GroupWarningColumn)),
                    sortKey(index.siblingAtColumn(NameColumn))).toQString();

            case GroupTypeColumn:
                return nx::format("%1\n%2\n%3",
                    sortKey(index.siblingAtColumn(GroupTypeColumn)),
                    groupsPreSortKey(),
                    sortKey(index.siblingAtColumn(NameColumn))).toQString();

            case NameColumn:
                return nx::format("%1\n%2",
                    sortKey(index.siblingAtColumn(NameColumn)),
                    sortKey(index.siblingAtColumn(GroupTypeColumn))).toQString();

            case DescriptionColumn:
                return nx::format("%1\n%2\n%3\n%4",
                    sortKey(index.siblingAtColumn(DescriptionColumn)),
                    sortKey(index.siblingAtColumn(GroupTypeColumn)),
                    ldapDefaultKey,
                    sortKey(index.siblingAtColumn(NameColumn))).toQString();

            case ParentGroupsColumn:
                return nx::format("%1\n%2\n%3\n%4",
                    sortKey(index.siblingAtColumn(ParentGroupsColumn)),
                    sortKey(index.siblingAtColumn(GroupTypeColumn)),
                    groupsPreSortKey(),
                    sortKey(index.siblingAtColumn(NameColumn))).toQString();

            case PermissionsColumn:
                return nx::format("%1\n%2\n%3\n%4",
                    sortKey(index.siblingAtColumn(PermissionsColumn)),
                    sortKey(index.siblingAtColumn(GroupTypeColumn)),
                    groupsPreSortKey(),
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

    void refreshCycledGroupSets(const QSet<QnUuid>& groupsToCheck = {})
    {
        if (groupsToCheck.isEmpty())
        {
            cycledGroupSets = q->systemContext()->accessSubjectHierarchy()->findCycledGroupSets();
        }
        else
        {
            // Remove all group sets containing the specified groups.

            auto groups = groupsToCheck;

            while (!groups.empty())
            {
                const QnUuid group = *groups.begin();
                groups.erase(groups.begin());
                for (auto it = cycledGroupSets.begin(); it != cycledGroupSets.end();
                    /*no increment*/)
                {
                    if (!it->contains(group))
                    {
                        ++it;
                        continue;
                    }

                    groups -= *it;
                    it = cycledGroupSets.erase(it);
                }
            }

            cycledGroupSets += q->systemContext()->accessSubjectHierarchy()
                ->findCycledGroupSetsForSpecificGroups(groupsToCheck);
        }

        cycledGroups.clear();
        for (const QSet<QnUuid>& cycledGroupSet: cycledGroupSets)
            cycledGroups.unite(cycledGroupSet);
    }

    void markGroupNotFound(const QnUuid& groupId, bool notFound = true)
    {
        if (notFound)
        {
            const auto count = notFoundGroups.size();
            notFoundGroups.insert(groupId);
            if (count < notFoundGroups.count())
                emit q->notFoundGroupsChanged();
        }
        else
        {
            if (notFoundGroups.remove(groupId))
                emit q->notFoundGroupsChanged();
        }
    }

    void updateLdapGroupsNotFound()
    {
        int groupIndex = -1;
        for (const auto& group: orderedGroups)
        {
            ++groupIndex;

            if (group.type != api::UserType::ldap)
                continue;

            const bool notFound = notFoundGroups.contains(group.id);
            const bool notFoundNewValue = ldapGroupNotFound(group);

            if (notFound != notFoundNewValue)
            {
                markGroupNotFound(group.id, notFoundNewValue);
                emit q->dataChanged(
                    q->index(groupIndex, GroupWarningColumn),
                    q->index(groupIndex, GroupWarningColumn),
                    {Qn::DecorationPathRole, Qt::DecorationRole, Qt::ToolTipRole});
            }
        }
    }
};

// -----------------------------------------------------------------------------------------------
// UserGroupListModel

UserGroupListModel::UserGroupListModel(SystemContext* systemContext, QObject* parent):
    base_type(parent),
    SystemContextAware(systemContext),
    d(new Private(this))
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
    using namespace common::html;

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
                case CheckBoxColumn:
                    if (systemContext()->nonEditableUsersAndGroups()->containsGroup(group.id))
                        return tr("You do not have permissions to modify or delete this group.");
                    break;

                case GroupWarningColumn:
                {
                    QStringList lines;
                    if (d->ldapServerOnline && d->notFoundGroups.contains(group.id))
                        lines << tr("Group is not found in the LDAP database.");

                    if (!d->nonUniqueNameTracker.isUnique(group.id))
                    {
                        lines << tr("There are multiple groups with this name in the system. To "
                            "maintain a clear and organized structure, we suggest providing "
                            "unique names for each group.");
                    }

                    if (d->cycledGroups.contains(group.id))
                    {
                        lines << tr("Group has another group as both its parent, and as a child "
                            "member, or is a part of such circular reference chain. This can lead "
                            "to an incorrect calculation of permissions.");
                    }

                    return lines.join("\n");
                }

                case GroupTypeColumn:
                {
                    if (group.type == nx::vms::api::UserType::ldap)
                        return tr("LDAP group");

                    return Private::isBuiltIn(group)
                        ? tr("Built-in group")
                        : tr("Custom group");
                }

                case ParentGroupsColumn:
                {
                    QStringList groupNames = d->getParentGroupNames(group);
                    for (auto& line: groupNames)
                        line = toHtmlEscaped(line);
                    return noWrap(groupNames.join(kLineBreak));
                }

                default:
                    return noWrap(toHtmlEscaped(data(index, Qt::DisplayRole).toString()));
            }
        }

        case Qn::DecorationPathRole:
        {
            switch (index.column())
            {
                case GroupWarningColumn:
                {
                    if ((d->ldapServerOnline && d->notFoundGroups.contains(group.id))
                        || !d->nonUniqueNameTracker.isUnique(group.id)
                        || d->cycledGroups.contains(group.id))
                    {
                        return QString("user_settings/user_alert.svg");
                    }

                    return {};
                }

                case GroupTypeColumn:
                {
                    if (group.type == nx::vms::api::UserType::ldap)
                        return QString("user_settings/group_ldap.svg");

                    return Private::isBuiltIn(group)
                        ? QString("user_settings/group_built_in.svg")
                        : QString("user_settings/group_custom.svg");
                }

                case PermissionsColumn:
                {
                    if (d->hasOwnPermissions(group))
                        return QString("text_buttons/ok_20.svg");

                    return {};
                }

                default:
                    return {};
            }
        }

        case Qt::DecorationRole:
        {
            if (const auto path = data(index, Qn::DecorationPathRole).toString(); !path.isEmpty())
            {
                core::SvgIconColorer::IconSubstitutions colorSubstitutions;

                const bool checked =
                    data(index.siblingAtColumn(CheckBoxColumn), Qt::CheckStateRole).toBool();
                if (data(index, Qn::DisabledRole).toBool())
                {
                    colorSubstitutions = checked
                        ? kDisabledCheckedIconSubstitutions : kDisabledUncheckedIconSubstitutions;
                }
                else
                {
                    colorSubstitutions = checked
                        ? kEnabledCheckedIconSubstitutions : kEnabledUncheckedIconSubstitutions;
                }
                return qnSkin->icon(path, colorSubstitutions);
            }
            break;
        }

        case Qt::CheckStateRole:
        {
            if (index.column() == CheckBoxColumn)
                return d->checkedGroupIds.contains(group.id) ? Qt::Checked : Qt::Unchecked;
            return {};
        }

        case Qn::DisabledRole:
            if (index.column() == CheckBoxColumn)
                return systemContext()->nonEditableUsersAndGroups()->containsGroup(group.id);
            return false;

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
            return tr("Groups");
        case PermissionsColumn:
            return tr("Custom");
        default:
            return QString();
    }
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

std::optional<api::UserGroupData> UserGroupListModel::findGroup(const QnUuid& groupId) const
{
    const auto it = d->findGroup(groupId);
    return it == d->orderedGroups.cend() ? std::nullopt : std::make_optional(*it);
}

int UserGroupListModel::groupRow(const QnUuid& groupId) const
{
    const auto it = d->findGroup(groupId);
    return it == d->orderedGroups.end() ? -1 : std::distance(d->orderedGroups.begin(), it);
}

QSet<QnUuid> UserGroupListModel::checkedGroupIds() const
{
    return d->checkedGroupIds;
}

QSet<QnUuid> UserGroupListModel::nonEditableGroupIds() const
{
    return systemContext()->nonEditableUsersAndGroups()->groups();
}

void UserGroupListModel::setCheckedGroupIds(const QSet<QnUuid>& value)
{
    d->checkedGroupIds.clear();
    const auto allGroups = groupIds();
    for (const auto& id: value)
    {
        if (!NX_ASSERT(allGroups.contains(id)))
            continue;

        if (systemContext()->nonEditableUsersAndGroups()->containsGroup(id))
            continue;

        d->checkedGroupIds.insert(id);
    }

    emit dataChanged(index(0, CheckBoxColumn), index(rowCount() - 1, CheckBoxColumn),
        {Qt::CheckStateRole});
}

QSet<QnUuid> UserGroupListModel::notFoundGroups() const
{
    return d->notFoundGroups;
}

QSet<QnUuid> UserGroupListModel::nonUniqueGroups() const
{
    return d->nonUniqueNameTracker.nonUniqueNameIds();
}

void UserGroupListModel::setChecked(const QnUuid& groupId, bool checked)
{
    const int row = groupRow(groupId);
    if (row < 0)
        return;

    if (systemContext()->nonEditableUsersAndGroups()->containsGroup(groupId))
        return;

    if (checked)
        d->checkedGroupIds.insert(groupId);
    else
        d->checkedGroupIds.remove(groupId);

    const auto index = this->index(row, CheckBoxColumn);
    emit dataChanged(index, index, {Qt::CheckStateRole});
}

void UserGroupListModel::reset(const UserGroupDataList& groups)
{
    const ScopedReset reset(this);
    d->checkedGroupIds.clear();
    d->notFoundGroups.clear();
    d->nonUniqueNameTracker = {};
    d->orderedGroups = groups;

    std::sort(d->orderedGroups.begin(), d->orderedGroups.end(),
        [](const UserGroupData& left, const UserGroupData& right) { return left.id < right.id; });

    const auto newEnd = std::unique(d->orderedGroups.begin(), d->orderedGroups.end(),
        [](const UserGroupData& left, const UserGroupData& right) { return left.id == right.id; });

    if (!NX_ASSERT(newEnd == d->orderedGroups.end()))
        d->orderedGroups.erase(newEnd, d->orderedGroups.end());

    for (const auto& group: d->orderedGroups)
    {
        if (d->ldapGroupNotFound(group))
            d->notFoundGroups.insert(group.id);

        d->nonUniqueNameTracker.update(group.id, group.name.toLower());
    }
    d->refreshCycledGroupSets();
    emit notFoundGroupsChanged();
    emit nonUniqueGroupsChanged();
}

bool UserGroupListModel::canDeleteGroup(const QnUuid& groupId) const
{
    return systemContext()->accessController()->hasPermissions(groupId, Qn::RemovePermission);
}

bool UserGroupListModel::addOrUpdateGroup(const UserGroupData& group)
{
    if (d->nonUniqueNameTracker.update(group.id, group.name.toLower()))
        emit nonUniqueGroupsChanged();

    const auto position = d->findPosition(group.id);
    if (position == d->orderedGroups.end() || position->id != group.id)
    {
        // Add.
        const int row = position - d->orderedGroups.begin();
        const ScopedInsertRows insertRows(this, row, row);
        d->orderedGroups.insert(position, group);
        d->markGroupNotFound(group.id, d->ldapGroupNotFound(group));
        d->refreshCycledGroupSets(utils::toQSet(group.parentGroupIds) + QSet{group.id});
        return true;
    }

    // Update.
    d->markGroupNotFound(group.id, d->ldapGroupNotFound(group));

    const auto oldParentGroupIds = position->parentGroupIds;

    *position = group;

    if (oldParentGroupIds != group.parentGroupIds)
    {
        const auto groupsToCheck = QSet{group.id}
            + utils::toQSet(oldParentGroupIds)
            + utils::toQSet(group.parentGroupIds);
        d->refreshCycledGroupSets(groupsToCheck);
    }
    const int row = position - d->orderedGroups.begin();
    emit dataChanged(index(row, 0), index(row, ColumnCount - 1));
    return false;
}

bool UserGroupListModel::removeGroup(const QnUuid& groupId)
{
    const auto it = d->findGroup(groupId);
    if (it == d->orderedGroups.end())
        return false;

    if (it->attributes.testFlag(nx::vms::api::UserAttribute::readonly)
        || it->type == api::UserType::cloud)
    {
        return false;
    }

    if (d->nonUniqueNameTracker.remove(groupId))
        emit nonUniqueGroupsChanged();

    const int row = it - d->orderedGroups.begin();

    const ScopedRemoveRows removeRows(this, row, row);
    d->orderedGroups.erase(it);
    d->checkedGroupIds.remove(groupId);
    d->markGroupNotFound(groupId, false);

    // Remove group from other groups.
    for (auto& group: d->orderedGroups)
    {
        auto groupIt = std::find(group.parentGroupIds.begin(), group.parentGroupIds.end(), groupId);
        if (groupIt != group.parentGroupIds.end())
            group.parentGroupIds.erase(groupIt);
    }

    if (d->cycledGroups.contains(groupId))
        d->refreshCycledGroupSets({groupId});

    return true;
}

} // namespace nx::vms::client::desktop
