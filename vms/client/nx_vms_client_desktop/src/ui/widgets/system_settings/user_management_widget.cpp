// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_management_widget.h"
#include "ui_user_management_widget.h"

#include <boost/algorithm/cxx11/any_of.hpp>

#include <QtCore/QSortFilterProxyModel>
#include <QtGui/QKeyEvent>
#include <QtWidgets/QMenu>

#include <api/global_settings.h>
#include <client/client_settings.h>
#include <common/common_module.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resources_changes_manager.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/client/desktop/common/delegates/switch_item_delegate.h>
#include <nx/vms/client/desktop/common/utils/item_view_hover_tracker.h>
#include <nx/vms/client/desktop/common/widgets/checkable_header_view.h>
#include <nx/vms/client/desktop/common/widgets/snapped_scroll_bar.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/dialogs/force_secure_auth_dialog.h>
#include <nx/vms/client/desktop/ui/messages/resources_messages.h>
#include <ui/common/palette.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/dialogs/ldap_settings_dialog.h>
#include <ui/dialogs/ldap_users_dialog.h>
#include <ui/dialogs/resource_properties/user_settings_dialog.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/models/user_list_model.h>
#include <ui/widgets/views/resource_list_view.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>
#include <utils/common/event_processors.h>
#include <utils/common/ldap.h>
#include <utils/common/scoped_painter_rollback.h>
#include <utils/math/color_transformations.h>

using namespace nx::vms::client::desktop;
using namespace nx::vms::client::desktop::ui;

namespace {

static constexpr int kMaximumColumnWidth = 200;

class QnUserListDelegate: public QStyledItemDelegate
{
    using base_type = QStyledItemDelegate;
    Q_DECLARE_TR_FUNCTIONS(QnUserManagementWidget)

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
    explicit QnUserListDelegate(ItemViewHoverTracker* hoverTracker, QObject* parent = nullptr) :
        base_type(parent),
        m_hoverTracker(hoverTracker),
        m_linkText(tr("Edit")),
        m_editIcon(qnSkin->icon("user_settings/user_edit.png"))
    {
        NX_ASSERT(m_hoverTracker);
    }

    virtual QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        switch (index.column())
        {
            case QnUserListModel::UserTypeColumn:
                return Skin::maximumSize(index.data(Qt::DecorationRole).value<QIcon>());

            case QnUserListModel::UserRoleColumn:
                return base_type::sizeHint(option, index) + QSize(
                    linkMetrics(option).totalWidth() + kLinkPadding, 0);

            default:
                return base_type::sizeHint(option, index);
        }
    }

    virtual void paint(QPainter* painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        /* Determine item opacity based on user enabled state: */
        QnScopedPainterOpacityRollback opacityRollback(painter);
        if (!QnUserListModel::isInteractiveColumn(index.column()) &&
            index.sibling(index.row(), QnUserListModel::EnabledColumn).data(Qt::CheckStateRole).toInt() != Qt::Checked)
        {
            painter->setOpacity(painter->opacity() * nx::style::Hints::kDisabledItemOpacity);
        }

        /* Paint right-aligned user type icon: */
        if (index.column() == QnUserListModel::UserTypeColumn)
        {
            auto icon = index.data(Qt::DecorationRole).value<QIcon>();
            if (icon.isNull())
                return;

            auto rect = QStyle::alignedRect(Qt::LeftToRight,
                Qt::AlignRight | Qt::AlignVCenter,
                Skin::maximumSize(icon),
                option.rect);

            icon.paint(painter, rect);
            return;
        }

        /* Determine if link should be drawn: */
        bool drawLink = index.column() == QnUserListModel::UserRoleColumn &&
                        option.state.testFlag(QStyle::State_MouseOver) &&
                        m_hoverTracker &&
                        !QnUserListModel::isInteractiveColumn(m_hoverTracker->hoveredIndex().column());

        /* If not, call standard paint: */
        if (!drawLink)
        {
            base_type::paint(painter, option, index);
            return;
        }

        /* Obtain text area rect: */
        QStyleOptionViewItem newOption(option);
        initStyleOption(&newOption, index);
        QStyle* style = newOption.widget ? newOption.widget->style() : QApplication::style();
        QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &newOption, newOption.widget);

        /* Measure link metrics: */
        const auto linkMetrics = this->linkMetrics(option);

        /* Draw original text elided: */
        const int newTextWidth = textRect.width() - linkMetrics.totalWidth() - kLinkPadding;
        newOption.text = newOption.fontMetrics.elidedText(
            newOption.text, newOption.textElideMode, newTextWidth);
        style->drawControl(QStyle::CE_ItemViewItem, &newOption, painter, newOption.widget);

        opacityRollback.rollback();

        /* Draw link icon: */
        QRect iconRect(textRect.right() - linkMetrics.totalWidth() + 1, option.rect.top(),
            linkMetrics.iconWidth, option.rect.height());
        m_editIcon.paint(painter, iconRect, Qt::AlignCenter, QIcon::Active);

        /* Draw link text: */
        QnScopedPainterPenRollback penRollback(painter,
            nx::style::linkColor(newOption.palette, true));
        painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignRight, m_linkText);
    }

    LinkMetrics linkMetrics(const QStyleOptionViewItem &option) const
    {
        return LinkMetrics(
            option.fontMetrics.size({}, m_linkText).width(),
            qnSkin->maximumSize(m_editIcon).width());
    }

private:
    QPointer<ItemViewHoverTracker> m_hoverTracker;
    const QString m_linkText;
    const QIcon m_editIcon;
};

} // unnamed namespace

QnUserManagementWidget::QnUserManagementWidget(QWidget* parent) :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::QnUserManagementWidget),
    m_usersModel(new QnUserListModel(parent)),
    m_sortModel(new QnSortedUserListModel(parent)),
    m_header(new CheckableHeaderView(QnUserListModel::CheckBoxColumn, parent))
{
    ui->setupUi(this);

    ui->filterLineEdit->addAction(qnSkin->icon("theme/input_search.png"), QLineEdit::LeadingPosition);

    ui->filterButton->menu()->addAction(
        tr("All users"),
        [this] { m_sortModel->setDigestFilter(std::nullopt); });

    m_filterDigestAction = ui->filterButton->menu()->addAction(
        tr("With enabled digest authentication"),
        [this] { m_sortModel->setDigestFilter(true); });

    ui->filterButton->setAdjustSize(true);
    ui->filterButton->setCurrentIndex(0);

    m_sortModel->setSourceModel(m_usersModel);

    auto hoverTracker = new ItemViewHoverTracker(ui->usersTable);

    auto switchItemDelegate = new SwitchItemDelegate(this);
    switchItemDelegate->setHideDisabledItems(true);

    ui->usersTable->setModel(m_sortModel);
    ui->usersTable->setHeader(m_header);
    ui->usersTable->setIconSize(QSize(36, 24));
    ui->usersTable->setItemDelegate(new QnUserListDelegate(hoverTracker, this));
    ui->usersTable->setItemDelegateForColumn(QnUserListModel::EnabledColumn,  switchItemDelegate);

    m_header->setVisible(true);
    m_header->setMaximumSectionSize(kMaximumColumnWidth);
    m_header->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_header->setSectionResizeMode(QnUserListModel::FullNameColumn, QHeaderView::Stretch);
    m_header->setSectionsClickable(true);
    connect(m_header, &CheckableHeaderView::checkStateChanged, this, &QnUserManagementWidget::at_headerCheckStateChanged);

    ui->usersTable->sortByColumn(QnUserListModel::LoginColumn, Qt::AscendingOrder);

    SnappedScrollBar *scrollBar = new SnappedScrollBar(this);
    ui->usersTable->setVerticalScrollBar(scrollBar->proxyScrollBar());

    connect(qnGlobalSettings, &QnGlobalSettings::ldapSettingsChanged, this, &QnUserManagementWidget::updateLdapState);
    connect(ui->usersTable,   &QAbstractItemView::clicked,            this, &QnUserManagementWidget::at_usersTable_clicked);

    connect(ui->createUserButton,        &QPushButton::clicked,  this,  &QnUserManagementWidget::createUser);
    connect(ui->rolesButton,             &QPushButton::clicked,  this,  &QnUserManagementWidget::editRoles);
    connect(ui->enableSelectedButton,    &QPushButton::clicked,  this,  &QnUserManagementWidget::enableSelected);
    connect(ui->disableSelectedButton,   &QPushButton::clicked,  this,  &QnUserManagementWidget::disableSelected);
    connect(ui->deleteSelectedButton,    &QPushButton::clicked,  this,  &QnUserManagementWidget::deleteSelected);
    connect(ui->ldapSettingsButton,      &QPushButton::clicked,  this,  &QnUserManagementWidget::openLdapSettings);
    connect(ui->fetchButton,             &QPushButton::clicked,  this,  &QnUserManagementWidget::fetchUsers);
    connect(ui->forceSecureAuthButton, &QPushButton::clicked,
        this, &QnUserManagementWidget::forceSecureAuth);

    m_sortModel->setDynamicSortFilter(true);
    m_sortModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_sortModel->setFilterKeyColumn(-1);
    connect(ui->filterLineEdit,  &QLineEdit::textChanged, this, [this](const QString &text)
    {
        m_sortModel->setFilterWildcard(text);
    });

    connect(m_sortModel, &QAbstractItemModel::rowsInserted, this,   &QnUserManagementWidget::modelUpdated);
    connect(m_sortModel, &QAbstractItemModel::rowsRemoved,  this,   &QnUserManagementWidget::modelUpdated);
    connect(m_sortModel, &QAbstractItemModel::dataChanged,  this,   &QnUserManagementWidget::modelUpdated);

    /* By [Space] toggle checkbox: */
    connect(ui->usersTable, &TreeView::spacePressed, this, [this](const QModelIndex& index)
    {
        at_usersTable_clicked(index.sibling(index.row(), QnUserListModel::CheckBoxColumn));
    });

    /* By [Left] disable user, by [Right] enable user: */
    installEventHandler(ui->usersTable, QEvent::KeyPress, this,
        [this](QObject* object, QEvent* event)
        {
            Q_UNUSED(object);
            int key = static_cast<QKeyEvent*>(event)->key();
            switch (key)
            {
                case Qt::Key_Left:
                case Qt::Key_Right:
                {
                    if (!ui->usersTable->currentIndex().isValid())
                        return;
                    QnUserResourcePtr user = ui->usersTable->currentIndex().data(Qn::UserResourceRole).value<QnUserResourcePtr>();
                    if (!user)
                        return;
                    enableUser(user, key == Qt::Key_Right);
                }
                default:
                    return;
            }
        });

    setHelpTopic(this, Qn::SystemSettings_UserManagement_Help);
    setHelpTopic(ui->enableSelectedButton, Qn::UserSettings_DisableUser_Help);
    setHelpTopic(ui->disableSelectedButton, Qn::UserSettings_DisableUser_Help);
    setHelpTopic(ui->ldapSettingsButton, Qn::Ldap_Help);
    setHelpTopic(ui->fetchButton, Qn::Ldap_Help);
    setHelpTopic(ui->ldapTooltip, Qn::Ldap_Help);

    ui->ldapTooltip->setHintText(tr(
        "Users can be imported from an LDAP server. They will be able to log in only if LDAP "
        "server is online and their accounts are active on it."));

    /* Cursor changes with hover: */
    connect(hoverTracker, &ItemViewHoverTracker::itemEnter, this,
        [this](const QModelIndex& index)
        {
            if (!QnUserListModel::isInteractiveColumn(index.column()))
                ui->usersTable->setCursor(Qt::PointingHandCursor);
            else
                ui->usersTable->unsetCursor();
        });

    connect(hoverTracker, &ItemViewHoverTracker::itemLeave, this,
        [this]()
        {
            ui->usersTable->unsetCursor();
        });

    updateSelection();
}

QnUserManagementWidget::~QnUserManagementWidget()
{
}

void QnUserManagementWidget::loadDataToUi()
{
    ui->createUserButton->setEnabled(true);
    updateLdapState();
    m_usersModel->resetUsers(resourcePool()->getResources<QnUserResource>());
    updateSelection();
}

void QnUserManagementWidget::updateLdapState()
{
    ui->ldapSettingsButton->setEnabled(true);
    ui->fetchButton->setEnabled(qnGlobalSettings->ldapSettings().isValid());
}

void QnUserManagementWidget::applyChanges()
{
    auto modelUsers = m_usersModel->users();
    QSet<QnUserResourcePtr> enableDigestUsers;
    QSet<QnUserResourcePtr> disableDigestUsers;
    QnUserResourceList usersToDelete;

    auto userModified =
        [&](const QnUserResourcePtr& user)
        {
            if (m_usersModel->isDigestEnabled(user))
                enableDigestUsers.insert(user);
            else
                disableDigestUsers.insert(user);
        };

    for (auto user: resourcePool()->getResources<QnUserResource>())
    {
        if (!modelUsers.contains(user))
        {
            usersToDelete << user;
            continue;
        }

        bool enabled = m_usersModel->isUserEnabled(user);
        if (user->isEnabled() != enabled)
        {
            user->setEnabled(enabled);
            userModified(user);
        }

        if (!m_usersModel->isDigestEnabled(user) && user->shouldDigestAuthBeUsed())
            userModified(user);
    }

    if (!enableDigestUsers.empty())
    {
        qnResourcesChangesManager->saveUsers(
            enableDigestUsers.toList(), QnUserResource::DigestSupport::enable);
    }

    if (!disableDigestUsers.empty())
    {
        qnResourcesChangesManager->saveUsers(
            disableDigestUsers.toList(), QnUserResource::DigestSupport::disable);
    }

    /* User still can press cancel on 'Confirm Remove' dialog. */
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
            m_usersModel->resetUsers(resourcePool()->getResources<QnUserResource>());
        }
    }
}

bool QnUserManagementWidget::hasChanges() const
{
    if (!isEnabled())
        return false;

    using boost::algorithm::any_of;
    return any_of(resourcePool()->getResources<QnUserResource>(),
        [this, users = m_usersModel->users()](const QnUserResourcePtr& user)
        {
            return !users.contains(user)
                || user->isEnabled() != m_usersModel->isUserEnabled(user)
                || user->shouldDigestAuthBeUsed() != m_usersModel->isDigestEnabled(user);
        });
}

void QnUserManagementWidget::modelUpdated()
{
    ui->usersTable->setColumnHidden(QnUserListModel::UserTypeColumn,
        !boost::algorithm::any_of(visibleUsers(),
            [](const QnUserResourcePtr& user)
            {
                return !user->isLocal();
            }));


    const bool isEmptyModel = !m_sortModel->rowCount();
    ui->searchWidget->setCurrentWidget(isEmptyModel ? ui->nothingFoundPage : ui->usersPage);

    updateSelection();
}

void QnUserManagementWidget::updateSelection()
{
    auto users = visibleSelectedUsers();
    Qt::CheckState selectionState = Qt::Unchecked;

    bool hasSelection = !users.isEmpty();
    if (hasSelection)
    {
        if (users.size() == m_sortModel->rowCount())
            selectionState = Qt::Checked;
        else
            selectionState = Qt::PartiallyChecked;
    }

    m_header->setCheckState(selectionState);

    using boost::algorithm::any_of;

    ui->enableSelectedButton->setEnabled(any_of(users,
        [this](const QnUserResourcePtr& user)
        {
            return accessController()->hasPermissions(user, Qn::WriteAccessRightsPermission | Qn::SavePermission)
                && !m_usersModel->isUserEnabled(user);
        }));

    ui->disableSelectedButton->setEnabled(any_of(users,
        [this](const QnUserResourcePtr& user)
        {
            return accessController()->hasPermissions(user, Qn::WriteAccessRightsPermission | Qn::SavePermission)
                && m_usersModel->isUserEnabled(user);
        }));

    ui->deleteSelectedButton->setEnabled(any_of(users,
        [this](const QnUserResourcePtr& user)
        {
            return accessController()->hasPermissions(user, Qn::RemovePermission);
        }));

    ui->forceSecureAuthButton->setEnabled(any_of(users,
        [this](const QnUserResourcePtr& user)
        {
            return canDisableDigest(user)
                && m_usersModel->isDigestEnabled(user);
        }));

    update();
}

void QnUserManagementWidget::openLdapSettings()
{
    if (!context()->user())
        return;

    QScopedPointer<QnLdapSettingsDialog> dialog(new QnLdapSettingsDialog(this));
    dialog->setWindowModality(Qt::ApplicationModal);
    dialog->exec();
}

void QnUserManagementWidget::forceSecureAuth()
{
    if (!ForceSecureAuthDialog::isConfirmed(this))
        return;

    for (QnUserResourcePtr user: visibleSelectedUsers())
    {
        if (!canDisableDigest(user))
            continue;

        m_usersModel->setDigestEnabled(user, false);
    }

    emit hasChangesChanged();
    updateSelection();
}

void QnUserManagementWidget::editRoles()
{
    menu()->triggerIfPossible(action::UserRolesAction,
        action::Parameters().withArgument(Qn::ParentWidgetRole, QPointer<QWidget>(this)));
}

void QnUserManagementWidget::createUser()
{
    menu()->triggerIfPossible(action::NewUserAction,
        action::Parameters().withArgument(Qn::ParentWidgetRole, QPointer<QWidget>(this)));
}

void QnUserManagementWidget::fetchUsers()
{
    if (!context()->user())
        return;

    if (!qnGlobalSettings->ldapSettings().isValid())
        return;

    QScopedPointer<QnLdapUsersDialog> dialog(new QnLdapUsersDialog(this));
    dialog->setWindowModality(Qt::ApplicationModal);
    dialog->exec();
}

void QnUserManagementWidget::at_mergeLdapUsersAsync_finished(int status, int handle, const QString& errorString)
{
    Q_UNUSED(handle);

    if (status == 0 && errorString.isEmpty())
        return;

    // TODO: dk, please show correct message here in case of error
}

void QnUserManagementWidget::at_headerCheckStateChanged(Qt::CheckState state)
{
    for (const auto &user: visibleUsers())
        m_usersModel->setCheckState(state, user);

    updateSelection();
}

void QnUserManagementWidget::at_usersTable_clicked(const QModelIndex& index)
{
    QnUserResourcePtr user = index.data(Qn::UserResourceRole).value<QnUserResourcePtr>();
    if (!user)
        return;

    switch (index.column())
    {
        case QnUserListModel::CheckBoxColumn: /* Invert current state */
            m_usersModel->setCheckState(index.data(Qt::CheckStateRole).toInt() == Qt::Checked ?
                Qt::Unchecked : Qt::Checked, user);
            break;

        case QnUserListModel::EnabledColumn:
            enableUser(user, !m_usersModel->isUserEnabled(user));
            break;

        default:
            menu()->trigger(action::UserSettingsAction, action::Parameters(user)
                .withArgument(Qn::FocusTabRole, QnUserSettingsDialog::SettingsPage)
                .withArgument(Qn::ForceRole, true)
                .withArgument(Qn::ParentWidgetRole, QPointer<QWidget>(this))
            );
    }
}

bool QnUserManagementWidget::enableUser(const QnUserResourcePtr& user, bool enabled)
{
    if (!accessController()->hasPermissions(user, Qn::WriteAccessRightsPermission))
        return false;

    m_usersModel->setUserEnabled(user, enabled);
    emit hasChangesChanged();

    return true;
}

void QnUserManagementWidget::setSelectedEnabled(bool enabled)
{
    for (QnUserResourcePtr user : visibleSelectedUsers())
        enableUser(user, enabled);
}

void QnUserManagementWidget::enableSelected()
{
    setSelectedEnabled(true);
}

void QnUserManagementWidget::disableSelected()
{
    setSelectedEnabled(false);
}

void QnUserManagementWidget::deleteSelected()
{
    QnUserResourceList usersToDelete;
    for (QnUserResourcePtr user : visibleSelectedUsers())
    {
        if (!accessController()->hasPermissions(user, Qn::RemovePermission))
            continue;

        m_usersModel->removeUser(user);
    }
    emit hasChangesChanged();
}

QnUserResourceList QnUserManagementWidget::visibleUsers() const
{
    QnUserResourceList result;

    for (int row = 0; row < m_sortModel->rowCount(); ++row)
    {
        QModelIndex index = m_sortModel->index(row, QnUserListModel::CheckBoxColumn);
        auto user = index.data(Qn::UserResourceRole).value<QnUserResourcePtr>();
        if (user)
            result << user;
    }
    return result;
}


QnUserResourceList QnUserManagementWidget::visibleSelectedUsers() const
{
    QnUserResourceList result;

    for (int row = 0; row < m_sortModel->rowCount(); ++row)
    {
        QModelIndex index = m_sortModel->index(row, QnUserListModel::CheckBoxColumn);
        bool checked = index.data(Qt::CheckStateRole).toInt() == Qt::Checked;
        if (!checked)
            continue;

        auto user = index.data(Qn::UserResourceRole).value<QnUserResourcePtr>();
        if (user)
            result << user;
    }

    return result;
}

bool QnUserManagementWidget::canDisableDigest(const QnUserResourcePtr& user) const
{
    return accessController()->hasPermissions(user, Qn::WriteDigestPermission)
        && !user->isCloud();
}

void QnUserManagementWidget::filterDigestUsers()
{
    ui->filterButton->setCurrentAction(m_filterDigestAction);
}
