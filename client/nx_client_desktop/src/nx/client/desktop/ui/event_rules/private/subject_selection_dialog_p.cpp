#include "subject_selection_dialog_p.h"

#include <algorithm>

#include <client/client_globals.h>
#include <client/client_meta_types.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>
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
    setHasCheckBoxes(true);
    setUserCheckable(false); //< Entire row clicks are handled instead.
    setPredefinedRoleIdsEnabled(true);
}

void SubjectSelectionDialog::RoleListModel::setRoleValidator(Qn::RoleValidator roleValidator)
{
    m_roleValidator = roleValidator;
    m_validationStates.clear();
    emitDataChanged();
}

void SubjectSelectionDialog::RoleListModel::setUserValidator(Qn::UserValidator userValidator)
{
    m_userValidator = userValidator;
    if (!m_roleValidator)
    {
        m_validationStates.clear();
        emitDataChanged();
    }
}

void SubjectSelectionDialog::RoleListModel::emitDataChanged()
{
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
    if (m_allUsers && role == Qt::CheckStateRole && index.column() == CheckColumn)
        return qVariantFromValue<int>(Qt::Checked);

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

    const auto state = validateRole(roleId);
    m_validationStates[roleId] = state;
    return qVariantFromValue(state);
}

QValidator::State SubjectSelectionDialog::RoleListModel::validateRole(const QnUuid& roleId) const
{
    if (m_roleValidator)
        return m_roleValidator(roleId);

    return m_userValidator
        ? validateUsers(resourceAccessSubjectsCache()->usersInRole(roleId))
        : QValidator::Acceptable;
}

QValidator::State SubjectSelectionDialog::RoleListModel::validateUsers(
    const QList<QnResourceAccessSubject>& subjects) const
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

bool SubjectSelectionDialog::RoleListModel::allUsers() const
{
    return m_allUsers;
}

void SubjectSelectionDialog::RoleListModel::setAllUsers(bool value)
{
    if (m_allUsers == value)
        return;

    m_allUsers = value;
    emitDataChanged();
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
    m_usersModel->setUserCheckable(false); //< Entire row clicks are handled instead.
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
        case Qt::CheckStateRole:
            return index.column() == CheckColumn && m_allUsers
                ? qVariantFromValue<int>(Qt::Checked)
                : base_type::data(index, role);

        case Qn::ValidRole:
            return isValid(index);

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

bool SubjectSelectionDialog::UserListModel::allUsers() const
{
    return m_allUsers;
}

void SubjectSelectionDialog::UserListModel::setAllUsers(bool value)
{
    if (m_allUsers == value)
        return;

    m_allUsers = value;
    columnsChanged(0, ColumnCount - 1);
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
    setOptions(HighlightChecked | ValidateOnlyChecked);
    setCheckBoxColumn(RoleListModel::CheckColumn);
}

SubjectSelectionDialog::RoleListDelegate::~RoleListDelegate()
{
}

void SubjectSelectionDialog::RoleListDelegate::initStyleOption(QStyleOptionViewItem* option,
    const QModelIndex& index) const
{
    base_type::initStyleOption(option, index);
    if (index.column() == QnUserRolesModel::NameColumn)
    {
        const auto validationState = index.data(Qn::ValidationStateRole).value<QValidator::State>();
        option->icon = validationState == QValidator::Acceptable
            ? qnSkin->icon(lit("tree/users.png"))
            : qnSkin->icon(lit("tree/users_alert.png"));

        option->decorationSize = QnSkin::maximumSize(option->icon);
        option->features |= QStyleOptionViewItem::HasDecoration;
    }
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

SubjectSelectionDialog::UserListDelegate::UserListDelegate(QObject* parent):
    base_type(parent)
{
    setOptions(HighlightChecked | ValidateOnlyChecked);
    setCheckBoxColumn(UserListModel::CheckColumn);
}

void SubjectSelectionDialog::UserListDelegate::initStyleOption(
    QStyleOptionViewItem* option,
    const QModelIndex& index) const
{
    base_type::initStyleOption(option, index);
    if (index.column() == UserListModel::NameColumn && !index.data(Qn::ValidRole).toBool())
        option->icon = qnSkin->icon(lit("tree/user_alert.png"));
}

//-------------------------------------------------------------------------------------------------

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
