#include "user_roles_settings_model.h"

#include <core/resource_access/shared_resources_manager.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>
#include <core/resource/user_resource.h>

#include <ui/style/resource_icon_cache.h>

#include <nx/utils/string.h>


QnUserRolesSettingsModel::RoleReplacement::RoleReplacement() :
    RoleReplacement(QnUuid(), Qn::NoGlobalPermissions)
{
}

QnUserRolesSettingsModel::RoleReplacement::RoleReplacement(
            const QnUuid& role,
            Qn::GlobalPermissions permissions) :
    role(role),
    permissions(permissions)
{
}

bool QnUserRolesSettingsModel::RoleReplacement::isEmpty() const
{
    return role.isNull() && permissions == Qn::NoGlobalPermissions;
}

QnUserRolesSettingsModel::QnUserRolesSettingsModel(QObject* parent /*= nullptr*/) :
    base_type(parent),
    m_currentRoleId(),
    m_roles()
{
}

QnUserRolesSettingsModel::~QnUserRolesSettingsModel()
{
}

ec2::ApiUserGroupDataList QnUserRolesSettingsModel::roles() const
{
    return m_roles;
}

void QnUserRolesSettingsModel::setRoles(const ec2::ApiUserGroupDataList& value)
{
    beginResetModel();

    m_roles = value;
    std::sort(m_roles.begin(), m_roles.end(),
        [](const ec2::ApiUserGroupData& l, const ec2::ApiUserGroupData& r)
        {
            /* Case Insensitive sort. */
            return nx::utils::naturalStringCompare(l.name, r.name, Qt::CaseInsensitive) < 0;
        });

    m_accessibleResources.clear();
    for (const auto& role : m_roles)
        m_accessibleResources[role.id] = qnSharedResourcesManager->sharedResources(role);

    m_replacements.clear();

    endResetModel();
}

int QnUserRolesSettingsModel::addRole(const ec2::ApiUserGroupData& role)
{
    NX_ASSERT(!role.id.isNull());
    int row = static_cast<int>(m_roles.size());

    beginInsertRows(QModelIndex(), row, row);
    m_roles.push_back(role);
    endInsertRows();

    return row;
}

void QnUserRolesSettingsModel::removeRole(const QnUuid& id, const RoleReplacement& replacement)
{
    auto iter = std::find_if(m_roles.begin(), m_roles.end(), [id](const ec2::ApiUserGroupData& elem) { return elem.id == id; });
    int row = std::distance(m_roles.begin(), iter);

    beginRemoveRows(QModelIndex(), row, row);
    m_roles.erase(iter);
    m_replacements[id] = replacement;
    endRemoveRows();

    if (id == m_currentRoleId)
        m_currentRoleId = QnUuid();
}

void QnUserRolesSettingsModel::selectRole(const QnUuid& value)
{
    m_currentRoleId = value;
}

QnUuid QnUserRolesSettingsModel::selectedRole() const
{
    return m_currentRoleId;
}

QString QnUserRolesSettingsModel::roleName() const
{
    auto iter = currentRole();
    if (iter == m_roles.cend())
        return QString();
    return iter->name;
}

void QnUserRolesSettingsModel::setRoleName(const QString& value)
{
    auto iter = currentRole();
    if (iter == m_roles.end())
        return;

    int row = std::distance(m_roles.begin(), iter);
    QModelIndex idx = index(row);
    iter->name = value;
    emit dataChanged(idx, idx);
}

Qn::GlobalPermissions QnUserRolesSettingsModel::rawPermissions() const
{
    auto iter = currentRole();
    if (iter == m_roles.cend())
        return Qn::NoGlobalPermissions;
    return iter->permissions;
}

void QnUserRolesSettingsModel::setRawPermissions(Qn::GlobalPermissions value)
{
    auto iter = currentRole();
    if (iter == m_roles.end())
        return;
    iter->permissions = value;
}

QSet<QnUuid> QnUserRolesSettingsModel::accessibleResources() const
{
    return m_accessibleResources.value(m_currentRoleId);
}

QSet<QnUuid> QnUserRolesSettingsModel::accessibleResources(const ec2::ApiUserGroupData& role) const
{
    return m_accessibleResources.value(role.id);
}

int QnUserRolesSettingsModel::rowCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return static_cast<int>(m_roles.size());
}

QVariant QnUserRolesSettingsModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_roles.size())
        return QVariant();

    auto userRole = m_roles[index.row()];

    switch (role)
    {
    case Qt::DisplayRole:
    case Qt::AccessibleTextRole:
    case Qt::ToolTipRole:
    case Qt::StatusTipRole:
    case Qt::WhatsThisRole:
    case Qt::AccessibleDescriptionRole:
        return userRole.name;

    case Qt::DecorationRole:
        return qnResIconCache->icon(QnResourceIconCache::Users);

    case Qn::UuidRole:
        return qVariantFromValue(userRole.id);

    default:
        break;
    }
    return QVariant();
}

void QnUserRolesSettingsModel::setAccessibleResources(const QSet<QnUuid>& value)
{
    if (m_currentRoleId.isNull())
        return;

    m_accessibleResources[m_currentRoleId] = value;
}

QnResourceAccessSubject QnUserRolesSettingsModel::subject() const
{
    return qnUserRolesManager->userRole(m_currentRoleId);
}

ec2::ApiUserGroupDataList::iterator QnUserRolesSettingsModel::currentRole()
{
    if (m_currentRoleId.isNull())
        return m_roles.end();

    return std::find_if(m_roles.begin(), m_roles.end(), [this](const ec2::ApiUserGroupData& elem) { return elem.id == m_currentRoleId; });
}

ec2::ApiUserGroupDataList::const_iterator QnUserRolesSettingsModel::currentRole() const
{
    if (m_currentRoleId.isNull())
        return m_roles.cend();

    return std::find_if(m_roles.cbegin(), m_roles.cend(), [this](const ec2::ApiUserGroupData& elem) { return elem.id == m_currentRoleId; });
}

QnUserResourceList QnUserRolesSettingsModel::users(const QnUuid& roleId, bool withCandidates) const
{
    if (roleId.isNull())
        return QnUserResourceList();

    return qnResPool->getResources<QnUserResource>().filtered([this, roleId, withCandidates](const QnUserResourcePtr& user)
    {
        QnUuid userRole = user->userGroup();
        if (userRole == roleId)
            return true;

        if (!withCandidates)
            return false;

        return replacement(userRole).role == roleId;
    });
}

QnUserResourceList QnUserRolesSettingsModel::users(bool withCandidates) const
{
    return users(m_currentRoleId, withCandidates);
}

QnUserRolesSettingsModel::RoleReplacement QnUserRolesSettingsModel::replacement(const QnUuid& source) const
{
    RoleReplacement replacement = directReplacement(source);
    while (!replacement.role.isNull())
    {
        auto iterator = m_replacements.find(replacement.role);
        if (iterator != m_replacements.end())
            replacement = *iterator;
        else
            break;
    }

    return replacement;
}

QnUserRolesSettingsModel::RoleReplacement QnUserRolesSettingsModel::directReplacement(const QnUuid& source) const
{
    return m_replacements[source];
}
