// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_list_widget.h"
#include "ui_user_list_widget.h"

#include <algorithm>

#include <QtCore/QMap>
#include <QtCore/QSortFilterProxyModel>
#include <QtGui/QKeyEvent>
#include <QtWidgets/QMenu>
#include <QtWidgets/QStyledItemDelegate>

#include <client/client_settings.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resources_changes_manager.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/range_adapters.h>
#include <nx/vms/client/desktop/common/utils/item_view_hover_tracker.h>
#include <nx/vms/client/desktop/common/widgets/checkable_header_view.h>
#include <nx/vms/client/desktop/common/widgets/control_bars.h>
#include <nx/vms/client/desktop/common/widgets/snapped_scroll_bar.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <nx/vms/client/desktop/system_administration/models/user_list_model.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>
#include <nx/vms/client/desktop/ui/dialogs/force_secure_auth_dialog.h>
#include <nx/vms/client/desktop/ui/messages/resources_messages.h>
#include <ui/common/palette.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/dialogs/ldap_settings_dialog.h>
#include <ui/dialogs/ldap_users_dialog.h>
#include <ui/dialogs/resource_properties/user_settings_dialog.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/widgets/views/resource_list_view.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>
#include <utils/common/event_processors.h>
#include <utils/common/ldap.h>
#include <utils/common/scoped_painter_rollback.h>
#include <utils/math/color_transformations.h>

namespace nx::vms::client::desktop {

using namespace nx::vms::client::desktop::ui;

namespace {

static constexpr int kMaximumColumnWidth = 200;

} // namespace

// -----------------------------------------------------------------------------------------------
// UserListWidget::Delegate

class UserListWidget::Delegate: public QStyledItemDelegate
{
    using base_type = QStyledItemDelegate;
    Q_DECLARE_TR_FUNCTIONS(UserListWidget)

public:
    explicit Delegate(QObject* parent = nullptr):
        base_type(parent)
    {
    }

    virtual QSize sizeHint(
        const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        switch (index.column())
        {
            case UserListModel::UserTypeColumn:
                return Skin::maximumSize(index.data(Qt::DecorationRole).value<QIcon>());

            case UserListModel::IsCustomColumn:
                return Skin::maximumSize(index.data(Qt::DecorationRole).value<QIcon>())
                    + QSize(style::Metrics::kStandardPadding * 2, 0);

            default:
                return base_type::sizeHint(option, index);
        }
    }

    virtual void paint(QPainter* painter, const QStyleOptionViewItem& option,
        const QModelIndex& index) const override
    {
        // Determine item opacity based on user enabled state:
        QnScopedPainterOpacityRollback opacityRollback(painter);
        if (index.data(Qn::DisabledRole).toBool())
            painter->setOpacity(painter->opacity() * nx::style::Hints::kDisabledItemOpacity);

        // Paint right-aligned user type icon or left-aligned custom permissions icon:
        const bool isUserTypeColumn = index.column() == UserListModel::UserTypeColumn;
        if (isUserTypeColumn || index.column() == UserListModel::IsCustomColumn)
        {
            const auto icon = index.data(Qt::DecorationRole).value<QIcon>();
            if (icon.isNull())
                return;

            const auto horizontalAlignment = isUserTypeColumn ? Qt::AlignRight : Qt::AlignLeft;
            const qreal padding = isUserTypeColumn ? 0 : style::Metrics::kStandardPadding;

            const auto rect = QStyle::alignedRect(Qt::LeftToRight,
                horizontalAlignment | Qt::AlignVCenter,
                Skin::maximumSize(icon),
                option.rect.adjusted(padding, 0, -padding, 0));

            icon.paint(painter, rect);
            return;
        }

        base_type::paint(painter, option, index);
    }

protected:
    virtual void initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const
    {
        base_type::initStyleOption(option, index);

        option->checkState = index.siblingAtColumn(UserListModel::CheckBoxColumn).data(
            Qt::CheckStateRole).value<Qt::CheckState>();

        if (const bool disabledUser = index.data(Qn::DisabledRole).toBool())
        {
            option->palette.setColor(QPalette::Text, option->checkState == Qt::Unchecked
                ? colorTheme()->color("dark16")
                : colorTheme()->color("light14"));
        }
        else
        {
            option->palette.setColor(QPalette::Text, option->checkState == Qt::Unchecked
                ? colorTheme()->color("light10")
                : colorTheme()->color("light4"));
        }
    }
};

// -----------------------------------------------------------------------------------------------
// UserListWidget

class UserListWidget::Private: public QObject
{
    UserListWidget* const q;
    nx::utils::ImplPtr<Ui::UserListWidget> ui{new Ui::UserListWidget()};

public:
    UserListModel* const usersModel{new UserListModel(q)};
    SortedUserListModel* const sortModel{new SortedUserListModel(q)};
    CheckableHeaderView* const header{new CheckableHeaderView(UserListModel::CheckBoxColumn, q)};
    QAction* filterDigestAction = nullptr;

    ControlBar* const selectionControls{new ControlBar(q)};

    QPushButton* const enableSelectedButton{new QPushButton(tr("Enable"), selectionControls)};
    QPushButton* const disableSelectedButton{new QPushButton(tr("Disable"), selectionControls)};
    QPushButton* const deleteSelectedButton{new QPushButton(
        qnSkin->icon("text_buttons/trash.png"), tr("Delete"), selectionControls)};
    QPushButton* const forceSecureAuthButton{new QPushButton(tr("Force Secure Authentication"),
        selectionControls)};

public:
    explicit Private(UserListWidget* q);
    void setupUi();
    void loadData();
    void filterDigestUsers() { ui->filterButton->setCurrentAction(filterDigestAction); }

private:
    void createUser();
    void forceSecureAuth();

    bool enableUser(const QnUserResourcePtr& user, bool enabled);
    void setSelectedEnabled(bool enabled);
    void enableSelected() { setSelectedEnabled(true); }
    void disableSelected() { setSelectedEnabled(false); }
    void deleteSelected();

    void modelUpdated();
    void updateSelection();

    void handleHeaderCheckStateChanged(Qt::CheckState state);
    void handleUsersTableClicked(const QModelIndex& index);

    bool canDisableDigest(const QnUserResourcePtr& user) const;

    QnUserResourceList visibleUsers() const;
    QnUserResourceList visibleSelectedUsers() const;
};

UserListWidget::UserListWidget(QWidget* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    d(new Private(this))
{
    d->setupUi();
}

UserListWidget::~UserListWidget()
{
    // Required here for forward-declared scoped pointer destruction.
}

void UserListWidget::loadDataToUi()
{
    d->loadData();
}

void UserListWidget::applyChanges()
{
    const auto modelUsers = d->usersModel->users();
    QMap<QnUserResource::DigestSupport, QSet<QnUserResourcePtr>> usersToSave;
    QnUserResourceList usersToDelete;

    const auto digestSupport =
        [&](const QnUserResourcePtr& user)
        {
            if (user->isCloud())
                return QnUserResource::DigestSupport::keep;

            return d->usersModel->isDigestEnabled(user)
                ? QnUserResource::DigestSupport::enable
                : QnUserResource::DigestSupport::disable;
        };

    const auto userModified =
        [&](const QnUserResourcePtr& user)
        {
            usersToSave[digestSupport(user)].insert(user);
        };

    for (auto user: resourcePool()->getResources<QnUserResource>())
    {
        if (!modelUsers.contains(user))
        {
            usersToDelete << user;
            continue;
        }

        const bool enabled = d->usersModel->isUserEnabled(user);
        if (user->isEnabled() != enabled)
        {
            user->setEnabled(enabled);
            userModified(user);
        }

        if (!d->usersModel->isDigestEnabled(user) && user->shouldDigestAuthBeUsed())
            userModified(user);
    }

    for (const auto& [digestSupport, userSet]: nx::utils::constKeyValueRange(usersToSave))
        qnResourcesChangesManager->saveUsers(userSet.values(), digestSupport);

    // User still can press cancel on 'Confirm Remove' dialog.
    if (!usersToDelete.empty())
    {
        if (messages::Resources::deleteResources(this, usersToDelete))
        {
            qnResourcesChangesManager->deleteResources(usersToDelete, nx::utils::guarded(this,
                [this](bool /*success*/)
                {
                    setEnabled(true);
                    emit hasChangesChanged();
                }));

            setEnabled(false);
            emit hasChangesChanged();
        }
        else
        {
            d->usersModel->resetUsers(resourcePool()->getResources<QnUserResource>());
        }
    }
}

bool UserListWidget::hasChanges() const
{
    if (!isEnabled())
        return false;

    const auto users = resourcePool()->getResources<QnUserResource>();
    return std::any_of(users.cbegin(), users.cend(),
        [this, users = d->usersModel->users()](const QnUserResourcePtr& user)
        {
            return !users.contains(user)
                || user->isEnabled() != d->usersModel->isUserEnabled(user)
                || user->shouldDigestAuthBeUsed() != d->usersModel->isDigestEnabled(user);
        });
}

void UserListWidget::filterDigestUsers()
{
    d->filterDigestUsers();
}

// -----------------------------------------------------------------------------------------------
// UserListWidget::Private

UserListWidget::Private::Private(UserListWidget* q): q(q)
{
    sortModel->setDynamicSortFilter(true);
    sortModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    sortModel->setFilterKeyColumn(-1);
    sortModel->setSourceModel(usersModel);

    connect(sortModel, &QAbstractItemModel::rowsInserted, this, &Private::modelUpdated);
    connect(sortModel, &QAbstractItemModel::rowsRemoved, this, &Private::modelUpdated);
    connect(sortModel, &QAbstractItemModel::dataChanged, this, &Private::modelUpdated);
}

void UserListWidget::Private::setupUi()
{
    ui->setupUi(q);
    q->layout()->addWidget(selectionControls);

    ui->filterButton->menu()->addAction(
        tr("All users"),
        [this] { sortModel->setDigestFilter(std::nullopt); });

    filterDigestAction = ui->filterButton->menu()->addAction(
        tr("With enabled digest authentication"),
        [this] { sortModel->setDigestFilter(true); });

    ui->filterButton->setAdjustSize(true);
    ui->filterButton->setCurrentIndex(0);

    const auto hoverTracker = new ItemViewHoverTracker(ui->usersTable);

    ui->usersTable->setModel(sortModel);
    ui->usersTable->setHeader(header);
    ui->usersTable->setIconSize(QSize(36, 24));
    ui->usersTable->setItemDelegate(new UserListWidget::Delegate(q));

    header->setVisible(true);
    header->setHighlightCheckedIndicator(true);
    header->setMaximumSectionSize(kMaximumColumnWidth);
    header->setSectionResizeMode(QHeaderView::ResizeToContents);
    header->setSectionResizeMode(UserListModel::UserGroupsColumn, QHeaderView::Stretch);
    header->setSectionsClickable(true);

    connect(header, &CheckableHeaderView::checkStateChanged,
        this, &Private::handleHeaderCheckStateChanged);

    ui->usersTable->sortByColumn(UserListModel::LoginColumn, Qt::AscendingOrder);

    const auto scrollBar = new SnappedScrollBar(q->window());
    ui->usersTable->setVerticalScrollBar(scrollBar->proxyScrollBar());

    connect(ui->usersTable, &QAbstractItemView::clicked, this, &Private::handleUsersTableClicked);
    connect(ui->createUserButton, &QPushButton::clicked, this, &Private::createUser);

    enableSelectedButton->setFlat(true);
    disableSelectedButton->setFlat(true);
    deleteSelectedButton->setFlat(true);
    forceSecureAuthButton->setFlat(true);

    constexpr int kButtonBarHeight = 32;
    selectionControls->setFixedHeight(kButtonBarHeight);

    const auto buttonsLayout = selectionControls->horizontalLayout();
    buttonsLayout->setSpacing(16);
    buttonsLayout->addWidget(enableSelectedButton);
    buttonsLayout->addWidget(disableSelectedButton);
    buttonsLayout->addWidget(deleteSelectedButton);
    buttonsLayout->addWidget(forceSecureAuthButton);
    buttonsLayout->addStretch();

    connect(enableSelectedButton, &QPushButton::clicked, this, &Private::enableSelected);
    connect(disableSelectedButton, &QPushButton::clicked, this, &Private::disableSelected);
    connect(deleteSelectedButton, &QPushButton::clicked, this, &Private::deleteSelected);
    connect(forceSecureAuthButton, &QPushButton::clicked, this, &Private::forceSecureAuth);

    connect(ui->filterLineEdit, &SearchLineEdit::textChanged, this,
        [this](const QString& text) { sortModel->setFilterWildcard(text); });

    // By [Space] toggle checkbox:
    connect(ui->usersTable, &TreeView::spacePressed, this,
        [this](const QModelIndex& index)
        {
            handleUsersTableClicked(index.sibling(index.row(), UserListModel::CheckBoxColumn));
        });

    // By [Left] disable user, by [Right] enable user:
    installEventHandler(ui->usersTable, QEvent::KeyPress, this,
        [this](QObject* /*object*/, QEvent* event)
        {
            const int key = static_cast<QKeyEvent*>(event)->key();
            switch (key)
            {
                case Qt::Key_Left:
                case Qt::Key_Right:
                {
                    if (!ui->usersTable->currentIndex().isValid())
                        break;
                    const auto user = ui->usersTable->currentIndex().data(
                        Qn::UserResourceRole).value<QnUserResourcePtr>();
                    if (user)
                        enableUser(user, key == Qt::Key_Right);
                    break;
                }

                default:
                    break;
            }
        });

    setHelpTopic(q, Qn::SystemSettings_UserManagement_Help);
    setHelpTopic(enableSelectedButton, Qn::UserSettings_DisableUser_Help);
    setHelpTopic(disableSelectedButton, Qn::UserSettings_DisableUser_Help);

    // Cursor changes with hover:
    connect(hoverTracker, &ItemViewHoverTracker::itemEnter, this,
        [this](const QModelIndex& index)
        {
            if (!UserListModel::isInteractiveColumn(index.column()))
                ui->usersTable->setCursor(Qt::PointingHandCursor);
            else
                ui->usersTable->unsetCursor();
        });

    connect(hoverTracker, &ItemViewHoverTracker::itemLeave, this,
        [this]() { ui->usersTable->unsetCursor(); });

    updateSelection();
}

void UserListWidget::Private::modelUpdated()
{
    const auto users = visibleUsers();
    ui->usersTable->setColumnHidden(UserListModel::UserTypeColumn,
        !std::any_of(users.cbegin(), users.cend(),
            [](const QnUserResourcePtr& user) { return !user->isLocal(); }));

    const bool isEmptyModel = !sortModel->rowCount();
    ui->searchWidget->setCurrentWidget(isEmptyModel
        ? ui->nothingFoundPage
        : ui->usersPage);

    updateSelection();
}

void UserListWidget::Private::updateSelection()
{
    const auto users = visibleSelectedUsers();
    Qt::CheckState selectionState = Qt::Unchecked;

    const bool hasSelection = !users.isEmpty();
    if (hasSelection)
    {
        selectionState = users.size() == sortModel->rowCount()
            ? Qt::Checked
            : Qt::PartiallyChecked;
    }

    const auto canModifyUser =
        [this](const QnUserResourcePtr& user)
        {
            const auto requiredPermissions = Qn::WriteAccessRightsPermission | Qn::SavePermission;
            return q->accessController()->hasPermissions(user, requiredPermissions);
        };

    const bool canEnable = std::any_of(users.cbegin(), users.cend(),
        [this, canModifyUser](const QnUserResourcePtr& user)
        {
            return canModifyUser(user) && !usersModel->isUserEnabled(user);
        });

    const bool canDisable = std::any_of(users.cbegin(), users.cend(),
        [this, canModifyUser](const QnUserResourcePtr& user)
        {
            return canModifyUser(user) && usersModel->isUserEnabled(user);
        });

    const bool canDelete = std::any_of(users.cbegin(), users.cend(),
        [this](const QnUserResourcePtr& user)
        {
            return q->accessController()->hasPermissions(user, Qn::RemovePermission);
        });

    const auto canForceSecureAuth = std::any_of(users.cbegin(), users.cend(),
        [this](const QnUserResourcePtr& user)
        {
            return canDisableDigest(user) && usersModel->isDigestEnabled(user);
        });

    enableSelectedButton->setVisible(canEnable);
    disableSelectedButton->setVisible(canDisable);
    deleteSelectedButton->setVisible(canDelete);
    forceSecureAuthButton->setVisible(canForceSecureAuth);

    selectionControls->setDisplayed(canEnable || canDisable || canDelete || canForceSecureAuth);
    header->setCheckState(selectionState);

    q->update();
}

void UserListWidget::Private::forceSecureAuth()
{
    if (!ForceSecureAuthDialog::isConfirmed(q))
        return;

    const auto users = visibleSelectedUsers();
    for (const auto& user: users)
    {
        if (!canDisableDigest(user))
            continue;

        usersModel->setDigestEnabled(user, false);
    }

    emit q->hasChangesChanged();
    updateSelection();
}

void UserListWidget::Private::createUser()
{
    q->menu()->triggerIfPossible(action::NewUserAction,
        action::Parameters().withArgument(Qn::ParentWidgetRole, QPointer<QWidget>(q)));
}

bool UserListWidget::Private::enableUser(const QnUserResourcePtr& user, bool enabled)
{
    if (!q->accessController()->hasPermissions(user, Qn::WriteAccessRightsPermission))
        return false;

    usersModel->setUserEnabled(user, enabled);
    emit q->hasChangesChanged();

    return true;
}

void UserListWidget::Private::setSelectedEnabled(bool enabled)
{
    const auto users = visibleSelectedUsers();
    for (const auto& user: users)
        enableUser(user, enabled);
}

void UserListWidget::Private::deleteSelected()
{
    QnUserResourceList usersToDelete;
    const auto users = visibleSelectedUsers();
    for (const auto& user: users)
    {
        if (!q->accessController()->hasPermissions(user, Qn::RemovePermission))
            continue;

        usersModel->removeUser(user);
    }

    emit q->hasChangesChanged();
}

QnUserResourceList UserListWidget::Private::visibleUsers() const
{
    QnUserResourceList result;
    for (int row = 0; row < sortModel->rowCount(); ++row)
    {
        const QModelIndex index = sortModel->index(row, UserListModel::CheckBoxColumn);
        const auto user = index.data(Qn::UserResourceRole).value<QnUserResourcePtr>();
        if (user)
            result.push_back(user);
    }

    return result;
}

QnUserResourceList UserListWidget::Private::visibleSelectedUsers() const
{
    QnUserResourceList result;
    for (int row = 0; row < sortModel->rowCount(); ++row)
    {
        const QModelIndex index = sortModel->index(row, UserListModel::CheckBoxColumn);
        const bool checked = index.data(Qt::CheckStateRole).toInt() == Qt::Checked;
        if (!checked)
            continue;

        const auto user = index.data(Qn::UserResourceRole).value<QnUserResourcePtr>();
        if (user)
            result.push_back(user);
    }

    return result;
}

bool UserListWidget::Private::canDisableDigest(const QnUserResourcePtr& user) const
{
    return q->accessController()->hasPermissions(user, Qn::WriteDigestPermission);
}

void UserListWidget::Private::handleHeaderCheckStateChanged(Qt::CheckState state)
{
    const auto users = visibleUsers();
    for (const auto& user: users)
        usersModel->setCheckState(state, user);

    updateSelection();
}

void UserListWidget::Private::handleUsersTableClicked(const QModelIndex& index)
{
    const auto user = index.data(Qn::UserResourceRole).value<QnUserResourcePtr>();
    if (!user)
        return;

    switch (index.column())
    {
        case UserListModel::CheckBoxColumn:
        {
            const auto nextCheckState = index.data(Qt::CheckStateRole).toInt() == Qt::Checked
                ? Qt::Unchecked
                : Qt::Checked;

            usersModel->setCheckState(nextCheckState, user);
            break;
        }

        default:
        {
            q->menu()->trigger(action::UserSettingsAction, action::Parameters(user)
                .withArgument(Qn::FocusTabRole, QnUserSettingsDialog::SettingsPage)
                .withArgument(Qn::ForceRole, true)
                .withArgument(Qn::ParentWidgetRole, QPointer<QWidget>(q)));
            break;
        }
    }
}

void UserListWidget::Private::loadData()
{
    usersModel->resetUsers(q->resourcePool()->getResources<QnUserResource>());
    modelUpdated();
}

} // namespace nx::vms::client::desktop
