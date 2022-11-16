// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "subject_selection_dialog_p.h"

#include <algorithm>

#include <client/client_globals.h>
#include <client/client_meta_types.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <ui/models/user_roles_model.h>

namespace {

int countEnabledUsers(const std::unordered_set<QnResourceAccessSubject>& subjects)
{
    return std::count_if(subjects.cbegin(), subjects.cend(),
        [](const QnResourceAccessSubject& subject) -> bool
        {
            return subject.isUser() && subject.user()->isEnabled();
        });
}

} // namespace

namespace nx::vms::client::desktop {
namespace ui {
namespace subject_selection_dialog_private {

//-------------------------------------------------------------------------------------------------
// subject_selection_dialog_private::RoleListModel

RoleListModel::RoleListModel(QObject* parent):
    base_type(parent, StandardRoleFlag | UserRoleFlag)
{
    setHasCheckBoxes(true);
    setPredefinedRoleIdsEnabled(true);
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

    const auto roleId = index.data(Qn::UuidRole).value<QnUuid>();
    const auto iter = m_validationStates.find(roleId);

    if (iter != m_validationStates.end())
        return QVariant::fromValue(iter.value());

    const auto state = validateRole(roleId);
    m_validationStates[roleId] = state;
    return QVariant::fromValue(state);
}

QValidator::State RoleListModel::validateRole(const QnUuid& roleId) const
{
    if (m_roleValidator)
        return m_roleValidator(roleId);

    return m_userValidator
        ? validateUsers(resourceAccessSubjectsCache()->allUsersInRole(roleId))
        : QValidator::Acceptable;
}

QValidator::State RoleListModel::validateUsers(
    const std::unordered_set<QnResourceAccessSubject>& subjects) const
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

QSet<QnUuid> RoleListModel::checkedUsers() const
{
    QSet<QnUuid> checkedUsers;
    for (const auto& roleId: checkedRoles())
    {
        for (const auto& subject: resourceAccessSubjectsCache()->allUsersInRole(roleId))
        {
            NX_ASSERT(subject.isUser());
            if (const auto& user = subject.user())
                checkedUsers.insert(user->getId());
        }
    }

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
    m_usersModel(new QnResourceListModel(this)),
    m_rolesModel(rolesModel)
{
    NX_ASSERT(rolesModel);

    connect(m_rolesModel, &QnUserRolesModel::modelReset,
        this, &UserListModel::updateIndicators);

    connect(m_rolesModel, &QnUserRolesModel::dataChanged,
        this, &UserListModel::updateIndicators);

    m_usersModel->setHasCheckboxes(true);
    m_usersModel->setResources(resourcePool()->getResources<QnUserResource>());

    connect(resourcePool(), &QnResourcePool::resourceAdded, this,
        [this](const QnResourcePtr& resource)
        {
            if (m_usersModel->resources().contains(resource))
                return;

            if (auto user = resource.dynamicCast<QnUserResource>())
                m_usersModel->addResource(resource);
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

QSet<QnUuid> UserListModel::checkedUsers() const
{
    return m_usersModel->checkedResources();
}

void UserListModel::setCheckedUsers(const QSet<QnUuid>& ids)
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
    const auto id = userRolesManager()->predefinedRoleId(Qn::UserRole::customPermissions);
    return countEnabledUsers(resourceAccessSubjectsCache()->allUsersInRole(id)) > 0;
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
                ? QVariant(qnSkin->icon(lit("tree/users.svg")))
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

    if (m_customUsersOnly && user->userRole() != Qn::UserRole::customPermissions)
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

    const auto role = user->userRole();
    const auto roleId = role == Qn::UserRole::customUserRole
        ? user->firstRoleId()
        : QnUserRolesManager::predefinedRoleId(role);

    return m_rolesModel->checkedRoles().contains(roleId);
}

QnUserResourcePtr UserListModel::getUser(const QModelIndex& index)
{
    return index.sibling(index.row(), NameColumn)
        .data(Qn::ResourceRole).value<QnResourcePtr>().dynamicCast<QnUserResource>();
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
// subject_selection_dialog_private::RoleListDelegate

RoleListDelegate::RoleListDelegate(QObject* parent):
    base_type(parent)
{
    setOptions(HighlightChecked | ValidateOnlyChecked);
    setCheckBoxColumn(RoleListModel::CheckColumn);
}

RoleListDelegate::~RoleListDelegate()
{
}

void RoleListDelegate::initStyleOption(QStyleOptionViewItem* option,
    const QModelIndex& index) const
{
    base_type::initStyleOption(option, index);
    if (index.column() == QnUserRolesModel::NameColumn)
    {
        const auto validationState = index.data(Qn::ValidationStateRole).value<QValidator::State>();
        option->icon = validationState == QValidator::Acceptable
            ? qnSkin->icon(lit("tree/users.svg"))
            : qnSkin->icon(lit("tree/users_alert.svg"));

        option->decorationSize = Skin::maximumSize(option->icon);
        option->features |= QStyleOptionViewItem::HasDecoration;
    }
}

void RoleListDelegate::getDisplayInfo(const QModelIndex& index,
    QString& baseName, QString& extInfo) const
{
    static const auto kExtraInfoTemplate = QString::fromWCharArray(L"\x2013 %1"); //< "- %1"
    const auto roleId = index.data(Qn::UuidRole).value<QnUuid>();
    const int usersInRole = countEnabledUsers(resourceAccessSubjectsCache()->allUsersInRole(roleId));
    baseName = userRolesManager()->userRoleName(roleId);
    extInfo = kExtraInfoTemplate.arg(tr("%n Users", "", usersInRole));
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
        option->icon = qnSkin->icon(lit("tree/user_alert.svg"));
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
