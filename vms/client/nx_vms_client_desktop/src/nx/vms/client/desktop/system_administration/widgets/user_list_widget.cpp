// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ui_user_list_widget.h"

#include <algorithm>

#include <QtCore/QMap>
#include <QtCore/QSortFilterProxyModel>
#include <QtGui/QKeyEvent>
#include <QtGui/QPainter>
#include <QtGui/QPalette>
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
#include <nx/utils/scope_guard.h>
#include <nx/vms/api/data/ldap.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/access/access_controller.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/common/dialogs/qml_dialog_wrapper.h>
#include <nx/vms/client/desktop/common/utils/item_view_hover_tracker.h>
#include <nx/vms/client/desktop/common/widgets/checkable_header_view.h>
#include <nx/vms/client/desktop/common/widgets/message_bar.h>
#include <nx/vms/client/desktop/common/widgets/snapped_scroll_bar.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/resource/resources_changes_manager.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/system_administration/globals/user_group_request_chain.h>
#include <nx/vms/client/desktop/system_administration/models/user_list_model.h>
#include <nx/vms/client/desktop/system_administration/models/user_property_tracker.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/dialogs/force_secure_auth_dialog.h>
#include <nx/vms/client/desktop/ui/messages/resources_messages.h>
#include <nx/vms/client/desktop/utils/ldap_status_watcher.h>
#include <nx/vms/client/desktop/workbench/managers/settings_dialog_manager.h>
#include <nx/vms/common/system_settings.h>
#include <ui/common/palette.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/widgets/views/resource_list_view.h>
#include <ui/workbench/workbench_context.h>
#include <utils/common/event_processors.h>
#include <utils/math/color_transformations.h>

#include "private/highlighted_text_item_delegate.h"
#include "user_list_widget.h"

namespace nx::vms::client::desktop {

using namespace nx::vms::client::desktop::ui;

namespace {

static constexpr int kMaximumColumnWidth = 200;

static const core::SvgIconColorer::ThemeSubstitutions kTextButtonColors = {
    {QIcon::Normal, {"light4"}}, {QIcon::Active, {"light3"}}};

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

        // Paint right-aligned user type icon or left-aligned custom permissions icon:
        const bool isUserIconColumn = index.column() == UserListModel::UserTypeColumn
            || index.column() == UserListModel::UserWarningColumn;
        if (isUserIconColumn || index.column() == UserListModel::IsCustomColumn)
        {
            if (opt.state.testFlag(QStyle::StateFlag::State_Selected))
                painter->fillRect(opt.rect, opt.palette.highlight());

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
                opt.state.testFlag(QStyle::StateFlag::State_Selected)
                    ? QIcon::Selected : QIcon::Normal);
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

        if (index.data(Qn::DisabledRole).toBool())
        {
            if (option->checkState == Qt::Unchecked)
            {
                option->palette.setColor(QPalette::Text,
                    option->state.testFlag(QStyle::State_Selected)
                        ? core::colorTheme()->color("light13")
                        : core::colorTheme()->color("dark16"));
            }
            else
            {
                option->palette.setColor(QPalette::Text,
                    option->state.testFlag(QStyle::State_Selected)
                        ? core::colorTheme()->color("light10")
                        : core::colorTheme()->color("light13"));
            }
        }
        else
        {
            if (option->checkState == Qt::Unchecked)
            {
                option->palette.setColor(QPalette::Text,
                    option->state.testFlag(QStyle::State_Selected)
                        ? core::colorTheme()->color("light4")
                        : core::colorTheme()->color("light10"));
            }
            else
            {
                option->palette.setColor(QPalette::Text,
                    option->state.testFlag(QStyle::State_Selected)
                        ? core::colorTheme()->color("light2")
                        : core::colorTheme()->color("light4"));
            }
        }
    }
};

// -----------------------------------------------------------------------------------------------
// UserListHeaderView

class UserListHeaderView : public CheckableHeaderView
{
    using base_type = CheckableHeaderView;
public:
    UserListHeaderView(int checkboxColumn, int customColumn, QWidget* parent = nullptr):
        base_type(checkboxColumn, parent),
        m_customColumn(customColumn)
    {}

protected:
    virtual void paintSection(
        QPainter* painter, const QRect& rect, int logicalIndex) const override
    {
        if (logicalIndex != m_customColumn
            || !isSortIndicatorShown()
            || sortIndicatorSection() == logicalIndex)
        {
            base_type::paintSection(painter, rect, logicalIndex);
            return;
        }

        QStyleOptionHeader opt;
        initStyleOption(&opt);
        opt.rect = rect;
        opt.section = logicalIndex;
        opt.position = QStyleOptionHeader::End;
        opt.sortIndicator = QStyleOptionHeader::None;
        if (logicalIndexAt(mapFromGlobal(QCursor::pos())) == logicalIndex)
            opt.state |= QStyle::State_MouseOver;
        opt.textAlignment = Qt::AlignVCenter;
        opt.text = model()->headerData(logicalIndex, orientation()).toString();
        style()->drawControl(QStyle::CE_Header, &opt, painter, this);
    }

private:
    int m_customColumn;
};

// -----------------------------------------------------------------------------------------------
// UserListWidget

class UserListWidget::Private: public QObject
{
    Q_DECLARE_TR_FUNCTIONS(UserListWidget::Private)

    UserListWidget* const q;
    UserPropertyTracker m_visibleUsers;

public:
    nx::utils::ImplPtr<Ui::UserListWidget> ui{new Ui::UserListWidget()};

    UserListModel* const usersModel{new UserListModel(q)};
    SortedUserListModel* const sortModel{new SortedUserListModel(q)};
    UserListHeaderView* const header{
        new UserListHeaderView(UserListModel::CheckBoxColumn, UserListModel::IsCustomColumn, q)};
    QAction* filterDigestAction = nullptr;

    CommonMessageBar* const notFoundUsersWarning{
        new CommonMessageBar(q, {.level = BarDescription::BarLevel::Error, .isClosable = true})};
    QPushButton* const deleteNotFoundUsersButton{
        new QPushButton(qnSkin->icon("text_buttons/delete_20.svg", kTextButtonColors),
            QString(),
            notFoundUsersWarning)};

    CommonMessageBar* const ldapServerOfflineWarning{
        new CommonMessageBar(q, {.level = BarDescription::BarLevel::Error, .isClosable = true})};
    CommonMessageBar* const nonUniqueUsersWarning{
        new CommonMessageBar(q, {.level = BarDescription::BarLevel::Error, .isClosable = true})};

    ControlBar* const selectionControls{new ControlBar(q)};

    QPushButton* const deleteButton{new QPushButton(
        qnSkin->icon("text_buttons/delete_20.svg", kTextButtonColors),
        tr("Delete"),
        selectionControls)};

    QPushButton* const editButton{new QPushButton(
        qnSkin->icon("text_buttons/edit_20.svg", kTextButtonColors),
        tr("Edit"),
        selectionControls)};

    bool hasChanges = false;
    std::unique_ptr<UserGroupRequestChain> requestChain;

public:
    explicit Private(UserListWidget* q);
    void setupUi();
    void filterDigestUsers() { ui->filterButton->setCurrentAction(filterDigestAction); }
    void updateBanners();
    void setMassEditInProgress(bool inProgress);

private:
    void createUser();
    void editUser(const QnUserResourcePtr& user, bool raiseDialog = true);
    void deleteUsers(const QnUserResourceList& usersToDelete);
    void deleteSelected();
    void editSelected();
    void deleteNotFoundLdapUsers();

    void modelUpdated();
    void updateSelection();

    void setupPlaceholder();

    void handleHeaderCheckStateChanged(Qt::CheckState state);
    void handleUsersTableClicked(const QModelIndex& index);

    bool canDelete(const QSet<QnUserResourcePtr>& users) const;
    bool canEnableDisable(const QSet<QnUserResourcePtr>& users) const;
    bool canChangeAuthentication(const QSet<QnUserResourcePtr>& users) const;

    QnUserResourceList visibleUsers() const;

    void visibleAddedOrUpdated(int first, int last);
    void visibleAboutToBeRemoved(int first, int last);
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
    if (!NX_ASSERT(!isNetworkRequestRunning(), "Requests should already be completed."))
        discardChanges();
}

void UserListWidget::loadDataToUi()
{
    d->header->setCheckState(Qt::Unchecked);
    d->ui->usersTable->setCurrentIndex({});
}

void UserListWidget::applyChanges()
{
}

void UserListWidget::discardChanges()
{
    d->requestChain.reset();
}

bool UserListWidget::hasChanges() const
{
    if (!isEnabled())
        return false;

    return d->hasChanges;
}

void UserListWidget::resetWarnings()
{
    d->updateBanners();
}

bool UserListWidget::isNetworkRequestRunning() const
{
    return d->requestChain && d->requestChain->isRunning();
}

void UserListWidget::filterDigestUsers()
{
    d->filterDigestUsers();
}

// -----------------------------------------------------------------------------------------------
// UserListWidget::Private

UserListWidget::Private::Private(UserListWidget* q): q(q), m_visibleUsers(q->systemContext())
{
    sortModel->setDynamicSortFilter(true);
    sortModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    sortModel->setFilterKeyColumn(-1);
    sortModel->setSourceModel(usersModel);

    connect(sortModel, &QAbstractItemModel::rowsInserted, this,
        [this](const QModelIndex&, int first, int last)
        {
            visibleAddedOrUpdated(first, last);
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
            visibleAddedOrUpdated(topLeft.row(), bottomRight.row());
            modelUpdated();
        });

    connect(sortModel, &QAbstractItemModel::modelReset, this,
        [this]()
        {
            hasChanges = false;
            emit this->q->hasChangesChanged();

            m_visibleUsers.clear();
            if (sortModel->rowCount() > 0)
                visibleAddedOrUpdated(0, sortModel->rowCount() - 1);

            modelUpdated();
        });
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
    q->addAction(ui->searchAction);
    ui->usersTable->addAction(ui->deleteUserAction);

    auto alertsLayout = new QVBoxLayout();
    alertsLayout->setSpacing(0);
    alertsLayout->addWidget(selectionControls);
    alertsLayout->addWidget(notFoundUsersWarning);
    alertsLayout->addWidget(ldapServerOfflineWarning);
    alertsLayout->addWidget(nonUniqueUsersWarning);
    q->layout()->addItem(alertsLayout);

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
    header->setSectionResizeMode(QHeaderView::Stretch);
    header->setSectionResizeMode(UserListModel::IsCustomColumn, QHeaderView::ResizeToContents);
    header->setSectionsClickable(true);
    for (const auto column: {
        UserListModel::CheckBoxColumn,
        UserListModel::UserWarningColumn,
        UserListModel::UserTypeColumn})
    {
        header->setSectionResizeMode(column, QHeaderView::Fixed);
        header->setAlignment(column, Qt::AlignHCenter);
    }
    header->resizeSection(UserListModel::CheckBoxColumn, 32);
    header->resizeSection(UserListModel::UserWarningColumn, 20);
    header->resizeSection(UserListModel::UserTypeColumn, 30);

    connect(header, &CheckableHeaderView::checkStateChanged,
        this, &Private::handleHeaderCheckStateChanged);

    ui->usersTable->sortByColumn(UserListModel::UserTypeColumn, Qt::AscendingOrder);
    const auto scrollBar = new SnappedScrollBar(q->window());
    ui->usersTable->setVerticalScrollBar(scrollBar->proxyScrollBar());
    connect(ui->usersTable, &QAbstractItemView::clicked, this, &Private::handleUsersTableClicked);

    ui->createUserButton->setIcon(qnSkin->icon("buttons/add_20x20.svg", kTextButtonColors));
    connect(ui->createUserButton, &QPushButton::clicked, this, &Private::createUser);

    nonUniqueUsersWarning->setText(tr(
        "Multiple users share the same login, causing login failures. To resolve this issue, "
            "either update the affected user logins or disable/delete duplicates."));

    notFoundUsersWarning->addButton(deleteNotFoundUsersButton);
    connect(deleteNotFoundUsersButton, &QPushButton::clicked, this,
        &Private::deleteNotFoundLdapUsers);

    deleteButton->setFlat(true);
    editButton->setFlat(true);

    constexpr int kButtonBarHeight = 32;
    selectionControls->setFixedHeight(kButtonBarHeight);

    setPaletteColor(selectionControls, QPalette::Window, core::colorTheme()->color("dark11"));
    setPaletteColor(selectionControls, QPalette::WindowText, core::colorTheme()->color("light4"));
    setPaletteColor(selectionControls, QPalette::Dark, core::colorTheme()->color("dark15"));
    QColor disabledTextColor = core::colorTheme()->color("light4");
    disabledTextColor.setAlphaF(0.3);
    setPaletteColor(selectionControls, QPalette::Disabled, QPalette::WindowText,
        disabledTextColor);
    setPaletteColor(deleteNotFoundUsersButton, QPalette::Disabled, QPalette::WindowText,
        disabledTextColor);

    const auto buttonsLayout = selectionControls->horizontalLayout();
    buttonsLayout->setSpacing(16);
    buttonsLayout->addWidget(deleteButton);
    buttonsLayout->addWidget(editButton);
    buttonsLayout->addStretch();

    QWidget::setTabOrder(ui->usersTable, deleteButton);
    QWidget::setTabOrder(deleteButton, editButton);
    QWidget::setTabOrder(editButton, deleteNotFoundUsersButton);

    connect(deleteButton, &QPushButton::clicked, this, &Private::deleteSelected);
    connect(editButton, &QPushButton::clicked, this, &Private::editSelected);

    connect(ui->filterLineEdit, &SearchLineEdit::textChanged, this,
        [this](const QString& text)
        {
            sortModel->setFilterWildcard(text);
            q->update();
        });

    connect(ui->usersTable, &TreeView::spacePressed, this,
        [this](const QModelIndex& index)
        {
            // Toggle checkbox when Space is pressed.
            handleUsersTableClicked(index.sibling(index.row(), UserListModel::CheckBoxColumn));
        });
    ui->usersTable->setEnterKeyEventIgnored(false);
    connect(ui->usersTable, &TreeView::enterPressed, this,
        [this](const QModelIndex& index)
        {
            const auto user = index.data(Qn::UserResourceRole).value<QnUserResourcePtr>();
            if (!user)
                return;
            editUser(user);
        });

    connect(ui->usersTable, &TreeView::selectionChanging, this,
        [this](QItemSelectionModel::SelectionFlags selectionFlags,
            const QModelIndex& index,
            const QEvent* event)
        {
            if (!event || event->type() != QEvent::KeyPress)
                return;

            if (!selectionFlags.testFlag(QItemSelectionModel::Select))
                return;

            const auto user = index.data(Qn::UserResourceRole).value<QnUserResourcePtr>();
            if (!user)
                return;

            QMetaObject::invokeMethod(this,
                [this, user]() { editUser(user, /*openDialog*/ false); },
                Qt::QueuedConnection);
        });

    connect(ui->usersTable, &TreeView::gotFocus, this,
        [this](Qt::FocusReason reason)
        {
            if (reason != Qt::TabFocusReason && reason != Qt::BacktabFocusReason)
                return;

            if (ui->usersTable->currentIndex().isValid())
                ui->usersTable->setCurrentIndex(ui->usersTable->currentIndex());
            else
                ui->usersTable->setCurrentIndex(sortModel->index(0, 0));
        });
    connect(ui->usersTable, &TreeView::lostFocus, this,
        [this]()
        {
            if (!q->context()->settingsDialogManager()->isEditUserDialogVisible())
                ui->usersTable->clearSelection();
        });

    connect(ui->deleteUserAction, &QAction::triggered, this,
        [this]()
        {
            if (m_visibleUsers.selectedDeletable().size() > 0)
            {
                deleteSelected();
                return;
            }

            const auto index = ui->usersTable->currentIndex();
            if (!index.isValid())
                return;

            const auto user = index.data(Qn::UserResourceRole).value<QnUserResourcePtr>();
            if (usersModel->canDelete(user))
                deleteUsers({user});
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

    const auto updateCurrentUserId =
        [this]()
        {
            const QnUuid userId = q->context()->settingsDialogManager()->currentEditedUserId();
            if (userId.isNull())
            {
                ui->usersTable->clearSelection();
                return;
            }

            if (const auto user = q->resourcePool()->getResourceById<QnUserResource>(userId))
            {
                const auto row = usersModel->userRow(user);
                const auto index = usersModel->index(row);
                const auto mappedIndex = sortModel->mapFromSource(index);
                ui->usersTable->setCurrentIndex(mappedIndex);
            }
        };
    connect(q->context()->settingsDialogManager(),
        &SettingsDialogManager::currentEditedUserIdChanged, this, updateCurrentUserId);
    connect(q->context()->settingsDialogManager(),
        &SettingsDialogManager::currentEditedUserIdChangeFailed, this, updateCurrentUserId);

    connect(q->systemContext()->ldapStatusWatcher(), &LdapStatusWatcher::statusChanged, this,
        &Private::updateBanners);
    connect(usersModel, &UserListModel::notFoundUsersChanged, this, &Private::updateBanners);
    connect(usersModel, &UserListModel::nonUniqueUsersChanged, this, &Private::updateBanners);

    updateSelection();
    updateBanners();
    setupPlaceholder();
}

void UserListWidget::Private::setupPlaceholder()
{
    ui->nothingFoundPlaceholder->setFixedWidth(180);
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
    descriptionText->setWordWrap(true);
    descriptionText->setAlignment(Qt::AlignHCenter);
    font = descriptionText->font();
    font.setPixelSize(14);
    descriptionText->setFont(font);

    ui->nothingFoundPlaceholderLayout->addWidget(placeholderIcon, /*stretch*/ 0, Qt::AlignHCenter);
    ui->nothingFoundPlaceholderLayout->addSpacing(15);
    ui->nothingFoundPlaceholderLayout->addWidget(placeholderText, /*stretch*/ 0, Qt::AlignHCenter);
    ui->nothingFoundPlaceholderLayout->addSpacing(9);
    ui->nothingFoundPlaceholderLayout->addWidget(descriptionText);
}

void UserListWidget::Private::updateBanners()
{
    notFoundUsersWarning->setText(tr("%n existing LDAP users are not found in the LDAP database",
        "", usersModel->notFoundUsers().size()));
    deleteNotFoundUsersButton->setText(
        tr("Delete %n users", "", usersModel->notFoundUsers().size()));
    ldapServerOfflineWarning->setText(
        tr("LDAP server is offline. %n users are not able to log in.", "",
            usersModel->ldapUserCount()));

    ldapServerOfflineWarning->setVisible(
        q->systemContext()->ldapStatusWatcher()->status()
        && q->systemContext()->ldapStatusWatcher()->status()->state
            != api::LdapStatus::State::online
        && usersModel->ldapUserCount() > 0);

    notFoundUsersWarning->setVisible(!ldapServerOfflineWarning->isVisible()
        && !usersModel->notFoundUsers().isEmpty());
    nonUniqueUsersWarning->setVisible(!usersModel->nonUniqueUsers().isEmpty());
}

void UserListWidget::Private::setMassEditInProgress(bool inProgress)
{
    ui->usersTable->setDisabled(inProgress);
    ui->deleteUserAction->setDisabled(inProgress);

    deleteNotFoundUsersButton->setDisabled(inProgress);
    editButton->setDisabled(inProgress);
    deleteButton->setDisabled(inProgress);
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
    Qt::CheckState selectionState = Qt::Unchecked;

    if (!m_visibleUsers.selected().empty())
    {
        const auto selectedOrDisabledCount =
            m_visibleUsers.selected().size() + m_visibleUsers.disabledCount();
        selectionState = selectedOrDisabledCount == sortModel->rowCount()
            ? Qt::Checked
            : Qt::PartiallyChecked;
    }

    deleteButton->setVisible(!m_visibleUsers.selectedDeletable().isEmpty());
    editButton->setVisible(!m_visibleUsers.selectedEditable().isEmpty());

    selectionControls->setDisplayed(m_visibleUsers.hasSelectedModifiable());
    header->setCheckState(selectionState);

    emit q->selectionUpdated();
    q->update();
}

void UserListWidget::Private::createUser()
{
    q->context()->settingsDialogManager()->createUser(q);
}

void UserListWidget::Private::deleteUsers(const QnUserResourceList& usersToDelete)
{
    if (usersToDelete.isEmpty())
        return;

    if (!messages::Resources::deleteResources(q, usersToDelete, /*allowSilent*/ false))
        return;

    auto chain = new UserGroupRequestChain(q->systemContext());

    for (const auto& user: usersToDelete)
        chain->append(UserGroupRequest::RemoveUser{.id = user->getId()});

    setMassEditInProgress(true);
    auto rollback = nx::utils::makeScopeGuard([this]{ setMassEditInProgress(false); });

    chain->start(nx::utils::guarded(this,
        [this, chain, usersToDelete, rollback = std::move(rollback)](
            bool success, nx::network::rest::Result::Error errorCode, const QString& errorString)
        {
            if (!success && errorCode != nx::network::rest::Result::Error::SessionExpired)
            {
                QnResourceList nonDeletedUsers;
                for (const auto& user: usersToDelete)
                {
                    if (!user->hasFlags(Qn::removed))
                        nonDeletedUsers << user;
                }

                if (!nonDeletedUsers.isEmpty())
                {
                    QString text;
                    if (const auto count = nonDeletedUsers.count(); count == 1)
                    {
                        text = tr("Failed to delete user \"%1\".").arg(
                            nonDeletedUsers.first()->getName().toHtmlEscaped());
                    }
                    else
                    {
                        text = tr("Failed to delete %n users.", /*comment*/ "", count);
                    }

                    QnMessageBox messageBox(
                        QnMessageBoxIcon::Critical,
                        text,
                        errorString,
                        QDialogButtonBox::Ok,
                        QDialogButtonBox::Ok,
                        q);
                    messageBox.exec();
                }
            }

            chain->deleteLater();
        }));
}

void UserListWidget::Private::deleteSelected()
{
    const auto usersToDelete = m_visibleUsers.selectedDeletable();
    deleteUsers({usersToDelete.begin(), usersToDelete.end()});
}

void UserListWidget::Private::deleteNotFoundLdapUsers()
{
    deleteUsers(q->resourcePool()->getResourcesByIds<QnUserResource>(usersModel->notFoundUsers()));
}

void UserListWidget::Private::editSelected()
{
    const auto getSelectedUsers = [this]() -> std::tuple<QSet<QnUserResourcePtr>, QVariantMap>
    {
        const auto usersToEdit = m_visibleUsers.selectedEditable();

        return {usersToEdit, QVariantMap{
            {"usersCount", usersToEdit.size()},
            {"enableUsersEditable", canEnableDisable(usersToEdit)},
            {"digestEditable", canChangeAuthentication(usersToEdit)}}};
    };

    QSet<QnUserResourcePtr> usersToEdit;
    QVariantMap dialogParameters;
    std::tie(usersToEdit, dialogParameters) = getSelectedUsers();

    if (usersToEdit.empty())
        return;

    if (usersToEdit.size() == 1)
    {
        editUser(*usersToEdit.begin());
        return;
    }

    QmlDialogWrapper batchUserEditDialog(
        QUrl("Nx/Dialogs/UserManagement/BatchUserEditDialog.qml"), dialogParameters, q);

    q->connect(q, &UserListWidget::selectionUpdated, &batchUserEditDialog,
        [&batchUserEditDialog, &usersToEdit, getSelectedUsers]
        {
            if (!NX_ASSERT(batchUserEditDialog.window()))
                return;

            QVariantMap dialogParameters;
            std::tie(usersToEdit, dialogParameters) = getSelectedUsers();

            if (usersToEdit.empty())
            {
                batchUserEditDialog.reject();
                return;
            }

            for (const auto& [k, v]: dialogParameters.asKeyValueRange())
                batchUserEditDialog.window()->setProperty(k.toStdString().c_str(), v);
        });

    if (!batchUserEditDialog.exec())
        return; //< Cancelled.

    // Selection might change during dialog execution.
    if (usersToEdit.empty())
        return;

    const auto enableUsers = batchUserEditDialog.window()->property("enableUsers")
        .value<Qt::CheckState>();

    const auto disableDigest = batchUserEditDialog.window()->property("disableDigest")
        .value<Qt::CheckState>();

    if (enableUsers == Qt::PartiallyChecked && disableDigest == Qt::PartiallyChecked)
        return; //< No changes.

    requestChain.reset(new UserGroupRequestChain(q->systemContext()));
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

        requestChain->append(updateUser);
    }

    setMassEditInProgress(true);
    auto rollback = nx::utils::makeScopeGuard([this]{ setMassEditInProgress(false); });

    requestChain->start(
        [this, rollback = std::move(rollback)](
            bool success,  nx::network::rest::Result::Error errorCode, const QString& errorString)
        {
            if (success || errorCode == nx::network::rest::Result::Error::SessionExpired)
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

void UserListWidget::Private::editUser(const QnUserResourcePtr& user, bool raiseDialog)
{
    if (!NX_ASSERT(user))
        return;

    if (raiseDialog)
        q->context()->settingsDialogManager()->editUser(user->getId(), /*tab*/ -1, q);
    else
        q->context()->settingsDialogManager()->setCurrentEditedUserId(user->getId());
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

void UserListWidget::Private::visibleAddedOrUpdated(int first, int last)
{
    for (int row = first; row <= last; ++row)
    {
        const QModelIndex index = sortModel->index(row, UserListModel::CheckBoxColumn);
        const auto user = index.data(Qn::UserResourceRole).value<QnUserResourcePtr>();

        m_visibleUsers.addOrUpdate(
            user,
            {
                .selected = index.data(Qt::CheckStateRole).toInt() == Qt::Checked,
                .disabled = index.data(Qn::DisabledRole).toBool(),
                .editable = usersModel->canEnableDisable(user)
                    || usersModel->canChangeAuthentication(user),
                .deletable = usersModel->canDelete(user)
            });
    }
}

void UserListWidget::Private::visibleAboutToBeRemoved(int first, int last)
{
    for (int row = first; row <= last; ++row)
    {
        const QModelIndex index = sortModel->index(row, UserListModel::CheckBoxColumn);
        const auto user = index.data(Qn::UserResourceRole).value<QnUserResourcePtr>();

        m_visibleUsers.remove(user);
    }
}

bool UserListWidget::Private::canDelete(const QSet<QnUserResourcePtr>& users) const
{
    return !users.empty() && std::any_of(users.cbegin(), users.cend(),
        [this](const QnUserResourcePtr& user) { return usersModel->canDelete(user); });
}

bool UserListWidget::Private::canEnableDisable(const QSet<QnUserResourcePtr>& users) const
{
    return !users.empty() && std::all_of(users.cbegin(), users.cend(),
        [this](const QnUserResourcePtr& user) { return usersModel->canEnableDisable(user); });
}

bool UserListWidget::Private::canChangeAuthentication(const QSet<QnUserResourcePtr>& users) const
{
    return !users.empty() && std::all_of(users.cbegin(), users.cend(),
        [this](const QnUserResourcePtr& user)
        {
            return usersModel->canChangeAuthentication(user);
        });
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
            if (index.data(Qn::DisabledRole).toBool())
                break;

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
