// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_roles_settings_model.h"

#include <boost/algorithm/cxx11/all_of.hpp>
#include <boost/algorithm/cxx11/any_of.hpp>

#include <QtGui/QBrush>

#include <client/client_globals.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/shared_resources_manager.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>
#include <nx/utils/string.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>

using namespace nx::vms::client::desktop;

QnUserRolesSettingsModel::UserRoleReplacement::UserRoleReplacement():
    UserRoleReplacement(QnUuid(), GlobalPermission::none)
{
}

QnUserRolesSettingsModel::UserRoleReplacement::UserRoleReplacement(
    const QnUuid& userRoleId,
    GlobalPermissions permissions)
    :
    userRoleId(userRoleId),
    permissions(permissions)
{
}

bool QnUserRolesSettingsModel::UserRoleReplacement::isEmpty() const
{
    return userRoleId.isNull() && permissions == 0;
}

QnUserRolesSettingsModel::QnUserRolesSettingsModel(QObject* parent /*= nullptr*/):
    base_type(parent),
    m_currentUserRoleId(),
    m_userRoles()
{
    auto predefinedRoles = userRolesManager()->predefinedRoles();
    predefinedRoles << Qn::UserRole::customPermissions << Qn::UserRole::customUserRole;
    for (auto role : predefinedRoles)
        m_predefinedNames << userRolesManager()->userRoleName(role).trimmed().toLower();
}

QnUserRolesSettingsModel::~QnUserRolesSettingsModel()
{
}

QnUserRolesSettingsModel::UserRoleDataList QnUserRolesSettingsModel::userRoles() const
{
    return m_userRoles;
}

void QnUserRolesSettingsModel::setUserRoles(const UserRoleDataList& value)
{
    beginResetModel();

    m_userRoles = value;
    std::sort(m_userRoles.begin(), m_userRoles.end(),
        [](const UserRoleData& l, const UserRoleData& r)
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

int QnUserRolesSettingsModel::addUserRole(const UserRoleData& userRole)
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
        [userRoleId](const UserRoleData& elem) { return elem.id == userRoleId; });
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

bool QnUserRolesSettingsModel::isUserRoleValid(const UserRoleData& userRole) const
{
    const auto name = userRole.name.trimmed().toLower();
    if (name.isEmpty())
        return false;

    if (m_predefinedNames.contains(name))
        return false;

    using boost::algorithm::any_of;
    return !any_of(m_userRoles,
        [&](const UserRoleData& other)
        {
            return other.id != userRole.id
                && other.name.trimmed().toLower() == name;
        });
}

bool QnUserRolesSettingsModel::isValid() const
{
    using boost::algorithm::all_of;
    return all_of(m_userRoles,
        [this](const UserRoleData& role) { return isUserRoleValid(role); });
}

GlobalPermissions QnUserRolesSettingsModel::rawPermissions() const
{
    auto iter = currentRole();
    if (iter == m_userRoles.cend())
        return {};
    return iter->permissions;
}

void QnUserRolesSettingsModel::setRawPermissions(GlobalPermissions value)
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
    const UserRoleData& userRole) const
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

    case Qt::ForegroundRole:
        return isUserRoleValid(userRole)
            ? QVariant()
            : QBrush(colorTheme()->color("red_l2"));

    case Qt::DecorationRole:
        return isUserRoleValid(userRole)
            ? qnResIconCache->icon(QnResourceIconCache::Users)
            : qnSkin->icon("tree/users.svg").pixmap(nx::style::Metrics::kDefaultIconSize, QnIcon::Error);

    case Qn::UuidRole:
        return QVariant::fromValue(userRole.id);

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

QnUserRolesSettingsModel::UserRoleDataList::iterator QnUserRolesSettingsModel::currentRole()
{
    if (m_currentUserRoleId.isNull())
        return m_userRoles.end();

    return std::find_if(m_userRoles.begin(), m_userRoles.end(),
        [this](const UserRoleData& elem) { return elem.id == m_currentUserRoleId; });
}

QnUserRolesSettingsModel::UserRoleDataList::const_iterator
    QnUserRolesSettingsModel::currentRole() const
{
    if (m_currentUserRoleId.isNull())
        return m_userRoles.cend();

    return std::find_if(m_userRoles.cbegin(), m_userRoles.cend(),
        [this](const UserRoleData& elem) { return elem.id == m_currentUserRoleId; });
}

QnUserResourceList QnUserRolesSettingsModel::users(
    const QnUuid& userRoleId, bool withCandidates) const
{
    if (userRoleId.isNull())
        return QnUserResourceList();

    return resourcePool()->getResources<QnUserResource>().filtered(
        [this, userRoleId, withCandidates](const QnUserResourcePtr& user)
        {
            QnUuid id = user->firstRoleId();
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
