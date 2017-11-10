#include "subject_selection_dialog.h"
#include "private/subject_selection_dialog_p.h"
#include <ui_subject_selection_dialog.h> //< generated file

#include <QtGui/QStandardItemModel>

#include <common/common_module_aware.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/user_roles_manager.h>
#include <ui/common/indents.h>
#include <ui/style/helper.h>
#include <ui/style/globals.h>
#include <ui/style/nx_style.h>
#include <ui/style/skin.h>
#include <ui/widgets/common/snapped_scrollbar.h>
#include <utils/common/scoped_painter_rollback.h>
#include <nx/utils/string.h>

#include <nx/client/desktop/ui/common/item_view_utils.h>
#include <nx/client/desktop/common/models/natural_string_sort_proxy_model.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {

SubjectSelectionDialog::SubjectSelectionDialog(QWidget* parent, Qt::WindowFlags windowFlags):
    base_type(parent, windowFlags),
    ui(new Ui::SubjectSelectionDialog()),
    m_roles(new RoleListModel(this)),
    m_users(new UserListModel(m_roles, this)),
    m_roleListDelegate(new RoleListDelegate(this)),
    m_userListDelegate(new UserListDelegate(this))
{
    ui->setupUi(this);
    ui->nothingFoundLabel->setHidden(true);

    auto filterRoles = new QSortFilterProxyModel(m_roles);
    filterRoles->setFilterCaseSensitivity(Qt::CaseInsensitive);
    filterRoles->setFilterKeyColumn(QnUserRolesModel::NameColumn);
    filterRoles->setSourceModel(m_roles);
    ui->rolesTreeView->setModel(filterRoles);

    ui->usersTreeView->setModel(m_users);
    ui->usersTreeView->sortByColumn(UserListModel::NameColumn, Qt::AscendingOrder);

    ui->rolesTreeView->setItemDelegate(m_roleListDelegate);
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

    ui->allUsersCheckableLine->setUserCheckable(false); //< Entire row clicks are handled instead.

    ItemViewUtils::setupDefaultAutoToggle(ui->allUsersCheckableLine->view(),
        CheckableLineWidget::CheckColumn);

    ItemViewUtils::setupDefaultAutoToggle(ui->rolesTreeView, RoleListModel::CheckColumn);
    ItemViewUtils::setupDefaultAutoToggle(ui->usersTreeView, UserListModel::CheckColumn);

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

    const auto updateFilter =
        [this, filterRoles]()
        {
            const auto filter = ui->searchLineEdit->text().trimmed();
            m_users->setFilterFixedString(filter);
            filterRoles->setFilterFixedString(filter);
            ui->allUsersCheckableLine->setVisible(filter.isEmpty());
        };

    connect(ui->searchLineEdit, &QnSearchLineEdit::textChanged, this, updateFilter);
    connect(ui->searchLineEdit, &QnSearchLineEdit::enterKeyPressed, this, updateFilter);
    connect(ui->searchLineEdit, &QnSearchLineEdit::escKeyPressed, this,
        [this, updateFilter]()
        {
            ui->searchLineEdit->clear();
            updateFilter();
        });

    const auto connectToModelChanges =
        [this](QAbstractItemModel* model)
        {
            connect(model, &QAbstractItemModel::modelReset,
                this, &SubjectSelectionDialog::changed);
            connect(model, &QAbstractItemModel::rowsInserted,
                this, &SubjectSelectionDialog::changed);
            connect(model, &QAbstractItemModel::rowsRemoved,
                this, &SubjectSelectionDialog::changed);
            connect(model, &QAbstractItemModel::dataChanged,
                this, &SubjectSelectionDialog::changed);
        };

    connectToModelChanges(m_users);
    connectToModelChanges(m_roles);

    const auto updateVisibility =
        [this]()
        {
            const bool noRoles = !ui->rolesTreeView->model()->rowCount();
            const bool noUsers = !ui->usersTreeView->model()->rowCount();
            ui->rolesGroupBox->setHidden(noRoles);
            ui->usersGroupBox->setHidden(noUsers);
            ui->nothingFoundLabel->setVisible(noRoles && noUsers);
        };

    connect(ui->usersTreeView->model(), &QAbstractItemModel::rowsInserted, this, updateVisibility);
    connect(ui->usersTreeView->model(), &QAbstractItemModel::rowsRemoved, this, updateVisibility);
    connect(ui->usersTreeView->model(), &QAbstractItemModel::modelReset, this, updateVisibility);
    connect(ui->rolesTreeView->model(), &QAbstractItemModel::rowsInserted, this, updateVisibility);
    connect(ui->rolesTreeView->model(), &QAbstractItemModel::rowsRemoved, this, updateVisibility);
    connect(ui->rolesTreeView->model(), &QAbstractItemModel::modelReset, this, updateVisibility);

    // Customized top margin for panel content:
    static constexpr int kContentTopMargin = 8;
    QnNxStyle::setGroupBoxContentTopMargin(ui->usersGroupBox, kContentTopMargin);
    QnNxStyle::setGroupBoxContentTopMargin(ui->rolesGroupBox, kContentTopMargin);

    ui->allUsersCheckableLine->setText(tr("All Users"));
    setupTreeView(ui->allUsersCheckableLine->view());

    auto allUsersDelegate = new QnResourceItemDelegate(this);
    allUsersDelegate->setCheckBoxColumn(CheckableLineWidget::CheckColumn);
    allUsersDelegate->setOptions(
        QnResourceItemDelegate::HighlightChecked
      | QnResourceItemDelegate::ValidateOnlyChecked);

    ui->allUsersCheckableLine->view()->setItemDelegate(allUsersDelegate);

    const auto allUsersCheckStateChanged =
        [this](Qt::CheckState checkState)
        {
            const bool checked = checkState == Qt::Checked;
            ui->rolesGroupBox->setEnabled(!checked);
            ui->usersGroupBox->setEnabled(!checked);
            ui->searchLineEdit->setEnabled(!checked);
            if (checked)
                ui->searchLineEdit->clear();
            m_roles->setAllUsers(checked);
            m_users->setAllUsers(checked);
            emit changed();
        };

    connect(ui->allUsersCheckableLine, &CheckableLineWidget::checkStateChanged,
        this, allUsersCheckStateChanged);

    allUsersCheckStateChanged(ui->allUsersCheckableLine->checkState());
    validateAllUsers();
}

SubjectSelectionDialog::~SubjectSelectionDialog()
{
}

void SubjectSelectionDialog::showAllUsersChanged(bool value)
{
    m_users->setCustomUsersOnly(!value);

    ui->usersGroupBox->setVisible(value || m_users->systemHasCustomUsers());

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

void SubjectSelectionDialog::setUserValidator(Qn::UserValidator userValidator)
{
    m_users->setUserValidator(userValidator);
    m_roles->setUserValidator(userValidator);
    validateAllUsers();
}

void SubjectSelectionDialog::setRoleValidator(Qn::RoleValidator roleValidator)
{
    m_roles->setRoleValidator(roleValidator);
}

void SubjectSelectionDialog::validateAllUsers()
{
    const auto validationState = m_roles->validateUsers(
        resourceAccessSubjectsCache()->allSubjects());

    QIcon icon = (validationState == QValidator::Acceptable
        ? qnSkin->icon(lit("tree/users.png"))
        : qnSkin->icon(lit("tree/users_alert.png")));

    ui->allUsersCheckableLine->setIcon(icon);

    ui->allUsersCheckableLine->setData(qVariantFromValue(validationState),
        Qn::ValidationStateRole);
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

QSet<QnUuid> SubjectSelectionDialog::totalCheckedUsers() const
{
    if (!allUsers())
        return m_users->checkedUsers().unite(m_roles->checkedUsers());

    QSet<QnUuid> allUserIds;
    for (const auto& subject: resourceAccessSubjectsCache()->allSubjects())
    {
        if (const auto& user = subject.user())
            allUserIds.insert(user->getId());
    }

    return allUserIds;
}

bool SubjectSelectionDialog::allUsers() const
{
    return allUsersSelectorEnabled() && ui->allUsersCheckableLine->checked();
}

void SubjectSelectionDialog::setAllUsers(bool value)
{
    if (allUsersSelectorEnabled())
        ui->allUsersCheckableLine->setChecked(value);
}

bool SubjectSelectionDialog::allUsersSelectorEnabled() const
{
    return !ui->allUsersCheckableLine->isHidden();
}

void SubjectSelectionDialog::setAllUsersSelectorEnabled(bool value)
{
    const bool disabled = !value;
    ui->allUsersCheckableLine->setHidden(disabled);
    if (disabled)
        ui->allUsersCheckableLine->setChecked(false);
}

void SubjectSelectionDialog::showAlert(const QString& text)
{
    ui->alertBar->setText(text);
}

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
