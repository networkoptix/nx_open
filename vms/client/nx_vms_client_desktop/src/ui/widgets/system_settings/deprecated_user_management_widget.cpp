// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "deprecated_user_management_widget.h"
#include "ui_deprecated_user_management_widget.h"

#include <boost/algorithm/cxx11/any_of.hpp>

#include <QtCore/QMap>
#include <QtCore/QSortFilterProxyModel>
#include <QtGui/QKeyEvent>
#include <QtWidgets/QMenu>

#include <client/client_settings.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resources_changes_manager.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/range_adapters.h>
#include <nx/vms/client/desktop/common/delegates/switch_item_delegate.h>
#include <nx/vms/client/desktop/common/utils/item_view_hover_tracker.h>
#include <nx/vms/client/desktop/common/widgets/checkable_header_view.h>
#include <nx/vms/client/desktop/common/widgets/snapped_scroll_bar.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/dialogs/force_secure_auth_dialog.h>
#include <nx/vms/client/desktop/ui/messages/resources_messages.h>
#include <nx/vms/common/system_settings.h>
#include <ui/common/palette.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/dialogs/ldap_settings_dialog.h>
#include <ui/dialogs/ldap_users_dialog.h>
#include <ui/dialogs/resource_properties/user_settings_dialog.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/models/deprecated_user_list_model.h>
#include <ui/widgets/views/resource_list_view.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>
#include <utils/common/event_processors.h>
#include <utils/common/ldap.h>
#include <utils/common/scoped_painter_rollback.h>
#include <utils/math/color_transformations.h>

using namespace nx::vms::client::desktop;
using namespace nx::vms::client::desktop::ui;
using namespace nx::vms::common;

namespace {

static constexpr int kMaximumColumnWidth = 200;

// -----------------------------------------------------------------------------------------------
// QnUserListDelegate

class QnUserListDelegate: public QStyledItemDelegate
{
    using base_type = QStyledItemDelegate;
    Q_DECLARE_TR_FUNCTIONS(QnDeprecatedUserManagementWidget)

    struct LinkMetrics
    {
        const int textWidth = 0;
        const int iconWidth = 0;

        static constexpr int kIconPadding = 0;

        LinkMetrics(int textWidth, int iconWidth) :
            textWidth(textWidth),
            iconWidth(iconWidth)
        {
        }

        int totalWidth() const
        {
            return textWidth + iconWidth + kIconPadding;
        }
    };

    static constexpr int kLinkPadding = 0;

public:
    explicit QnUserListDelegate(ItemViewHoverTracker* hoverTracker, QObject* parent = nullptr):
        base_type(parent),
        m_hoverTracker(hoverTracker),
        m_linkText(tr("Edit")),
        m_editIcon(qnSkin->icon("user_settings/user_edit.png"))
    {
        NX_ASSERT(m_hoverTracker);
    }

    virtual QSize sizeHint(
        const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        switch (index.column())
        {
            case QnDeprecatedUserListModel::UserTypeColumn:
                return Skin::maximumSize(index.data(Qt::DecorationRole).value<QIcon>());

            case QnDeprecatedUserListModel::UserRoleColumn:
                return base_type::sizeHint(option, index) + QSize(
                    linkMetrics(option).totalWidth() + kLinkPadding, 0);

            default:
                return base_type::sizeHint(option, index);
        }
    }

    virtual void paint(QPainter* painter, const QStyleOptionViewItem& option,
        const QModelIndex& index) const override
    {
        // Determine item opacity based on user enabled state:
        QnScopedPainterOpacityRollback opacityRollback(painter);
        if (!QnDeprecatedUserListModel::isInteractiveColumn(index.column())
            && index.sibling(index.row(), QnDeprecatedUserListModel::EnabledColumn).data(
                Qt::CheckStateRole).toInt() != Qt::Checked)
        {
            painter->setOpacity(painter->opacity() * nx::style::Hints::kDisabledItemOpacity);
        }

        // Paint right-aligned user type icon:
        if (index.column() == QnDeprecatedUserListModel::UserTypeColumn)
        {
            const auto icon = index.data(Qt::DecorationRole).value<QIcon>();
            if (icon.isNull())
                return;

            const auto rect = QStyle::alignedRect(Qt::LeftToRight,
                Qt::AlignRight | Qt::AlignVCenter,
                Skin::maximumSize(icon),
                option.rect);

            icon.paint(painter, rect);
            return;
        }

        // Determine if link should be drawn:
        const bool drawLink = index.column() == QnDeprecatedUserListModel::UserRoleColumn
            && option.state.testFlag(QStyle::State_MouseOver)
            && m_hoverTracker
            && !QnDeprecatedUserListModel::isInteractiveColumn(
                m_hoverTracker->hoveredIndex().column());

        // If not, call standard paint:
        if (!drawLink)
        {
            base_type::paint(painter, option, index);
            return;
        }

        // Obtain text area rect:
        QStyleOptionViewItem newOption(option);
        initStyleOption(&newOption, index);
        const auto style = newOption.widget ? newOption.widget->style() : QApplication::style();
        const QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &newOption,
            newOption.widget);

        // Measure link metrics:
        const auto linkMetrics = this->linkMetrics(option);

        // Draw original text elided:
        const int newTextWidth = textRect.width() - linkMetrics.totalWidth() - kLinkPadding;
        newOption.text = newOption.fontMetrics.elidedText(
            newOption.text, newOption.textElideMode, newTextWidth);
        style->drawControl(QStyle::CE_ItemViewItem, &newOption, painter, newOption.widget);

        opacityRollback.rollback();

        // Draw link icon:
        const QRect iconRect(textRect.right() - linkMetrics.totalWidth() + 1, option.rect.top(),
            linkMetrics.iconWidth, option.rect.height());
        m_editIcon.paint(painter, iconRect, Qt::AlignCenter, QIcon::Active);

        // Draw link text:
        const QnScopedPainterPenRollback penRollback(painter,
            nx::style::linkColor(newOption.palette, true));
        painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignRight, m_linkText);
    }

    LinkMetrics linkMetrics(const QStyleOptionViewItem& option) const
    {
        return LinkMetrics(
            option.fontMetrics.size({}, m_linkText).width(),
            qnSkin->maximumSize(m_editIcon).width());
    }

private:
    const QPointer<ItemViewHoverTracker> m_hoverTracker;
    const QString m_linkText;
    const QIcon m_editIcon;
};

} // unnamed namespace

// -----------------------------------------------------------------------------------------------
// QnDeprecatedUserManagementWidget

class QnDeprecatedUserManagementWidget::Private: public QObject
{
    QnDeprecatedUserManagementWidget* const q;
    Ui::QnDeprecatedUserManagementWidget* const ui{q->ui.get()};

public:
    QnDeprecatedUserListModel* const usersModel{new QnDeprecatedUserListModel(q)};
    QnDeprecatedSortedUserListModel* const sortModel{new QnDeprecatedSortedUserListModel(q)};
    CheckableHeaderView* const header{new CheckableHeaderView(
        QnDeprecatedUserListModel::CheckBoxColumn, q)};
    QAction* filterDigestAction = nullptr;

public:
    Private(QnDeprecatedUserManagementWidget* q);
    void setupUi();
    void loadData();

private:
    void editRoles();
    void createUser();
    void fetchUsers();
    void openLdapSettings();
    void forceSecureAuth();

    bool enableUser(const QnUserResourcePtr& user, bool enabled);
    void setSelectedEnabled(bool enabled);
    void enableSelected() { setSelectedEnabled(true); }
    void disableSelected() { setSelectedEnabled(false); }
    void deleteSelected();

    void modelUpdated();
    void updateSelection();
    void updateLdapState();

    void handleHeaderCheckStateChanged(Qt::CheckState state);
    void handleUsersTableClicked(const QModelIndex& index);

    bool canDisableDigest(const QnUserResourcePtr& user) const;

    QnUserResourceList visibleUsers() const;
    QnUserResourceList visibleSelectedUsers() const;
};

QnDeprecatedUserManagementWidget::QnDeprecatedUserManagementWidget(QWidget* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::QnDeprecatedUserManagementWidget()),
    d(new Private(this))
{
    d->setupUi();
}

QnDeprecatedUserManagementWidget::~QnDeprecatedUserManagementWidget()
{
    // Required here for forward-declared scoped pointer destruction.
}

void QnDeprecatedUserManagementWidget::loadDataToUi()
{
    d->loadData();
}

void QnDeprecatedUserManagementWidget::applyChanges()
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

bool QnDeprecatedUserManagementWidget::hasChanges() const
{
    if (!isEnabled())
        return false;

    using boost::algorithm::any_of;
    return any_of(resourcePool()->getResources<QnUserResource>(),
        [this, users = d->usersModel->users()](const QnUserResourcePtr& user)
        {
            return !users.contains(user)
                || user->isEnabled() != d->usersModel->isUserEnabled(user)
                || user->shouldDigestAuthBeUsed() != d->usersModel->isDigestEnabled(user);
        });
}

void QnDeprecatedUserManagementWidget::filterDigestUsers()
{
    ui->filterButton->setCurrentAction(d->filterDigestAction);
}

// -----------------------------------------------------------------------------------------------
// QnDeprecatedUserManagementWidget::Private

QnDeprecatedUserManagementWidget::Private::Private(QnDeprecatedUserManagementWidget* q): q(q)
{
    sortModel->setDynamicSortFilter(true);
    sortModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    sortModel->setFilterKeyColumn(-1);
    sortModel->setSourceModel(usersModel);

    connect(sortModel, &QAbstractItemModel::rowsInserted, this, &Private::modelUpdated);
    connect(sortModel, &QAbstractItemModel::rowsRemoved, this, &Private::modelUpdated);
    connect(sortModel, &QAbstractItemModel::dataChanged, this, &Private::modelUpdated);

    connect(q->globalSettings(), &SystemSettings::ldapSettingsChanged,
        this, &Private::updateLdapState);
}

void QnDeprecatedUserManagementWidget::Private::setupUi()
{
    ui->setupUi(q);

    ui->filterLineEdit->addAction(qnSkin->icon("theme/input_search.png"),
        QLineEdit::LeadingPosition);

    ui->filterButton->menu()->addAction(
        QnDeprecatedUserManagementWidget::tr("All users"),
        [this] { sortModel->setDigestFilter(std::nullopt); });

    filterDigestAction = ui->filterButton->menu()->addAction(
        QnDeprecatedUserManagementWidget::tr("With enabled digest authentication"),
        [this] { sortModel->setDigestFilter(true); });

    ui->filterButton->setAdjustSize(true);
    ui->filterButton->setCurrentIndex(0);

    const auto hoverTracker = new ItemViewHoverTracker(ui->usersTable);

    const auto switchItemDelegate = new SwitchItemDelegate(q);
    switchItemDelegate->setHideDisabledItems(true);

    ui->usersTable->setModel(sortModel);
    ui->usersTable->setHeader(header);
    ui->usersTable->setIconSize(QSize(36, 24));
    ui->usersTable->setItemDelegate(new QnUserListDelegate(hoverTracker, q));
    ui->usersTable->setItemDelegateForColumn(
        QnDeprecatedUserListModel::EnabledColumn, switchItemDelegate);

    header->setVisible(true);
    header->setMaximumSectionSize(kMaximumColumnWidth);
    header->setSectionResizeMode(QHeaderView::ResizeToContents);
    header->setSectionResizeMode(QnDeprecatedUserListModel::FullNameColumn, QHeaderView::Stretch);
    header->setSectionsClickable(true);

    connect(header, &CheckableHeaderView::checkStateChanged,
        this, &Private::handleHeaderCheckStateChanged);

    ui->usersTable->sortByColumn(QnDeprecatedUserListModel::LoginColumn, Qt::AscendingOrder);

    const auto scrollBar = new SnappedScrollBar(q);
    ui->usersTable->setVerticalScrollBar(scrollBar->proxyScrollBar());

    connect(ui->usersTable, &QAbstractItemView::clicked, this, &Private::handleUsersTableClicked);
    connect(ui->createUserButton, &QPushButton::clicked, this, &Private::createUser);
    connect(ui->rolesButton, &QPushButton::clicked, this, &Private::editRoles);
    connect(ui->enableSelectedButton, &QPushButton::clicked, this, &Private::enableSelected);
    connect(ui->disableSelectedButton, &QPushButton::clicked, this, &Private::disableSelected);
    connect(ui->deleteSelectedButton, &QPushButton::clicked, this, &Private::deleteSelected);
    connect(ui->ldapSettingsButton, &QPushButton::clicked, this, &Private::openLdapSettings);
    connect(ui->fetchButton, &QPushButton::clicked, this, &Private::fetchUsers);
    connect(ui->forceSecureAuthButton, &QPushButton::clicked, this, &Private::forceSecureAuth);

    connect(ui->filterLineEdit, &QLineEdit::textChanged, this,
        [this](const QString& text) { sortModel->setFilterWildcard(text); });

    // By [Space] toggle checkbox:
    connect(ui->usersTable, &TreeView::spacePressed, this,
        [this](const QModelIndex& index)
        {
            handleUsersTableClicked(index.sibling(index.row(),
                QnDeprecatedUserListModel::CheckBoxColumn));
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
    setHelpTopic(ui->enableSelectedButton, Qn::UserSettings_DisableUser_Help);
    setHelpTopic(ui->disableSelectedButton, Qn::UserSettings_DisableUser_Help);
    setHelpTopic(ui->ldapSettingsButton, Qn::Ldap_Help);
    setHelpTopic(ui->fetchButton, Qn::Ldap_Help);
    setHelpTopic(ui->ldapTooltip, Qn::Ldap_Help);

    ui->ldapTooltip->setHintText(QnDeprecatedUserManagementWidget::tr(
        "Users can be imported from an LDAP server. They will be able to log in only if LDAP "
        "server is online and their accounts are active on it."));

    // Cursor changes with hover:
    connect(hoverTracker, &ItemViewHoverTracker::itemEnter, this,
        [this](const QModelIndex& index)
        {
            if (!QnDeprecatedUserListModel::isInteractiveColumn(index.column()))
                ui->usersTable->setCursor(Qt::PointingHandCursor);
            else
                ui->usersTable->unsetCursor();
        });

    connect(hoverTracker, &ItemViewHoverTracker::itemLeave, this,
        [this]() { ui->usersTable->unsetCursor(); });

    updateSelection();
}

void QnDeprecatedUserManagementWidget::Private::updateLdapState()
{
    ui->ldapSettingsButton->setEnabled(true);
    ui->fetchButton->setEnabled(
        q->globalSettings()->ldapSettings().isValid(/*checkPassword*/ false));
}

void QnDeprecatedUserManagementWidget::Private::modelUpdated()
{
    ui->usersTable->setColumnHidden(QnDeprecatedUserListModel::UserTypeColumn,
        !boost::algorithm::any_of(visibleUsers(),
            [](const QnUserResourcePtr& user) { return !user->isLocal(); }));

    const bool isEmptyModel = !sortModel->rowCount();
    ui->searchWidget->setCurrentWidget(isEmptyModel
        ? ui->nothingFoundPage
        : ui->usersPage);

    updateSelection();
}

void QnDeprecatedUserManagementWidget::Private::updateSelection()
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

    header->setCheckState(selectionState);

    using boost::algorithm::any_of;

    const auto canModifyUser =
        [this](const QnUserResourcePtr& user)
        {
            const auto requiredPermissions = Qn::WriteAccessRightsPermission | Qn::SavePermission;
            return q->accessController()->hasPermissions(user, requiredPermissions);
        };

    ui->enableSelectedButton->setEnabled(any_of(users,
        [this, canModifyUser](const QnUserResourcePtr& user)
        {
            return canModifyUser(user) && !usersModel->isUserEnabled(user);
        }));

    ui->disableSelectedButton->setEnabled(any_of(users,
        [this, canModifyUser](const QnUserResourcePtr& user)
        {
            return canModifyUser(user) && usersModel->isUserEnabled(user);
        }));

    ui->deleteSelectedButton->setEnabled(any_of(users,
        [this](const QnUserResourcePtr& user)
        {
            return q->accessController()->hasPermissions(user, Qn::RemovePermission);
        }));

    ui->forceSecureAuthButton->setEnabled(any_of(users,
        [this](const QnUserResourcePtr& user)
        {
            return canDisableDigest(user) && usersModel->isDigestEnabled(user);
        }));

    q->update();
}

void QnDeprecatedUserManagementWidget::Private::openLdapSettings()
{
    if (!q->context()->user())
        return;

    QScopedPointer<QnLdapSettingsDialog> dialog(new QnLdapSettingsDialog(q));
    dialog->setWindowModality(Qt::ApplicationModal);
    dialog->exec();
}

void QnDeprecatedUserManagementWidget::Private::forceSecureAuth()
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

void QnDeprecatedUserManagementWidget::Private::editRoles()
{
    q->menu()->triggerIfPossible(action::UserRolesAction,
        action::Parameters().withArgument(Qn::ParentWidgetRole, QPointer<QWidget>(q)));
}

void QnDeprecatedUserManagementWidget::Private::createUser()
{
    q->menu()->triggerIfPossible(action::NewUserAction,
        action::Parameters().withArgument(Qn::ParentWidgetRole, QPointer<QWidget>(q)));
}

void QnDeprecatedUserManagementWidget::Private::fetchUsers()
{
    if (!q->context()->user())
        return;

    if (!q->globalSettings()->ldapSettings().isValid(/*checkPassword*/ false))
        return;

    QScopedPointer<QnLdapUsersDialog> dialog(new QnLdapUsersDialog(q));
    dialog->setWindowModality(Qt::ApplicationModal);
    dialog->exec();
}

bool QnDeprecatedUserManagementWidget::Private::enableUser(
    const QnUserResourcePtr& user, bool enabled)
{
    if (!q->accessController()->hasPermissions(user, Qn::WriteAccessRightsPermission))
        return false;

    usersModel->setUserEnabled(user, enabled);
    emit q->hasChangesChanged();

    return true;
}

void QnDeprecatedUserManagementWidget::Private::setSelectedEnabled(bool enabled)
{
    const auto users = visibleSelectedUsers();
    for (const auto& user: users)
        enableUser(user, enabled);
}

void QnDeprecatedUserManagementWidget::Private::deleteSelected()
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

QnUserResourceList QnDeprecatedUserManagementWidget::Private::visibleUsers() const
{
    QnUserResourceList result;
    for (int row = 0; row < sortModel->rowCount(); ++row)
    {
        const QModelIndex index = sortModel->index(row, QnDeprecatedUserListModel::CheckBoxColumn);
        const auto user = index.data(Qn::UserResourceRole).value<QnUserResourcePtr>();
        if (user)
            result.push_back(user);
    }

    return result;
}

QnUserResourceList QnDeprecatedUserManagementWidget::Private::visibleSelectedUsers() const
{
    QnUserResourceList result;
    for (int row = 0; row < sortModel->rowCount(); ++row)
    {
        const QModelIndex index = sortModel->index(row, QnDeprecatedUserListModel::CheckBoxColumn);
        const bool checked = index.data(Qt::CheckStateRole).toInt() == Qt::Checked;
        if (!checked)
            continue;

        const auto user = index.data(Qn::UserResourceRole).value<QnUserResourcePtr>();
        if (user)
            result.push_back(user);
    }

    return result;
}

bool QnDeprecatedUserManagementWidget::Private::canDisableDigest(
    const QnUserResourcePtr& user) const
{
    return q->accessController()->hasPermissions(user, Qn::WriteDigestPermission);
}

void QnDeprecatedUserManagementWidget::Private::handleHeaderCheckStateChanged(Qt::CheckState state)
{
    const auto users = visibleUsers();
    for (const auto& user: users)
        usersModel->setCheckState(state, user);

    updateSelection();
}

void QnDeprecatedUserManagementWidget::Private::handleUsersTableClicked(const QModelIndex& index)
{
    const auto user = index.data(Qn::UserResourceRole).value<QnUserResourcePtr>();
    if (!user)
        return;

    switch (index.column())
    {
        case QnDeprecatedUserListModel::CheckBoxColumn:
        {
            const auto nextCheckState = index.data(Qt::CheckStateRole).toInt() == Qt::Checked
                ? Qt::Unchecked
                : Qt::Checked;

            usersModel->setCheckState(nextCheckState, user);
            break;
        }

        case QnDeprecatedUserListModel::EnabledColumn:
            enableUser(user, !usersModel->isUserEnabled(user));
            break;

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

void QnDeprecatedUserManagementWidget::Private::loadData()
{
    ui->createUserButton->setEnabled(true);
    updateLdapState();
    usersModel->resetUsers(q->resourcePool()->getResources<QnUserResource>());
    updateSelection();
}
