#include "user_groups_settings_model.h"

#include <core/resource_management/resource_access_manager.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/user_resource.h>

#include <ui/style/resource_icon_cache.h>

#include <nx/utils/string.h>


QnUserGroupSettingsModel::RoleReplacement::RoleReplacement() :
    RoleReplacement(QnUuid(), Qn::GlobalLiveViewerPermissionSet)
{
}

QnUserGroupSettingsModel::RoleReplacement::RoleReplacement(
            const QnUuid& group,
            Qn::GlobalPermissions permissions) :
    group(group),
    permissions(permissions)
{
}

QnUserGroupSettingsModel::QnUserGroupSettingsModel(QObject* parent /*= nullptr*/) :
    base_type(parent),
    m_currentGroupId(),
    m_groups()
{
}

QnUserGroupSettingsModel::~QnUserGroupSettingsModel()
{
}

ec2::ApiUserGroupDataList QnUserGroupSettingsModel::groups() const
{
    return m_groups;
}

void QnUserGroupSettingsModel::setGroups(const ec2::ApiUserGroupDataList& value)
{
    beginResetModel();

    m_groups = value;
    std::sort(m_groups.begin(), m_groups.end(), [](const ec2::ApiUserGroupData& l, const ec2::ApiUserGroupData& r)
    {
        /* Case Insensitive sort. */
        return nx::utils::naturalStringCompare(l.name, r.name, Qt::CaseInsensitive) < 0;
    });

    m_accessibleResources.clear();
    for (const auto& group : m_groups)
        m_accessibleResources[group.id] = qnResourceAccessManager->accessibleResources(group.id);

    m_replacements.clear();

    m_accessibleLayoutsPreviews.clear();

    endResetModel();
}

int QnUserGroupSettingsModel::addGroup(const ec2::ApiUserGroupData& group)
{
    NX_ASSERT(!group.id.isNull());
    int row = static_cast<int>(m_groups.size());

    beginInsertRows(QModelIndex(), row, row);
    m_groups.push_back(group);
    endInsertRows();

    return row;
}

void QnUserGroupSettingsModel::removeGroup(const QnUuid& id, const RoleReplacement& replacement)
{
    auto iter = std::find_if(m_groups.begin(), m_groups.end(), [id](const ec2::ApiUserGroupData& elem) { return elem.id == id; });
    int row = std::distance(m_groups.begin(), iter);

    beginRemoveRows(QModelIndex(), row, row);
    m_groups.erase(iter);
    m_replacements[id] = replacement;
    endRemoveRows();

    if (id == m_currentGroupId)
        m_currentGroupId = QnUuid();
}

void QnUserGroupSettingsModel::selectGroup(const QnUuid& value)
{
    m_currentGroupId = value;
}

QnUuid QnUserGroupSettingsModel::selectedGroup() const
{
    return m_currentGroupId;
}

QString QnUserGroupSettingsModel::groupName() const
{
    auto iter = currentGroup();
    if (iter == m_groups.cend())
        return QString();
    return iter->name;
}

void QnUserGroupSettingsModel::setGroupName(const QString& value)
{
    auto iter = currentGroup();
    if (iter == m_groups.end())
        return;

    int row = std::distance(m_groups.begin(), iter);
    QModelIndex idx = index(row);
    iter->name = value;
    emit dataChanged(idx, idx);
}

Qn::GlobalPermissions QnUserGroupSettingsModel::rawPermissions() const
{
    auto iter = currentGroup();
    if (iter == m_groups.cend())
        return Qn::NoGlobalPermissions;
    return iter->permissions;
}

void QnUserGroupSettingsModel::setRawPermissions(Qn::GlobalPermissions value)
{
    auto iter = currentGroup();
    if (iter == m_groups.end())
        return;
    iter->permissions = value;
}

QSet<QnUuid> QnUserGroupSettingsModel::accessibleResources() const
{
    return m_accessibleResources.value(m_currentGroupId);
}

QSet<QnUuid> QnUserGroupSettingsModel::accessibleResources(const QnUuid& groupId) const
{
    return m_accessibleResources.value(groupId);
}

int QnUserGroupSettingsModel::rowCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return static_cast<int>(m_groups.size());
}

QVariant QnUserGroupSettingsModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_groups.size())
        return QVariant();

    auto group = m_groups[index.row()];

    switch (role)
    {
    case Qt::DisplayRole:
    case Qt::AccessibleTextRole:
    case Qt::ToolTipRole:
    case Qt::StatusTipRole:
    case Qt::WhatsThisRole:
    case Qt::AccessibleDescriptionRole:
        return group.name;

    case Qt::DecorationRole:
        return qnResIconCache->icon(QnResourceIconCache::Users);

    case Qn::UuidRole:
        return qVariantFromValue(group.id);

    default:
        break;
    }
    return QVariant();
}

void QnUserGroupSettingsModel::setAccessibleResources(const QSet<QnUuid>& value)
{
    if (m_currentGroupId.isNull())
        return;

    m_accessibleResources[m_currentGroupId] = value;
}

ec2::ApiUserGroupDataList::iterator QnUserGroupSettingsModel::currentGroup()
{
    if (m_currentGroupId.isNull())
        return m_groups.end();

    return std::find_if(m_groups.begin(), m_groups.end(), [this](const ec2::ApiUserGroupData& elem) { return elem.id == m_currentGroupId; });
}

ec2::ApiUserGroupDataList::const_iterator QnUserGroupSettingsModel::currentGroup() const
{
    if (m_currentGroupId.isNull())
        return m_groups.cend();

    return std::find_if(m_groups.cbegin(), m_groups.cend(), [this](const ec2::ApiUserGroupData& elem) { return elem.id == m_currentGroupId; });
}

QnUserResourceList QnUserGroupSettingsModel::users(const QnUuid& groupId, bool withCandidates) const
{
    if (groupId.isNull())
        return QnUserResourceList();

    return qnResPool->getResources<QnUserResource>().filtered([this, groupId, withCandidates](const QnUserResourcePtr& user)
    {
        QnUuid userGroup = user->userGroup();
        if (userGroup == groupId)
            return true;

        if (!withCandidates)
            return false;

        return replacement(userGroup).group == groupId;
    });
}

QnUserResourceList QnUserGroupSettingsModel::users(bool withCandidates) const
{
    return users(m_currentGroupId, withCandidates);
}

QnUserGroupSettingsModel::RoleReplacement QnUserGroupSettingsModel::replacement(const QnUuid& source) const
{
    RoleReplacement replacement = directReplacement(source);
    while (!replacement.group.isNull())
    {
        auto iterator = m_replacements.find(replacement.group);
        if (iterator != m_replacements.end())
            replacement = *iterator;
        else
            break;
    }

    return replacement;
}

QnUserGroupSettingsModel::RoleReplacement QnUserGroupSettingsModel::directReplacement(const QnUuid& source) const
{
    return m_replacements[source];
}

void QnUserGroupSettingsModel::setAccessibleLayoutsPreview(const QSet<QnUuid>& value)
{
    m_accessibleLayoutsPreviews[m_currentGroupId] = value;
}

QnIndirectAccessProviders QnUserGroupSettingsModel::accessibleLayouts() const
{
    auto role = qnResourceAccessManager->userGroup(m_currentGroupId);
    auto layouts = QnResourceAccessProvider().indirectlyAccessibleLayouts(role);

    for (auto layoutPreview : m_accessibleLayoutsPreviews[m_currentGroupId])
        layouts.insert(layoutPreview, QSet<QnResourcePtr>());

    return layouts;
}
