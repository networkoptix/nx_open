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
#include <utils/common/scoped_painter_rollback.h>
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

    UserListModel(QnUserRolesModel* rolesModel, QObject* parent = nullptr):
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
                if (index.column() != IndicatorColumn)
                    return base_type::data(index, role);

                const auto user = this->user(index.row());
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

    bool isValid(const QModelIndex& index) const
    {
        const auto user = this->user(index.row());
        return user && (!m_userValidator || m_userValidator(user) == QValidator::Acceptable);
    }

    bool isChecked(const QModelIndex& index) const
    {
        const auto checkIndex = index.sibling(index.row(), UserListModel::CheckColumn);
        return checkIndex.data(Qt::CheckStateRole).toInt() == Qt::Checked;
    }

    QnUserResourcePtr user(int row) const
    {
        return index(row, 0).data(Qn::ResourceRole)
            .value<QnResourcePtr>().dynamicCast<QnUserResource>();
    }

    void setUserValidator(UserValidator userValidator)
    {
        m_userValidator = userValidator;
        columnsChanged(0, columnCount() - 1);
    }

private:
    void columnsChanged(int firstColumn, int lastColumn, const QVector<int> roles = {})
    {
        const int lastRow = rowCount() - 1;
        if (lastRow >= 0)
            emit dataChanged(index(0, firstColumn), index(lastRow, lastColumn), roles);
    }

    void updateIndicators()
    {
        columnsChanged(IndicatorColumn, IndicatorColumn, { Qt::DecorationRole });
    };

private:
    QnResourceListModel* const m_usersModel;
    QnUserRolesModel* const m_rolesModel;
    UserValidator m_userValidator;
};

class SubjectSelectionDialog::UserListDelegate: public QnResourceItemDelegate
{
    using base_type = QnResourceItemDelegate;

public:
    using base_type::base_type; //< Forward constructors.

protected:
    virtual void initStyleOption(QStyleOptionViewItem* option,
        const QModelIndex& index) const override
    {
        base_type::initStyleOption(option, index);
        if (index.data(Qn::ValidRole).toBool())
            return;

        if (index.column() == UserListModel::NameColumn)
        {
            const auto checkIndex = index.sibling(index.row(), UserListModel::CheckColumn);
            const bool checked = checkIndex.data(Qt::CheckStateRole).toInt() == Qt::Checked;
            const bool selected = option->state.testFlag(QStyle::State_Selected);

            static const QnIcon::SuffixesList errorSuffixes {
                { QnIcon::Selected, lit("error") },
                { QnIcon::Active, lit("error") } };

            option->icon = qnSkin->icon(lit("tree/user_alert.png"), QString(),
                (selected || !checked) ? nullptr : &errorSuffixes);
        }
    }
};

SubjectSelectionDialog::SubjectSelectionDialog(QWidget* parent, Qt::WindowFlags windowFlags):
    base_type(parent, windowFlags),
    ui(new Ui::SubjectSelectionDialog()),
    m_roles(new QnUserRolesModel(this,
        QnUserRolesModel::StandardRoleFlag
      | QnUserRolesModel::UserRoleFlag)),
    m_users(new UserListModel(m_roles, this)),
    m_userListDelegate(new UserListDelegate(this))
{
    ui->setupUi(this);

    m_roles->setCheckable(true);
    m_roles->setPredefinedRoleIdsEnabled(true);

    auto filterRoles = new QSortFilterProxyModel(m_roles);
    filterRoles->setFilterCaseSensitivity(Qt::CaseInsensitive);
    filterRoles->setFilterKeyColumn(QnUserRolesModel::NameColumn);
    filterRoles->setSourceModel(m_roles);
    ui->rolesTreeView->setModel(filterRoles);

    auto sortFilterUsers = new NaturalStringSortProxyModel(this);
    sortFilterUsers->setSortCaseSensitivity(Qt::CaseInsensitive);
    sortFilterUsers->setSourceModel(m_users);

    ui->usersTreeView->setModel(sortFilterUsers);
    ui->usersTreeView->sortByColumn(UserListModel::NameColumn, Qt::AscendingOrder);

    ui->rolesTreeView->setItemDelegate(new UserRoleSelectionDelegate(
        UserRoleSelectionDelegate::GetRoleStatus(), //TODO: #vkutin #3.1 Implement correct validation.
        this));

    ui->usersTreeView->setItemDelegate(m_userListDelegate);

    auto indicatorDelegate = new QnCustomizableItemDelegate(this);
    indicatorDelegate->setCustomSizeHint(
        [](const QStyleOptionViewItem& option, const QModelIndex& /*index*/)
        {
            return qnSkin->maximumSize(option.icon);
        });

    indicatorDelegate->setCustomPaint(
        [](QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& /*index*/)
        {
            option.widget->style()->drawPrimitive(QStyle::PE_PanelItemViewItem,
                &option, painter, option.widget);
            QnScopedPainterOpacityRollback opacityRollback(painter);
            const bool selected = option.state.testFlag(QStyle::State_Selected);
            if (selected)
                painter->setOpacity(painter->opacity() * style::Hints::kDisabledItemOpacity);
            option.icon.paint(painter, option.rect, Qt::AlignCenter,
                selected ? QIcon::Normal : QIcon::Disabled);
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

    showAllUsersChanged(ui->showAllUsers->isChecked());
    connect(ui->showAllUsers, &QPushButton::toggled,
        this, &SubjectSelectionDialog::showAllUsersChanged);

    const auto updateFilterText =
        [this, sortFilterUsers, filterRoles]()
        {
            sortFilterUsers->setFilterFixedString(ui->searchLineEdit->text());
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

void SubjectSelectionDialog::showAllUsersChanged(bool value)
{
    const auto acceptCustomUsers =
        [this](int sourceRow, const QModelIndex& sourceParent) -> bool
        {
            const auto index = m_users->sourceModel()->index(sourceRow, 0, sourceParent);
            const auto user = index.data(Qn::ResourceRole)
                .value<QnResourcePtr>().dynamicCast<QnUserResource>();
            return user && user->userRole() == Qn::UserRole::CustomPermissions;
        };

    m_users->setCustomFilterAcceptsRow(value
        ? UserListModel::AcceptPredicate()
        : acceptCustomUsers);

    ui->usersGroupBox->setTitle(value
        ? tr("Users")
        : tr("Custom Users"));

    m_userListDelegate->setCustomInfoLevel(value
        ? Qn::RI_FullInfo
        : Qn::RI_NameOnly);

    // Reduce layout flicker.
    ui->usersGroupBox->updateGeometry();
    ui->mainWidget->updateGeometry();
    updateGeometry();
    layout()->activate();
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
