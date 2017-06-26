#include "subject_selection_dialog_p.h"

#include <algorithm>

#include <client/client_globals.h>
#include <client/client_meta_types.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>
#include <core/resource_access/resource_access_subjects_cache.h>
#include <nx/utils/uuid.h>
#include <ui/models/user_roles_model.h>
#include <ui/style/globals.h>
#include <ui/style/skin.h>

namespace {

int countEnabledUsers(const QList<QnResourceAccessSubject>& subjects)
{
    return std::count_if(subjects.cbegin(), subjects.cend(),
        [](const QnResourceAccessSubject& subject) -> bool
        {
            return subject.isUser() && subject.user()->isEnabled();
        });
}

} // namespace

namespace nx {
namespace client {
namespace desktop {
namespace ui {

//-------------------------------------------------------------------------------------------------
// SubjectSelectionDialog::RoleListModel

SubjectSelectionDialog::RoleListModel::RoleListModel(QObject* parent):
    base_type(parent, StandardRoleFlag | UserRoleFlag),
    QnCommonModuleAware(parent)
{
}

void SubjectSelectionDialog::RoleListModel::setUserValidator(Qn::UserValidator userValidator)
{
    m_userValidator = userValidator;
    m_validationStates.clear();

    const int lastRow = rowCount() - 1;
    if (lastRow >= 0)
    {
        emit dataChanged(
            index(0, 0),
            index(lastRow, columnCount() - 1));
    }
}

QVariant SubjectSelectionDialog::RoleListModel::data(const QModelIndex& index, int role) const
{
    if (role != Qn::ValidationStateRole)
        return base_type::data(index, role);

    if (!index.isValid() || index.model() != this)
        return QVariant();

    if (!m_userValidator)
        return qVariantFromValue(QValidator::Acceptable);

    const auto roleId = index.data(Qn::UuidRole).value<QnUuid>();
    const auto iter = m_validationStates.find(roleId);

    if (iter != m_validationStates.end())
        return qVariantFromValue(iter.value());

    const auto state = validate(roleId);
    m_validationStates[roleId] = state;
    return qVariantFromValue(state);
}

QValidator::State SubjectSelectionDialog::RoleListModel::validate(const QnUuid& roleId) const
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

    for (const auto& subject: resourceAccessSubjectsCache()->usersInRole(roleId))
    {
        NX_EXPECT(subject.isUser());
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

QSet<QnUuid> SubjectSelectionDialog::RoleListModel::checkedUsers() const
{
    QSet<QnUuid> checkedUsers;
    for (const auto& roleId: checkedRoles())
    {
        for (const auto& subject: resourceAccessSubjectsCache()->usersInRole(roleId))
        {
            NX_ASSERT(subject.isUser());
            if (const auto& user = subject.user())
                checkedUsers.insert(user->getId());
        }
    }

    return checkedUsers;
}

//-------------------------------------------------------------------------------------------------
// SubjectSelectionDialog::UserListModel

SubjectSelectionDialog::UserListModel::UserListModel(
    RoleListModel* rolesModel, QObject* parent)
    :
    base_type(parent),
    QnCommonModuleAware(parent),
    m_usersModel(new QnResourceListModel(this)),
    m_rolesModel(rolesModel)
{
    NX_ASSERT(rolesModel);

    connect(m_rolesModel, &QnUserRolesModel::modelReset,
        this, &UserListModel::updateIndicators);

    connect(m_rolesModel, &QnUserRolesModel::dataChanged,
        this, &UserListModel::updateIndicators);

    m_usersModel->setHasCheckboxes(true);
    m_usersModel->setUserCheckable(true);
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

QSet<QnUuid> SubjectSelectionDialog::UserListModel::checkedUsers() const
{
    return m_usersModel->checkedResources();
}

void SubjectSelectionDialog::UserListModel::setCheckedUsers(const QSet<QnUuid>& ids)
{
    m_usersModel->setCheckedResources(ids);
}

bool SubjectSelectionDialog::UserListModel::customUsersOnly() const
{
    return m_customUsersOnly;
}

void SubjectSelectionDialog::UserListModel::setCustomUsersOnly(bool value)
{
    if (m_customUsersOnly == value)
        return;

    m_customUsersOnly = value;
    invalidateFilter();
}

bool SubjectSelectionDialog::UserListModel::systemHasCustomUsers() const
{
    const auto id = userRolesManager()->predefinedRoleId(Qn::UserRole::CustomPermissions);
    return countEnabledUsers(resourceAccessSubjectsCache()->usersInRole(id)) > 0;
}

Qt::ItemFlags SubjectSelectionDialog::UserListModel::flags(const QModelIndex& index) const
{
    return index.column() == IndicatorColumn
        ? base_type::flags(index.sibling(index.row(), NameColumn))
        : base_type::flags(index);
}

QVariant SubjectSelectionDialog::UserListModel::data(const QModelIndex& index, int role) const
{
    switch (role)
    {
        case Qn::ValidRole:
            return isValid(index);

        case Qt::ForegroundRole:
            return isValid(index) || !isChecked(index)
                ? QVariant()
                : QBrush(qnGlobals->errorTextColor());

        case Qt::DecorationRole:
        {
            if (index.column() != IndicatorColumn)
                return base_type::data(index, role);

            const auto user = getUser(index);
            if (!user)
                return QVariant();

            const auto role = user->userRole();
            const auto roleId = role == Qn::UserRole::CustomUserRole
                ? user->userRoleId()
                : QnUserRolesManager::predefinedRoleId(role);

            return m_rolesModel->checkedRoles().contains(roleId)
                ? QVariant(qnSkin->icon(lit("tree/users.png")))
                : QVariant();
        }

        default:
            return base_type::data(index, role);
    }
}

bool SubjectSelectionDialog::UserListModel::filterAcceptsRow(int sourceRow,
    const QModelIndex& sourceParent) const
{
    const auto user = getUser(sourceModel()->index(sourceRow, 0, sourceParent));
    if (!user || !user->isEnabled())
        return false;

    if (m_customUsersOnly && user->userRole() != Qn::UserRole::CustomPermissions)
        return false;

    return base_type::filterAcceptsRow(sourceRow, sourceParent);
}

bool SubjectSelectionDialog::UserListModel::isValid(const QModelIndex& index) const
{
    const auto user = getUser(index);
    return user && (!m_userValidator || m_userValidator(user));
}

bool SubjectSelectionDialog::UserListModel::isChecked(const QModelIndex& index) const
{
    const auto checkIndex = index.sibling(index.row(), UserListModel::CheckColumn);
    return checkIndex.data(Qt::CheckStateRole).toInt() == Qt::Checked;
}

QnUserResourcePtr SubjectSelectionDialog::UserListModel::getUser(const QModelIndex& index)
{
    return index.data(Qn::ResourceRole).value<QnResourcePtr>().dynamicCast<QnUserResource>();
}

void SubjectSelectionDialog::UserListModel::setUserValidator(Qn::UserValidator userValidator)
{
    m_userValidator = userValidator;
    columnsChanged(0, columnCount() - 1);
}

void SubjectSelectionDialog::UserListModel::columnsChanged(int firstColumn, int lastColumn,
    const QVector<int> roles)
{
    const int lastRow = rowCount() - 1;
    if (lastRow >= 0)
        emit dataChanged(index(0, firstColumn), index(lastRow, lastColumn), roles);
}

void SubjectSelectionDialog::UserListModel::updateIndicators()
{
    columnsChanged(IndicatorColumn, IndicatorColumn, { Qt::DecorationRole });
};

//-------------------------------------------------------------------------------------------------
// SubjectSelectionDialog::RoleListDelegate

SubjectSelectionDialog::RoleListDelegate::RoleListDelegate(QObject* parent):
    base_type(parent),
    QnCommonModuleAware(parent)
{
}

SubjectSelectionDialog::RoleListDelegate::~RoleListDelegate()
{
}

void SubjectSelectionDialog::RoleListDelegate::initStyleOption(QStyleOptionViewItem* option,
    const QModelIndex& index) const
{
    base_type::initStyleOption(option, index);

    const bool checked = index.sibling(index.row(), QnUserRolesModel::CheckColumn)
        .data(Qt::CheckStateRole).toInt() == Qt::Checked;

    const auto validationState = index.data(Qn::ValidationStateRole).value<QValidator::State>();

    const bool red = checked
        && validationState == QValidator::Invalid
        && !option->state.testFlag(QStyle::State_Selected);

    if (red)
        option->state |= State_Error; //< Our constant, not Qt.

    if (index.column() == QnUserRolesModel::NameColumn)
    {
        option->icon = validationState == QValidator::Acceptable
            ? qnSkin->icon(lit("tree/users.png"))
            : qnSkin->icon(lit("tree/users_alert.png"));

        option->decorationSize = QnSkin::maximumSize(option->icon);
        option->features |= QStyleOptionViewItem::HasDecoration;
    }
}

SubjectSelectionDialog::RoleListDelegate::ItemState
    SubjectSelectionDialog::RoleListDelegate::itemState(const QModelIndex& index) const
{
    const auto checkIndex = index.sibling(index.row(), QnUserRolesModel::CheckColumn);
    return checkIndex.data(Qt::CheckStateRole).toInt() == Qt::Checked
        ? ItemState::selected
        : ItemState::normal;
}

void SubjectSelectionDialog::RoleListDelegate::getDisplayInfo(const QModelIndex& index,
    QString& baseName, QString& extInfo) const
{
    static const auto kExtraInfoTemplate = QString::fromWCharArray(L"\x2013 %1"); //< "- %1"
    const auto roleId = index.data(Qn::UuidRole).value<QnUuid>();
    const int usersInRole = countEnabledUsers(resourceAccessSubjectsCache()->usersInRole(roleId));
    baseName = userRolesManager()->userRoleName(roleId);
    extInfo = kExtraInfoTemplate.arg(tr("%n users", "", usersInRole));
}

//-------------------------------------------------------------------------------------------------
// SubjectSelectionDialog::UserListDelegate

void SubjectSelectionDialog::UserListDelegate::initStyleOption(
    QStyleOptionViewItem* option,
    const QModelIndex& index) const
{
    base_type::initStyleOption(option, index);
    if (index.data(Qn::ValidRole).toBool())
        return;

    if (index.column() == UserListModel::NameColumn)
    {
        const auto checkIndex = index.sibling(index.row(), UserListModel::CheckColumn);
        const bool checked = checkIndex.data(Qt::CheckStateRole).toInt() == Qt::Checked;
        const bool selected = option->state.testFlag(QStyle::State_Selected);

        if (!selected && checked)
            option->state |= State_Error; //< Our constant, not Qt.

        option->icon = qnSkin->icon(lit("tree/user_alert.png"));
    }
}

//-------------------------------------------------------------------------------------------------

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
