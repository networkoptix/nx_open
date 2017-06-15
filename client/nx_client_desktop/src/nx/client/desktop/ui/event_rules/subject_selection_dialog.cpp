#include "subject_selection_dialog.h"
#include <ui_subject_selection_dialog.h> //< generated file

#include <common/common_module_aware.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/user_roles_manager.h>
#include <core/resource_management/resource_pool.h>
#include <ui/common/indents.h>
#include <ui/delegates/resource_item_delegate.h>
#include <ui/delegates/customizable_item_delegate.h>
#include <ui/models/user_roles_model.h>
#include <ui/models/resource/resource_list_model.h>
#include <ui/style/custom_style.h>
#include <ui/style/globals.h>
#include <ui/style/skin.h>
#include <ui/widgets/common/snapped_scrollbar.h>
#include <nx/utils/string.h>

#include <nx/client/desktop/ui/common/column_remap_proxy_model.h>
#include <nx/client/desktop/ui/common/customizable_sort_filter_proxy_model.h>
#include <nx/client/desktop/ui/common/natural_string_sort_proxy_model.h>
#include <nx/client/desktop/ui/event_rules/user_role_selection_delegate.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {

class SubjectSelectionDialog::UserListModel:
    public CustomizableSortFilterProxyModel,
    public QnCommonModuleAware
{
    using base_type = CustomizableSortFilterProxyModel;

public:
    enum Columns
    {
        NameColumn,
        IndicatorColumn,
        CheckColumn,
        ColumnCount
    };

    UserListModel(QObject* parent = nullptr):
        base_type(parent),
        QnCommonModuleAware(parent),
        m_usersModel(new QnResourceListModel(this))
    {
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

    QSet<QnUuid> checkedUsers() const
    {
        return m_usersModel->checkedResources();
    }

    void setCheckedUsers(const QSet<QnUuid>& ids)
    {
        m_usersModel->setCheckedResources(ids);
    }

    virtual Qt::ItemFlags flags(const QModelIndex& index) const override
    {
        return index.column() == IndicatorColumn
            ? base_type::flags(index.sibling(index.row(), NameColumn))
            : base_type::flags(index);
    }

    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override
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
                if (index.column() == NameColumn)
                {
                    return base_type::data(index, role);
                    //TODO: #vkutin #3.1 Implement working coloring
                    //if (isValid(index))
                    //    return base_type::data(index, role);

                    //static const QnIcon::SuffixesList uncheckedSuffixes {
                    //    { QnIcon::Normal,   lit("") },
                    //    { QnIcon::Selected, lit("") },
                    //    { QnIcon::Disabled, lit("disabled") } };

                    //static const QnIcon::SuffixesList checkedSuffixes {
                    //    { QnIcon::Normal,   lit("error") },
                    //    { QnIcon::Selected, lit("selected") },
                    //    { QnIcon::Disabled, lit("disabled") } };

                    //return isChecked(index)
                    //    ? qnSkin->icon(lit("tree/user_alert.png"), QString(), &uncheckedSuffixes)
                    //    : qnSkin->icon(lit("tree/user_alert.png"), QString(), &checkedSuffixes);
                }

                if (index.column() != IndicatorColumn)
                    return base_type::data(index, role);

                const auto user = this->user(index.row());
                if (!user)
                    return QVariant();

                const auto role = user->userRole();
                const auto uuid = role == Qn::UserRole::CustomUserRole
                    ? user->userRoleId()
                    : QnUserRolesManager::predefinedRoleId(role);

                return m_roleSelected.value(uuid)
                    ? QVariant(qnSkin->icon(lit("tree/users.png")))
                    : QVariant();
            }

            default:
                return base_type::data(index, role);
        }
    }

    bool isValid(const QModelIndex& index) const
    {
        const auto user = this->user(index.row());
        return user && (!m_userValidator || m_userValidator(user) == QValidator::Acceptable);
    }

    bool isChecked(const QModelIndex& index) const
    {
        const auto checkIndex = index.sibling(index.row(), CheckColumn);
        return checkIndex.data(Qt::CheckStateRole).toInt() == Qt::Checked;
    }

    void roleStateChanged(const QnUuid& roleId, bool selected)
    {
        //TODO: #vkutin #3.1 Refactor this, link to QnUserRolesModel
        m_roleSelected[roleId] = selected;
    }

    QnUserResourcePtr user(int row) const
    {
        return index(row, 0).data(Qn::ResourceRole)
            .value<QnResourcePtr>().dynamicCast<QnUserResource>();
    }

    void setUserValidator(UserValidator userValidator)
    {
        m_userValidator = userValidator;

        const int numUsers = rowCount();
        if (numUsers)
        {
            emit dataChanged(
                createIndex(0, 0),
                createIndex(numUsers - 1, columnCount() - 1));
        }
    }

private:
    QHash<QnUuid, bool> m_roleSelected;
    QnResourceListModel* const m_usersModel;
    UserValidator m_userValidator;
};

SubjectSelectionDialog::SubjectSelectionDialog(QWidget* parent, Qt::WindowFlags windowFlags):
    base_type(parent, windowFlags),
    ui(new Ui::SubjectSelectionDialog()),
    m_roles(new QnUserRolesModel(this,
        QnUserRolesModel::StandardRoleFlag
      | QnUserRolesModel::UserRoleFlag)),
    m_users(new UserListModel(this))
{
    ui->setupUi(this);

    //TODO: #vkutin #3.1 Split this looooong constructor to a number of functions.

    m_roles->setCheckable(true);
    m_roles->setPredefinedRoleIdsEnabled(true);

    auto filterRoles = new QSortFilterProxyModel(m_roles);
    filterRoles->setFilterCaseSensitivity(Qt::CaseInsensitive);
    filterRoles->setFilterKeyColumn(QnUserRolesModel::NameColumn);
    filterRoles->setSourceModel(m_roles);
    ui->rolesTreeView->setModel(filterRoles);

    auto sortUsersModel = new NaturalStringSortProxyModel(this);
    sortUsersModel->setSortCaseSensitivity(Qt::CaseInsensitive);
    sortUsersModel->setSourceModel(m_users);

    ui->usersTreeView->setModel(sortUsersModel);
    ui->usersTreeView->sortByColumn(UserListModel::NameColumn, Qt::AscendingOrder);

    ui->rolesTreeView->setItemDelegate(new UserRoleSelectionDelegate(
        UserRoleSelectionDelegate::GetRoleStatus(), //TODO: #vkutin #3.1 Implement correct validation.
        this));

    auto userNameDelegate = new QnResourceItemDelegate(this);
    ui->usersTreeView->setItemDelegateForColumn(UserListModel::NameColumn, userNameDelegate);

    auto indicatorDelegate = new QnCustomizableItemDelegate(this);
    indicatorDelegate->setCustomSizeHint(
        [](const QStyleOptionViewItem& option, const QModelIndex& /*index*/)
        {
            return qnSkin->maximumSize(option.icon);
        });

    ui->usersTreeView->setItemDelegateForColumn(
        UserListModel::IndicatorColumn, indicatorDelegate);

    //TODO: #vkutin Space single- and batch-toggle!

    auto setupTreeView =
        [this](QnTreeView* treeView)
        {
            const QnIndents kIndents(1, 0);
            treeView->header()->setStretchLastSection(false);
            treeView->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
            treeView->header()->setSectionResizeMode(0, QHeaderView::Stretch);
            treeView->setProperty(style::Properties::kSideIndentation,
                QVariant::fromValue(kIndents));
            treeView->setIgnoreDefaultSpace(true);
        };

    setupTreeView(ui->rolesTreeView);
    setupTreeView(ui->usersTreeView);

    auto scrollBar = new QnSnappedScrollBar(ui->mainWidget);
    scrollBar->setUseItemViewPaddingWhenVisible(false);
    scrollBar->setUseMaximumSpace(true);
    ui->scrollArea->setVerticalScrollBar(scrollBar->proxyScrollBar());

    const auto updateFilter =
        [this, userNameDelegate](bool checked)
        {
            const auto acceptCustomUsers =
                [this](int sourceRow, const QModelIndex& sourceParent) -> bool
                {
                    const auto index = m_users->sourceModel()->index(sourceRow, 0, sourceParent);
                    const auto user = index.data(Qn::ResourceRole)
                        .value<QnResourcePtr>().dynamicCast<QnUserResource>();
                    return user && user->userRole() == Qn::UserRole::CustomPermissions;
                };

            m_users->setCustomFilterAcceptsRow(checked
                ? UserListModel::AcceptPredicate()
                : acceptCustomUsers);

            ui->usersGroupBox->setTitle(checked
                ? tr("Users")
                : tr("Custom Users"));

            userNameDelegate->setCustomInfoLevel(checked
                ? Qn::RI_FullInfo
                : Qn::RI_NameOnly);

            // Reduce layout flicker:
            ui->usersGroupBox->updateGeometry();
            ui->mainWidget->updateGeometry();
            updateGeometry();
            layout()->activate();
        };

    connect(ui->showAllUsers, &QPushButton::toggled, this, updateFilter);
    updateFilter(ui->showAllUsers->isChecked());

    const auto rolesChanged =
        [this](int first, int last)
        {
            for (int row = first; row <= last; ++row)
            {
                const auto index = m_roles->index(row, QnUserRolesModel::CheckColumn);
                const auto roleId = index.data(Qn::UuidRole).value<QnUuid>();
                const bool selected = index.data(Qt::CheckStateRole).toInt() == Qt::Checked;
                m_users->roleStateChanged(roleId, selected);
            }

            const int numUsers = m_users->rowCount();
            if (!numUsers)
                return;

            emit m_users->dataChanged(
                m_users->index(0, 0),
                m_users->index(numUsers - 1, UserListModel::ColumnCount - 1));
        };

    connect(m_roles, &QnUserRolesModel::modelReset, this,
        [this, rolesChanged]() { rolesChanged(0, m_roles->rowCount() - 1); });

    connect(m_roles, &QnUserRolesModel::dataChanged, this,
        [this, rolesChanged](const QModelIndex& topLeft, const QModelIndex& bottomRight)
        {
            if (bottomRight.column() >= QnUserRolesModel::CheckColumn
                && topLeft.column() <= QnUserRolesModel::CheckColumn
                && m_users->rowCount())
            {
                rolesChanged(topLeft.row(), bottomRight.row());
            }
        });

    rolesChanged(0, m_roles->rowCount() - 1);

    const auto updateFilterText =
        [this, filterRoles]()
        {
            m_users->setFilterFixedString(ui->searchLineEdit->text());
            filterRoles->setFilterFixedString(ui->searchLineEdit->text());
        };

    connect(ui->searchLineEdit, &QnSearchLineEdit::textChanged, this, updateFilterText);
    connect(ui->searchLineEdit, &QnSearchLineEdit::enterKeyPressed, this, updateFilterText);
    connect(ui->searchLineEdit, &QnSearchLineEdit::escKeyPressed, this,
        [this, updateFilterText]()
        {
            ui->searchLineEdit->clear();
            updateFilterText();
        });

    //TODO: #vkutin #3.1 Connect to resource watcher and update
    // user and role model rows when some user property changes externally.
}

SubjectSelectionDialog::~SubjectSelectionDialog()
{
}

void SubjectSelectionDialog::setUserValidator(UserValidator userValidator)
{
    m_users->setUserValidator(userValidator);
    //m_roleDelegate ->setUserValidator(userValidator); //TODO: #vkutin #3.1
}

void SubjectSelectionDialog::setCheckedSubjects(const QSet<QnUuid>& ids)
{
    QnUserResourceList users;
    QList<QnUuid> roleIds;
    userRolesManager()->usersAndRoles(ids, users, roleIds);

    QSet<QnUuid> userIds;
    bool nonCustomUsers = false;

    for (const auto& user: users)
    {
        userIds.insert(user->getId());
        nonCustomUsers = nonCustomUsers || user->userRole() != Qn::UserRole::CustomPermissions;
    }

    m_users->setCheckedUsers(userIds);
    m_roles->setCheckedRoles(roleIds.toSet());

    if (nonCustomUsers)
        ui->showAllUsers->setChecked(true);
}

QSet<QnUuid> SubjectSelectionDialog::checkedSubjects() const
{
    return m_users->checkedUsers().unite(m_roles->checkedRoles());
}

void SubjectSelectionDialog::showAlert(const QString& text)
{
    ui->alertBar->setText(text);
}

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
