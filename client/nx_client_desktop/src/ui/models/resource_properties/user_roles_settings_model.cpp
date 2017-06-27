#include "user_roles_settings_model.h"

#include <core/resource_access/shared_resources_manager.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>
#include <core/resource/user_resource.h>

#include <ui/style/globals.h>
#include <ui/style/resource_icon_cache.h>
#include <ui/style/skin.h>

#include <nx/utils/string.h>

QnUserRolesSettingsModel::UserRoleReplacement::UserRoleReplacement():
    UserRoleReplacement(QnUuid(), Qn::NoGlobalPermissions)
{
}

QnUserRolesSettingsModel::UserRoleReplacement::UserRoleReplacement(
    const QnUuid& userRoleId,
    Qn::GlobalPermissions permissions)
    :
    userRoleId(userRoleId),
    permissions(permissions)
{
}

bool QnUserRolesSettingsModel::UserRoleReplacement::isEmpty() const
{
    return userRoleId.isNull() && permissions == Qn::NoGlobalPermissions;
}

QnUserRolesSettingsModel::QnUserRolesSettingsModel(QObject* parent /*= nullptr*/):
    base_type(parent),
    m_currentUserRoleId(),
    m_userRoles()
{
    auto predefinedRoles = userRolesManager()->predefinedRoles();
    predefinedRoles << Qn::UserRole::CustomPermissions << Qn::UserRole::CustomUserRole;
    for (auto role : predefinedRoles)
        m_predefinedNames << userRolesManager()->userRoleName(role).trimmed().toLower();
}

QnUserRolesSettingsModel::~QnUserRolesSettingsModel()
{
}

ec2::ApiUserRoleDataList QnUserRolesSettingsModel::userRoles() const
{
    return m_userRoles;
}

void QnUserRolesSettingsModel::setUserRoles(const ec2::ApiUserRoleDataList& value)
{
    beginResetModel();

    m_userRoles = value;
    std::sort(m_userRoles.begin(), m_userRoles.end(),
        [](const ec2::ApiUserRoleData& l, const ec2::ApiUserRoleData& r)
        {
            // Case Insensitive sort.
            return nx::utils::naturalStringCompare(l.name, r.name, Qt::CaseInsensitive) < 0;
        });

    m_accessibleResources.clear();
    for (const auto& role : m_userRoles)
        m_accessibleResources[role.id] = sharedResourcesManager()->sharedResources(role);

    m_replacements.clear();

    endResetModel();
}

int QnUserRolesSettingsModel::addUserRole(const ec2::ApiUserRoleData& userRole)
{
    NX_ASSERT(!userRole.id.isNull());
    int row = static_cast<int>(m_userRoles.size());

    beginInsertRows(QModelIndex(), row, row);
    m_userRoles.push_back(userRole);
    endInsertRows();

    return row;
}

void QnUserRolesSettingsModel::removeUserRole(
    const QnUuid& userRoleId, const UserRoleReplacement& replacement)
{
    auto iter = std::find_if(m_userRoles.begin(), m_userRoles.end(),
        [userRoleId](const ec2::ApiUserRoleData& elem) { return elem.id == userRoleId; });
    int row = std::distance(m_userRoles.begin(), iter);

    beginRemoveRows(QModelIndex(), row, row);
    m_userRoles.erase(iter);
    m_replacements[userRoleId] = replacement;
    endRemoveRows();

    if (userRoleId == m_currentUserRoleId)
        m_currentUserRoleId = QnUuid();
}

void QnUserRolesSettingsModel::selectUserRoleId(const QnUuid& value)
{
    m_currentUserRoleId = value;
}

QnUuid QnUserRolesSettingsModel::selectedUserRoleId() const
{
    return m_currentUserRoleId;
}

QString QnUserRolesSettingsModel::userRoleName() const
{
    auto iter = currentRole();
    if (iter == m_userRoles.cend())
        return QString();
    return iter->name;
}

void QnUserRolesSettingsModel::setUserRoleName(const QString& value)
{
    auto iter = currentRole();
    if (iter == m_userRoles.end())
        return;

    int row = std::distance(m_userRoles.begin(), iter);
    QModelIndex idx = index(row);
    iter->name = value;
    emit dataChanged(idx, idx);
}

bool QnUserRolesSettingsModel::isUserRoleValid(const ec2::ApiUserRoleData& userRole) const
{
    const auto name = userRole.name.trimmed().toLower();
    if (name.isEmpty())
        return false;

    if (m_predefinedNames.contains(name))
        return false;

    using boost::algorithm::any_of;
    return !any_of(m_userRoles,
        [&](const ec2::ApiUserRoleData& other)
        {
            return other.id != userRole.id
                && other.name.trimmed().toLower() == name;
        });
}

bool QnUserRolesSettingsModel::isValid() const
{
    using boost::algorithm::all_of;
    return all_of(m_userRoles,
        [this](const ec2::ApiUserRoleData& role) { return isUserRoleValid(role); });
}

Qn::GlobalPermissions QnUserRolesSettingsModel::rawPermissions() const
{
    auto iter = currentRole();
    if (iter == m_userRoles.cend())
        return Qn::NoGlobalPermissions;
    return iter->permissions;
}

void QnUserRolesSettingsModel::setRawPermissions(Qn::GlobalPermissions value)
{
    auto iter = currentRole();
    if (iter == m_userRoles.end())
        return;
    iter->permissions = value;
}

QSet<QnUuid> QnUserRolesSettingsModel::accessibleResources() const
{
    return m_accessibleResources.value(m_currentUserRoleId);
}

QSet<QnUuid> QnUserRolesSettingsModel::accessibleResources(
    const ec2::ApiUserRoleData& userRole) const
{
    return m_accessibleResources.value(userRole.id);
}

int QnUserRolesSettingsModel::rowCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return static_cast<int>(m_userRoles.size());
}

QVariant QnUserRolesSettingsModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || (size_t) index.row() >= m_userRoles.size())
        return QVariant();

    auto userRole = m_userRoles[index.row()];

    switch (role)
    {
    case Qt::DisplayRole:
    case Qt::AccessibleTextRole:
    case Qt::ToolTipRole:
    case Qt::StatusTipRole:
    case Qt::WhatsThisRole:
    case Qt::AccessibleDescriptionRole:
        return userRole.name;

    case Qt::TextColorRole:
        return isUserRoleValid(userRole)
            ? QVariant()
            : QBrush(qnGlobals->errorTextColor());

    case Qt::DecorationRole:
        return isUserRoleValid(userRole)
            ? qnResIconCache->icon(QnResourceIconCache::Users)
            : qnSkin->icon("tree/users_error.png");

    case Qn::UuidRole:
        return qVariantFromValue(userRole.id);

    default:
        break;
    }
    return QVariant();
}

void QnUserRolesSettingsModel::setAccessibleResources(const QSet<QnUuid>& value)
{
    if (m_currentUserRoleId.isNull())
        return;

    m_accessibleResources[m_currentUserRoleId] = value;
}

QnResourceAccessSubject QnUserRolesSettingsModel::subject() const
{
    return userRolesManager()->userRole(m_currentUserRoleId);
}

ec2::ApiUserRoleDataList::iterator QnUserRolesSettingsModel::currentRole()
{
    if (m_currentUserRoleId.isNull())
        return m_userRoles.end();

    return std::find_if(m_userRoles.begin(), m_userRoles.end(),
        [this](const ec2::ApiUserRoleData& elem) { return elem.id == m_currentUserRoleId; });
}

ec2::ApiUserRoleDataList::const_iterator QnUserRolesSettingsModel::currentRole() const
{
    if (m_currentUserRoleId.isNull())
        return m_userRoles.cend();

    return std::find_if(m_userRoles.cbegin(), m_userRoles.cend(),
        [this](const ec2::ApiUserRoleData& elem) { return elem.id == m_currentUserRoleId; });
}

QnUserResourceList QnUserRolesSettingsModel::users(
    const QnUuid& userRoleId, bool withCandidates) const
{
    if (userRoleId.isNull())
        return QnUserResourceList();

    return resourcePool()->getResources<QnUserResource>().filtered(
        [this, userRoleId, withCandidates](const QnUserResourcePtr& user)
        {
            QnUuid id = user->userRoleId();
            if (id == userRoleId)
                return true;

            if (!withCandidates)
                return false;

            return replacement(id).userRoleId == userRoleId;
        });
}

QnUserResourceList QnUserRolesSettingsModel::users(bool withCandidates) const
{
    return users(m_currentUserRoleId, withCandidates);
}

QnUserRolesSettingsModel::UserRoleReplacement QnUserRolesSettingsModel::replacement(
    const QnUuid& sourceUserRoleId) const
{
    UserRoleReplacement replacement = directReplacement(sourceUserRoleId);
    while (!replacement.userRoleId.isNull())
    {
        auto iterator = m_replacements.find(replacement.userRoleId);
        if (iterator != m_replacements.end())
            replacement = *iterator;
        else
            break;
    }

    return replacement;
}

QnUserRolesSettingsModel::UserRoleReplacement QnUserRolesSettingsModel::directReplacement(
    const QnUuid& sourceUserRoleId) const
{
    return m_replacements[sourceUserRoleId];
}
