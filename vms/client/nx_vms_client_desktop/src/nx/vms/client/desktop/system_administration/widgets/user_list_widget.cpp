// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ui_user_list_widget.h"

#include <algorithm>

#include <QtCore/QMap>
#include <QtCore/QSortFilterProxyModel>
#include <QtGui/QKeyEvent>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMenu>
#include <QtWidgets/QStyledItemDelegate>

#include <common/common_module.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/qt_helpers.h>
#include <nx/utils/range_adapters.h>
#include <nx/vms/api/data/ldap.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/access/access_controller.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/common/dialogs/qml_dialog_wrapper.h>
#include <nx/vms/client/desktop/common/utils/item_view_hover_tracker.h>
#include <nx/vms/client/desktop/common/widgets/checkable_header_view.h>
#include <nx/vms/client/desktop/common/widgets/control_bars.h>
#include <nx/vms/client/desktop/common/widgets/snapped_scroll_bar.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/resource/resources_changes_manager.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/system_administration/globals/user_group_request_chain.h>
#include <nx/vms/client/desktop/system_administration/models/user_list_model.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/dialogs/force_secure_auth_dialog.h>
#include <nx/vms/client/desktop/ui/messages/resources_messages.h>
#include <nx/vms/common/system_settings.h>
#include <ui/common/palette.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/widgets/views/resource_list_view.h>
#include <ui/workbench/workbench_context.h>
#include <utils/common/event_processors.h>
#include <utils/common/scoped_painter_rollback.h>
#include <utils/math/color_transformations.h>

#include "private/highlighted_text_item_delegate.h"
#include "user_list_widget.h"

namespace nx::vms::client::desktop {

using namespace nx::vms::client::desktop::ui;

namespace {

static constexpr int kMaximumColumnWidth = 200;

static const QMap<QIcon::Mode, nx::vms::client::core::SvgIconColorer::ThemeColorsRemapData>
    kTextButtonColors = {{QIcon::Normal, {"light4"}}, {QIcon::Active, {"light3"}}};

} // namespace

// -----------------------------------------------------------------------------------------------
// UserListWidget::Delegate

class UserListWidget::Delegate: public HighlightedTextItemDelegate
{
    using base_type = HighlightedTextItemDelegate;
    Q_DECLARE_TR_FUNCTIONS(UserListWidget)

public:
    explicit Delegate(QObject* parent = nullptr):
        base_type(parent, {
            UserListModel::LoginColumn,
            UserListModel::FullNameColumn,
            UserListModel::EmailColumn,
            UserListModel::UserGroupsColumn})
    {
    }

    virtual QSize sizeHint(
        const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        switch (index.column())
        {
            case UserListModel::UserWarningColumn:
            case UserListModel::UserTypeColumn:
                return core::Skin::maximumSize(index.data(Qt::DecorationRole).value<QIcon>());

            case UserListModel::IsCustomColumn:
                return core::Skin::maximumSize(index.data(Qt::DecorationRole).value<QIcon>())
                    + QSize(style::Metrics::kStandardPadding * 2, 0);

            default:
                return base_type::sizeHint(option, index);
        }
    }

    virtual void paint(QPainter* painter, const QStyleOptionViewItem& option,
        const QModelIndex& index) const override
    {
        auto opt = option;
        initStyleOption(&opt, index);

        // Determine item opacity based on user enabled state:
        QnScopedPainterOpacityRollback opacityRollback(painter);
        if (index.data(Qn::DisabledRole).toBool())
            painter->setOpacity(painter->opacity() * nx::style::Hints::kDisabledItemOpacity);

        // Paint right-aligned user type icon or left-aligned custom permissions icon:
        const bool isUserIconColumn = index.column() == UserListModel::UserTypeColumn
            || index.column() == UserListModel::UserWarningColumn;
        if (isUserIconColumn || index.column() == UserListModel::IsCustomColumn)
        {
            const auto icon = index.data(Qt::DecorationRole).value<QIcon>();
            if (icon.isNull())
                return;

            const auto horizontalAlignment = isUserIconColumn ? Qt::AlignCenter : Qt::AlignLeft;
            const qreal padding = isUserIconColumn ? 0 : style::Metrics::kStandardPadding;

            const auto rect = QStyle::alignedRect(Qt::LeftToRight,
                horizontalAlignment | Qt::AlignVCenter,
                core::Skin::maximumSize(icon),
                opt.rect.adjusted(padding, 0, -padding, 0));

            icon.paint(painter, rect, Qt::AlignCenter,
                opt.checkState == Qt::Checked ? QIcon::Selected : QIcon::Normal);
            return;
        }

        base_type::paint(painter, opt, index);
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
                ? core::colorTheme()->color("dark16")
                : core::colorTheme()->color("light14"));
        }
        else
        {
            option->palette.setColor(QPalette::Text, option->checkState == Qt::Unchecked
                ? core::colorTheme()->color("light10")
                : core::colorTheme()->color("light4"));
        }
    }
};

// -----------------------------------------------------------------------------------------------
// UserListWidget

class UserListWidget::Private: public QObject
{
    UserListWidget* const q;
    nx::utils::ImplPtr<Ui::UserListWidget> ui{new Ui::UserListWidget()};
    QSet<QnUserResourcePtr> m_visibleSelected;

public:
    UserListModel* const usersModel{new UserListModel(q)};
    SortedUserListModel* const sortModel{new SortedUserListModel(q)};
    CheckableHeaderView* const header{new CheckableHeaderView(UserListModel::CheckBoxColumn, q)};
    QAction* filterDigestAction = nullptr;

    ControlBar* const selectionControls{new ControlBar(q)};

    QPushButton* const deleteButton{new QPushButton(
        qnSkin->icon("text_buttons/delete_20.svg", kTextButtonColors),
        tr("Delete"),
        selectionControls)};

    QPushButton* const editButton{new QPushButton(
        qnSkin->icon("text_buttons/edit_20.svg", kTextButtonColors),
        tr("Edit"),
        selectionControls)};

    bool m_hasChanges = false;
    std::unique_ptr<UserGroupRequestChain> m_requestChain;

public:
    explicit Private(UserListWidget* q);
    void setupUi();
    void filterDigestUsers() { ui->filterButton->setCurrentAction(filterDigestAction); }

private:
    void createUser();
    void editUser(const QnUserResourcePtr& user);
    void deleteSelected();
    void editSelected();

    void modelUpdated();
    void updateSelection();

    void setupPlaceholder();

    void handleHeaderCheckStateChanged(Qt::CheckState state);
    void handleUsersTableClicked(const QModelIndex& index);

    bool canDelete(const QnUserResourcePtr& user) const;
    bool canDelete(const QnUserResourceList& users) const;
    bool canEnableDisable(const QnUserResourcePtr& user) const;
    bool canEnableDisable(const QnUserResourceList& users) const;
    bool canChangeAuthentication(const QnUserResourcePtr& user) const;
    bool canChangeAuthentication(const QnUserResourceList& users) const;

    QnUserResourceList visibleUsers() const;
    QnUserResourceList visibleSelectedUsers() const;

    void visibleAdded(int first, int last);
    void visibleAboutToBeRemoved(int first, int last);
    void visibleModified(int first, int last);
};

UserListWidget::UserListWidget(QWidget* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    d(new Private(this))
{
    d->setupUi();

    d->usersModel->resetUsers();
}

UserListWidget::~UserListWidget()
{
    // Required here for forward-declared scoped pointer destruction.
}

void UserListWidget::loadDataToUi()
{
    d->header->setCheckState(Qt::Unchecked);
}

void UserListWidget::applyChanges()
{
    d->m_requestChain.reset(new UserGroupRequestChain(systemContext()));
    for (auto user: resourcePool()->getResources<QnUserResource>())
    {
        UserGroupRequest::UpdateUser updateUser;

        const bool enabled = d->usersModel->isUserEnabled(user);
        if (user->isEnabled() != enabled)
        {
            updateUser.id = user->getId();
            updateUser.enabled = enabled;
        }
        else
        {
            updateUser.enabled = user->isEnabled();
        }

        if (!d->usersModel->isDigestEnabled(user) && user->shouldDigestAuthBeUsed())
        {
            updateUser.id = user->getId();
            updateUser.enableDigest = false;
        }

        if (!updateUser.id.isNull())
            d->m_requestChain->append(updateUser);
    }

    setEnabled(false);

    d->m_requestChain->start(
        [this](bool success, const QString& errorString)
        {
            setEnabled(true);
            d->m_hasChanges = false;
            emit hasChangesChanged();

            if (success)
                return;

            QnMessageBox messageBox(
                QnMessageBoxIcon::Critical,
                errorString,
                {},
                QDialogButtonBox::Ok,
                QDialogButtonBox::Ok,
                this);
            messageBox.exec();
        });
}

bool UserListWidget::hasChanges() const
{
    if (!isEnabled())
        return false;

    return d->m_hasChanges;
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
    sortModel->setSyncId(q->globalSettings()->ldap().syncId());

    connect(sortModel, &QAbstractItemModel::rowsInserted, this,
        [this](const QModelIndex&, int first, int last)
        {
            visibleAdded(first, last);
            modelUpdated();
        });

    connect(sortModel, &QAbstractItemModel::rowsAboutToBeRemoved, this,
        [this](const QModelIndex&, int first, int last)
        {
            visibleAboutToBeRemoved(first, last);
        });

    connect(sortModel, &QAbstractItemModel::rowsRemoved, this, &Private::modelUpdated);

    connect(sortModel, &QAbstractItemModel::dataChanged, this,
        [this](const QModelIndex& topLeft, const QModelIndex& bottomRight, const QList<int>&)
        {
            visibleModified(topLeft.row(), bottomRight.row());
            modelUpdated();
        });

    connect(sortModel, &QAbstractItemModel::modelReset, this,
        [this]()
        {
            m_hasChanges = false;
            emit this->q->hasChangesChanged();

            m_visibleSelected.clear();
            if (sortModel->rowCount() > 0)
                visibleAdded(0, sortModel->rowCount() - 1);

            modelUpdated();
        });

    connect(q->globalSettings(), &common::SystemSettings::ldapSettingsChanged, this,
        [this]() { sortModel->setSyncId(this->q->globalSettings()->ldap().syncId()); });
}

QSize UserListWidget::sizeHint() const
{
    // We need to provide some valid size because of resizeContentsPrecision == 0. Without a valid
    // sizeHint parent layout may expand the viewport widget which leads to performance issues.
    return QSize(200, 200);
}

void UserListWidget::Private::setupUi()
{
    ui->setupUi(q);
    q->layout()->addWidget(selectionControls);

    ui->filterButton->menu()->addAction(
        tr("All Users"),
        [this] { sortModel->setDigestFilter(std::nullopt); });

    filterDigestAction = ui->filterButton->menu()->addAction(
        tr("Users with Digest Authentication"),
        [this] { sortModel->setDigestFilter(true); });

    ui->filterButton->setAdjustSize(true);
    ui->filterButton->setCurrentIndex(0);

    const auto hoverTracker = new ItemViewHoverTracker(ui->usersTable);

    ui->usersTable->setModel(sortModel);
    ui->usersTable->setHeader(header);
    ui->usersTable->setIconSize(QSize(24, 24));

    auto delegate = new UserListWidget::Delegate(q);
    ui->usersTable->setItemDelegate(delegate);
    delegate->setView(ui->usersTable);

    header->setVisible(true);
    header->setHighlightCheckedIndicator(true);
    header->setMaximumSectionSize(kMaximumColumnWidth);
    header->setResizeContentsPrecision(0); //< Calculate resize using only the visible area.
    header->setSectionResizeMode(QHeaderView::ResizeToContents);
    header->setSectionResizeMode(UserListModel::UserGroupsColumn, QHeaderView::Stretch);
    header->setSectionsClickable(true);
    for (const auto column: {UserListModel::UserWarningColumn, UserListModel::UserTypeColumn})
    {
        header->setSectionResizeMode(column, QHeaderView::Fixed);
        header->setAlignment(column, Qt::AlignHCenter);
    }
    header->resizeSection(UserListModel::UserWarningColumn, 20);
    header->resizeSection(UserListModel::UserTypeColumn, 30);

    connect(header, &CheckableHeaderView::checkStateChanged,
        this, &Private::handleHeaderCheckStateChanged);

    ui->usersTable->sortByColumn(UserListModel::LoginColumn, Qt::AscendingOrder);

    const auto scrollBar = new SnappedScrollBar(q->window());
    ui->usersTable->setVerticalScrollBar(scrollBar->proxyScrollBar());

    connect(ui->usersTable, &QAbstractItemView::clicked, this, &Private::handleUsersTableClicked);
    connect(ui->createUserButton, &QPushButton::clicked, this, &Private::createUser);

    deleteButton->setFlat(true);
    editButton->setFlat(true);

    constexpr int kButtonBarHeight = 32;
    selectionControls->setFixedHeight(kButtonBarHeight);

    setPaletteColor(selectionControls, QPalette::Window, core::colorTheme()->color("dark11"));
    setPaletteColor(selectionControls, QPalette::WindowText, core::colorTheme()->color("light4"));
    setPaletteColor(selectionControls, QPalette::Dark, core::colorTheme()->color("dark15"));

    const auto buttonsLayout = selectionControls->horizontalLayout();
    buttonsLayout->setSpacing(16);
    buttonsLayout->addWidget(deleteButton);
    buttonsLayout->addWidget(editButton);
    buttonsLayout->addStretch();

    connect(deleteButton, &QPushButton::clicked, this, &Private::deleteSelected);
    connect(editButton, &QPushButton::clicked, this, &Private::editSelected);

    connect(ui->filterLineEdit, &SearchLineEdit::textChanged, this,
        [this](const QString& text)
        {
            sortModel->setFilterWildcard(text);
            q->update();
        });

    // By [Space] toggle checkbox:
    connect(ui->usersTable, &TreeView::spacePressed, this,
        [this](const QModelIndex& index)
        {
            handleUsersTableClicked(index.sibling(index.row(), UserListModel::CheckBoxColumn));
        });

    setHelpTopic(q, HelpTopic::Id::SystemSettings_UserManagement);

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

    setupPlaceholder();
}

void UserListWidget::Private::setupPlaceholder()
{
    const auto placeholderIcon = new QLabel(ui->nothingFoundPlaceholder);
    placeholderIcon->setPixmap(qnSkin->pixmap("left_panel/placeholders/search.svg"));

    const auto placeholderText = new QLabel(ui->nothingFoundPlaceholder);
    placeholderText->setText(tr("No users found"));
    QFont font(placeholderText->font());
    font.setPixelSize(16);
    font.setWeight(QFont::Medium);
    placeholderText->setFont(font);

    const auto descriptionText = new QLabel(ui->nothingFoundPlaceholder);
    descriptionText->setText(tr("Change search criteria or create a new user"));

    ui->nothingFoundPlaceholderLayout->addWidget(placeholderIcon, /*stretch*/ 0, Qt::AlignHCenter);
    ui->nothingFoundPlaceholderLayout->addWidget(placeholderText, /*stretch*/ 0, Qt::AlignHCenter);
    ui->nothingFoundPlaceholderLayout->addSpacing(8);
    ui->nothingFoundPlaceholderLayout->addWidget(descriptionText, /*stretch*/ 0, Qt::AlignHCenter);
}

void UserListWidget::Private::modelUpdated()
{
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

    const auto canDelete = this->canDelete(users);
    const auto canEdit = canEnableDisable(users) || canChangeAuthentication(users);

    deleteButton->setVisible(canDelete);
    editButton->setVisible(canEdit);

    selectionControls->setDisplayed(canDelete || canEdit);
    header->setCheckState(selectionState);

    q->update();
}

void UserListWidget::Private::createUser()
{
    q->menu()->triggerIfPossible(action::NewUserAction,
        action::Parameters().withArgument(Qn::ParentWidgetRole, QPointer<QWidget>(q)));
}

void UserListWidget::Private::deleteSelected()
{
    const auto usersToDelete = visibleSelectedUsers().filtered(
        [this](const QnUserResourcePtr& user) { return canDelete(user); });

    if (usersToDelete.isEmpty())
        return;

    if (!messages::Resources::deleteResources(q, usersToDelete, /*allowSilent*/ false))
        return;

    auto callback = nx::utils::mutableGuarded(q,
        [this, n = usersToDelete.size(), nonDeletedResources = QnResourceList()](
            bool success,
            const QnResourcePtr& resource) mutable
        {
            if (!success)
                nonDeletedResources.push_back(resource);

            if (--n != 0)
                return;

            q->setEnabled(true);

            if (!nonDeletedResources.isEmpty())
                ui::messages::Resources::deleteResourcesFailed(q, nonDeletedResources);
        });

    for (const auto& resource: usersToDelete)
        qnResourcesChangesManager->deleteResource(resource, callback);

    q->setEnabled(false);
}

void UserListWidget::Private::editSelected()
{
    const auto usersToEdit = visibleSelectedUsers().filtered(
        [this](const QnUserResourcePtr& user)
        {
            return canEnableDisable(user) || canChangeAuthentication(user);
        });

    if (usersToEdit.empty())
        return;

    if (usersToEdit.size() == 1)
    {
        editUser(usersToEdit.front());
        return;
    }

    const QVariantMap dialogParameters{
        {"usersCount", usersToEdit.size()},
        {"enableUsersEditable", canEnableDisable(usersToEdit)},
        {"digestEditable", canChangeAuthentication(usersToEdit)}};

    QmlDialogWrapper batchUserEditDialog(
        QUrl("Nx/Dialogs/UserManagement/BatchUserEditDialog.qml"), dialogParameters, q);

    if (!batchUserEditDialog.exec())
        return; //< Cancelled.

    const auto enableUsers = batchUserEditDialog.window()->property("enableUsers")
        .value<Qt::CheckState>();

    const auto disableDigest = batchUserEditDialog.window()->property("disableDigest")
        .value<Qt::CheckState>();

    if (enableUsers == Qt::PartiallyChecked && disableDigest == Qt::PartiallyChecked)
        return; //< No changes.

    m_requestChain.reset(new UserGroupRequestChain(q->systemContext()));
    for (auto user: usersToEdit)
    {
        UserGroupRequest::UpdateUser updateUser;
        updateUser.id = user->getId();

        // TODO: #vkutin #ikulaychuk These fields should be optional in the API request.
        // Refactor the client side.
        updateUser.enabled = enableUsers != Qt::PartiallyChecked
            ? (enableUsers == Qt::Checked)
            : user->isEnabled();

        updateUser.enableDigest = disableDigest != Qt::PartiallyChecked
            ? (disableDigest == Qt::Checked)
            : user->shouldDigestAuthBeUsed();

        m_requestChain->append(updateUser);
    }

    q->setEnabled(false);

    m_requestChain->start(
        [this](bool success, const QString& errorString)
        {
            q->setEnabled(true);
            if (success)
                return;

            QnMessageBox messageBox(
                QnMessageBoxIcon::Critical,
                errorString,
                {},
                QDialogButtonBox::Ok,
                QDialogButtonBox::Ok,
                q);
            messageBox.exec();
        });
}

void UserListWidget::Private::editUser(const QnUserResourcePtr& user)
{
    if (!NX_ASSERT(user))
        return;

    q->menu()->trigger(action::UserSettingsAction, action::Parameters(user)
        .withArgument(Qn::ForceRole, true)
        .withArgument(Qn::ParentWidgetRole, QPointer<QWidget>(q)));
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

void UserListWidget::Private::visibleAdded(int first, int last)
{
    for (int row = first; row <= last; ++row)
    {
        const QModelIndex index = sortModel->index(row, UserListModel::CheckBoxColumn);
        const auto user = index.data(Qn::UserResourceRole).value<QnUserResourcePtr>();

        const bool checked = index.data(Qt::CheckStateRole).toInt() == Qt::Checked;
        if (!checked)
            continue;

        if (user)
            m_visibleSelected.insert(user);
    }
}

void UserListWidget::Private::visibleAboutToBeRemoved(int first, int last)
{
    for (int row = first; row <= last; ++row)
    {
        const QModelIndex index = sortModel->index(row, UserListModel::CheckBoxColumn);
        const auto user = index.data(Qn::UserResourceRole).value<QnUserResourcePtr>();

        m_visibleSelected.remove(user);
    }
}

void UserListWidget::Private::visibleModified(int first, int last)
{
    for (int row = first; row <= last; ++row)
    {
        const QModelIndex index = sortModel->index(row, UserListModel::CheckBoxColumn);
        const auto user = index.data(Qn::UserResourceRole).value<QnUserResourcePtr>();

        const bool checked = index.data(Qt::CheckStateRole).toInt() == Qt::Checked;
        if (!checked)
            m_visibleSelected.remove(user);
        else
            m_visibleSelected.insert(user);
    }
}

QnUserResourceList UserListWidget::Private::visibleSelectedUsers() const
{
    QnUserResourceList result;
    for (const auto& user: m_visibleSelected)
    {
        const int row = usersModel->userRow(user);
        const QModelIndex index = usersModel->index(row, UserListModel::CheckBoxColumn);

        const bool checked = index.data(Qt::CheckStateRole).toInt() == Qt::Checked;
        if (!checked)
            continue;

        result.push_back(user);
    }

    return result;
}

bool UserListWidget::Private::canDelete(const QnUserResourcePtr& user) const
{
    return q->accessController()->hasPermissions(user, Qn::RemovePermission);
}

bool UserListWidget::Private::canDelete(const QnUserResourceList& users) const
{
    return !users.empty() && std::any_of(users.cbegin(), users.cend(),
        [this](const QnUserResourcePtr& user) { return canDelete(user); });
}

bool UserListWidget::Private::canEnableDisable(const QnUserResourcePtr& user) const
{
    return q->accessController()->hasPermissions(user,
        Qn::WritePermission | Qn::WriteAccessRightsPermission | Qn::SavePermission);
}

bool UserListWidget::Private::canEnableDisable(const QnUserResourceList& users) const
{
    return !users.empty() && std::all_of(users.cbegin(), users.cend(),
        [this](const QnUserResourcePtr& user) { return canEnableDisable(user); });
}

bool UserListWidget::Private::canChangeAuthentication(const QnUserResourcePtr& user) const
{
    return q->accessController()->hasPermissions(user,
        Qn::WritePermission | Qn::WriteDigestPermission | Qn::SavePermission);
}

bool UserListWidget::Private::canChangeAuthentication(const QnUserResourceList& users) const
{
    return !users.empty() && std::all_of(users.cbegin(), users.cend(),
        [this](const QnUserResourcePtr& user) { return canChangeAuthentication(user); });
}

void UserListWidget::Private::handleHeaderCheckStateChanged(Qt::CheckState state)
{
    if (sortModel->rowCount() == usersModel->rowCount({}))
    {
        usersModel->setCheckState(state, {});
    }
    else
    {
        const auto users = visibleUsers();
        for (const auto& user: users)
            usersModel->setCheckState(state, user);
    }
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
            editUser(user);
            break;
        }
    }
}

} // namespace nx::vms::client::desktop
