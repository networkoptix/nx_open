// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "subject_selection_dialog_p.h"

#include <algorithm>

#include <QtCore/QMap>

#include <client/client_globals.h>
#include <client/client_meta_types.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/resource_access_subject_hierarchy.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/unicode_chars.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/core/skin/svg_icon_colorer.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/user_management/predefined_user_groups.h>
#include <nx/vms/common/user_management/user_group_manager.h>
#include <nx/vms/common/user_management/user_management_helpers.h>
#include <ui/models/user_roles_model.h>

namespace {

int countEnabledUsers(const auto& subjects)
{
    return std::count_if(subjects.cbegin(), subjects.cend(),
        [](const QnResourceAccessSubject& subject) -> bool
        {
            return subject.isUser() && subject.user()->isEnabled();
        });
}

bool isUserVisible(const QnUserResourcePtr& user)
{
    return !user->attributes().testFlag(nx::vms::api::UserAttribute::hidden);
}

static const nx::vms::client::core::SvgIconColorer::ThemeSubstitutions colorSubs = {
    {QnIcon::Normal, {.primary = "light10", .secondary="light4"}},
    {QnIcon::Selected, {.primary = "light4", .secondary="light1"}}};

NX_DECLARE_COLORIZED_ICON(kUserAlertIcon, "20x20/Solid/user_alert.svg",\
    colorSubs)
NX_DECLARE_COLORIZED_ICON(kGroupIcon, "20x20/Solid/group.svg",\
    colorSubs)
NX_DECLARE_COLORIZED_ICON(kGroupAlertIcon, "20x20/Solid/group_alert.svg",\
    colorSubs)
NX_DECLARE_COLORIZED_ICON(kGroupLdapIcon, "20x20/Solid/group_ldap.svg",\
    colorSubs)
NX_DECLARE_COLORIZED_ICON(kGroupDefaultIcon, "20x20/Solid/group_default.svg",\
    colorSubs)

} // namespace

namespace nx::vms::client::desktop {
namespace ui {
namespace subject_selection_dialog_private {

//-------------------------------------------------------------------------------------------------
// subject_selection_dialog_private::RoleListModel

RoleListModel::RoleListModel(SystemContext* systemContext, QObject* parent):
    base_type(parent, StandardRoleFlag | UserRoleFlag),
    SystemContextAware(systemContext)
{
    setHasCheckBoxes(true);
}

void RoleListModel::setRoleValidator(RoleValidator roleValidator)
{
    m_roleValidator = roleValidator;
    m_validationStates.clear();
    emitDataChanged();
}

void RoleListModel::setUserValidator(UserValidator userValidator)
{
    m_userValidator = userValidator;
    if (!m_roleValidator)
    {
        m_validationStates.clear();
        emitDataChanged();
    }
}

void RoleListModel::emitDataChanged()
{
    const int lastRow = rowCount() - 1;
    if (lastRow >= 0)
    {
        emit dataChanged(
            index(0, 0),
            index(lastRow, columnCount() - 1));
    }
}

QVariant RoleListModel::data(const QModelIndex& index, int role) const
{
    if (m_allUsers && role == Qt::CheckStateRole && index.column() == CheckColumn)
        return QVariant::fromValue<int>(Qt::Checked);

    if (role != Qn::ValidationStateRole)
        return base_type::data(index, role);

    if (!index.isValid() || index.model() != this)
        return QVariant();

    if (!m_roleValidator && !m_userValidator)
        return QVariant::fromValue(QValidator::Acceptable);

    const auto roleId = index.data(core::UuidRole).value<nx::Uuid>();
    const auto iter = m_validationStates.find(roleId);

    if (iter != m_validationStates.end())
        return QVariant::fromValue(iter.value());

    const auto state = validateRole(roleId);
    m_validationStates[roleId] = state;
    return QVariant::fromValue(state);
}

QValidator::State RoleListModel::validateRole(const nx::Uuid& roleId) const
{
    if (m_roleValidator)
        return m_roleValidator(roleId);

    const auto users = systemContext()->accessSubjectHierarchy()->usersInGroups({roleId});
    std::vector<QnResourceAccessSubject> subjects{users.cbegin(), users.cend()};

    return m_userValidator
        ? validateUsers(std::move(subjects))
        : QValidator::Acceptable;
}

QValidator::State RoleListModel::validateUsers(
    std::vector<QnResourceAccessSubject> subjects) const
{
    if (!m_userValidator)
        return QValidator::Acceptable;

    enum Composition
    {
        noUsers = 0,
        validUsers = 0x1,
        invalidUsers = 0x2,
        mixedUsers = validUsers | invalidUsers
    };

    int composition = noUsers;

    for (const auto& subject: subjects)
    {
        if (!subject.isUser() || !subject.user()->isEnabled())
            continue;
        composition |= (m_userValidator(subject.user()) ? validUsers : invalidUsers);
        if (composition == mixedUsers)
            break; //< No need to iterate further.
    }

    switch (composition)
    {
        case noUsers:
        case validUsers:
            return QValidator::Acceptable;

        case invalidUsers:
            return QValidator::Invalid;

        default: // mixedUsers
            return QValidator::Intermediate;
    }
}

QSet<nx::Uuid> RoleListModel::checkedUsers() const
{
    QSet<nx::Uuid> checkedUsers;
    for (const auto& user: systemContext()->accessSubjectHierarchy()->usersInGroups(checkedRoles()))
        checkedUsers.insert(user->getId());

    return checkedUsers;
}

bool RoleListModel::allUsers() const
{
    return m_allUsers;
}

void RoleListModel::setAllUsers(bool value)
{
    if (m_allUsers == value)
        return;

    m_allUsers = value;
    emitDataChanged();
}

//-------------------------------------------------------------------------------------------------
// subject_selection_dialog_private::UserListModel

UserListModel::UserListModel(
    RoleListModel* rolesModel, QObject* parent)
    :
    base_type(parent),
    SystemContextAware(rolesModel->systemContext()),
    m_usersModel(new QnResourceListModel(this)),
    m_rolesModel(rolesModel)
{
    connect(m_rolesModel, &QnUserRolesModel::modelReset,
        this, &UserListModel::updateIndicators);

    connect(m_rolesModel, &QnUserRolesModel::dataChanged,
        this, &UserListModel::updateIndicators);

    m_usersModel->setHasCheckboxes(true);
    m_usersModel->setResources(
        resourcePool()->getResources<QnUserResource>(&isUserVisible));

    connect(resourcePool(), &QnResourcePool::resourceAdded, this,
        [this](const QnResourcePtr& resource)
        {
            if (auto user = resource.dynamicCast<QnUserResource>())
            {
                if (isUserVisible(user) && !m_usersModel->resources().contains(user))
                    m_usersModel->addResource(user);
            }
        });

    connect(resourcePool(), &QnResourcePool::resourceRemoved,
        m_usersModel, &QnResourceListModel::removeResource);

    static const QVector<int> kSourceColumns =
        []()
        {
            QVector<int> result(ColumnCount, -1);
            result[NameColumn] = QnResourceListModel::NameColumn;
            result[CheckColumn] = QnResourceListModel::CheckColumn;
            return result;
        }();

    auto indicatorColumnModel = new ColumnRemapProxyModel(kSourceColumns, this);
    indicatorColumnModel->setSourceModel(m_usersModel);

    setSourceModel(indicatorColumnModel);
    setFilterCaseSensitivity(Qt::CaseInsensitive);
    setFilterKeyColumn(NameColumn);
}

QSet<nx::Uuid> UserListModel::checkedUsers() const
{
    return m_usersModel->checkedResources();
}

void UserListModel::setCheckedUsers(const QSet<nx::Uuid>& ids)
{
    m_usersModel->setCheckedResources(ids);
}

bool UserListModel::customUsersOnly() const
{
    return m_customUsersOnly;
}

void UserListModel::setCustomUsersOnly(bool value)
{
    if (m_customUsersOnly == value)
        return;

    m_customUsersOnly = value;
    invalidateFilter();
}

bool UserListModel::systemHasCustomUsers() const
{
    // TODO: #vkutin Rethink the meaning of custom users in this model.
    // Is it still viable? Or we need to detect users with customized access rights?

    const auto customUsers = resourcePool()->getResources<QnUserResource>(
        [](const QnUserResourcePtr& user)
        {
            return isUserVisible(user) && user->groupIds().empty();
        });

    return !customUsers.empty();
}

Qt::ItemFlags UserListModel::flags(const QModelIndex& index) const
{
    return index.column() == IndicatorColumn
        ? base_type::flags(index.sibling(index.row(), NameColumn))
        : base_type::flags(index);
}

QVariant UserListModel::data(const QModelIndex& index, int role) const
{
    switch (role)
    {
        case Qt::CheckStateRole:
            return index.column() == CheckColumn && m_allUsers
                ? QVariant::fromValue<int>(Qt::Checked)
                : base_type::data(index, role);

        case Qn::ValidRole:
            return isValid(index);

        case Qt::DecorationRole:
        {
            if (index.column() != IndicatorColumn)
                return base_type::data(index, role);

            return !m_allUsers && isIndirectlyChecked(index)
                ? QVariant(qnSkin->icon(kGroupIcon))
                : QVariant();
        }

        default:
            return base_type::data(index, role);
    }
}

bool UserListModel::filterAcceptsRow(int sourceRow,
    const QModelIndex& sourceParent) const
{
    const auto user = getUser(sourceModel()->index(sourceRow, 0, sourceParent));
    if (!user || !user->isEnabled())
        return false;

    if (m_customUsersOnly && !user->groupIds().empty())
        return false;

    return base_type::filterAcceptsRow(sourceRow, sourceParent);
}

bool UserListModel::isValid(const QModelIndex& index) const
{
    const auto user = getUser(index);
    return user && (!m_userValidator || m_userValidator(user));
}

bool UserListModel::isChecked(const QModelIndex& index) const
{
    const auto checkIndex = index.sibling(index.row(), UserListModel::CheckColumn);
    return checkIndex.data(Qt::CheckStateRole).toInt() == Qt::Checked;
}

bool UserListModel::isIndirectlyChecked(const QModelIndex& index) const
{
    const auto user = getUser(index);
    if (!user)
        return false;

    const auto groups = nx::vms::common::userGroupsWithParents(user);

    return std::any_of(groups.begin(), groups.end(),
        [this](const nx::Uuid& groupId) { return m_rolesModel->checkedRoles().contains(groupId); });
}

QnUserResourcePtr UserListModel::getUser(const QModelIndex& index)
{
    return index.sibling(index.row(), NameColumn)
        .data(core::ResourceRole).value<QnResourcePtr>().dynamicCast<QnUserResource>();
}

void UserListModel::setUserValidator(UserValidator userValidator)
{
    m_userValidator = userValidator;
    columnsChanged(0, columnCount() - 1);
}

bool UserListModel::allUsers() const
{
    return m_allUsers;
}

void UserListModel::setAllUsers(bool value)
{
    if (m_allUsers == value)
        return;

    m_allUsers = value;
    columnsChanged(0, ColumnCount - 1);
}

void UserListModel::columnsChanged(int firstColumn, int lastColumn,
    const QVector<int> roles)
{
    const int lastRow = rowCount() - 1;
    if (lastRow >= 0)
        emit dataChanged(index(0, firstColumn), index(lastRow, lastColumn), roles);
}

void UserListModel::updateIndicators()
{
    columnsChanged(IndicatorColumn, IndicatorColumn, { Qt::DecorationRole });
};

//-------------------------------------------------------------------------------------------------
// subject_selection_dialog_private::GroupListDelegate

GroupListDelegate::GroupListDelegate(SystemContext* systemContext, QObject* parent):
    base_type(parent),
    SystemContextAware(systemContext)
{
    setOptions(HighlightChecked | ValidateOnlyChecked);
    setCheckBoxColumn(RoleListModel::CheckColumn);
}

GroupListDelegate::~GroupListDelegate()
{
}

void GroupListDelegate::initStyleOption(QStyleOptionViewItem* option,
    const QModelIndex& index) const
{
    base_type::initStyleOption(option, index);
    if (index.column() == QnUserRolesModel::NameColumn)
    {
        const auto roleId = index.data(core::UuidRole).value<nx::Uuid>();
        const auto role = userGroupManager()->find(roleId).value_or(api::UserGroupData{});
        QString iconPath;
        const auto validationState =
            index.data(Qn::ValidationStateRole).value<QValidator::State>();
        if (validationState != QValidator::Acceptable)
            option->icon = qnSkin->icon(kGroupAlertIcon);
        else if (role.type == nx::vms::api::UserType::ldap)
            option->icon = qnSkin->icon(kGroupLdapIcon);
        else if (nx::vms::common::PredefinedUserGroups::contains(roleId))
            option->icon = qnSkin->icon(kGroupDefaultIcon);
        else
            option->icon = qnSkin->icon(kGroupIcon);

        option->decorationSize = core::Skin::maximumSize(option->icon);
        option->features |= QStyleOptionViewItem::HasDecoration;
    }
}

void GroupListDelegate::getDisplayInfo(const QModelIndex& index,
    QString& baseName, QString& extInfo) const
{
    const auto roleId = index.data(core::UuidRole).value<nx::Uuid>();
    const int usersInRole = countEnabledUsers(
        systemContext()->accessSubjectHierarchy()->usersInGroups({roleId}));
    baseName = userGroupManager()->find(roleId).value_or(api::UserGroupData{}).name;
    extInfo = QString("%1 %2").arg(nx::UnicodeChars::kEnDash, tr("%n Users", "", usersInRole));
}

//-------------------------------------------------------------------------------------------------
// subject_selection_dialog_private::UserListDelegate

UserListDelegate::UserListDelegate(QObject* parent):
    base_type(parent)
{
    setOptions(HighlightChecked | ValidateOnlyChecked);
    setCheckBoxColumn(UserListModel::CheckColumn);
}

void UserListDelegate::initStyleOption(
    QStyleOptionViewItem* option,
    const QModelIndex& index) const
{
    base_type::initStyleOption(option, index);
    if (index.column() == UserListModel::NameColumn && !index.data(Qn::ValidRole).toBool())
        option->icon = qnSkin->icon(kUserAlertIcon);
}

QnResourceItemDelegate::ItemState UserListDelegate::itemState(const QModelIndex& index) const
{
    auto model = qobject_cast<const UserListModel*>(index.model());
    NX_ASSERT(model);

    if (model->isIndirectlyChecked(index))
        return ItemState::selected;

    return base_type::itemState(index);
}

//-------------------------------------------------------------------------------------------------

} // namespace subject_selection_dialog_private
} // namespace ui
} // namespace nx::vms::client::desktop
