#include "user_management_widget.h"
#include "ui_user_management_widget.h"

#include <QtCore/QSortFilterProxyModel>

#include <api/global_settings.h>

#include <client/client_settings.h>

#include <common/common_module.h>

#include <core/resource/user_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resources_changes_manager.h>

#include <ui/common/palette.h>
#include <ui/actions/action_manager.h>
#include <ui/common/item_view_hover_tracker.h>
#include <ui/delegates/switch_item_delegate.h>
#include <ui/dialogs/ldap_settings_dialog.h>
#include <ui/dialogs/ldap_users_dialog.h>
#include <ui/dialogs/resource_properties/user_settings_dialog.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/widgets/common/snapped_scrollbar.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/models/user_list_model.h>
#include <ui/style/globals.h>
#include <ui/style/helper.h>
#include <ui/style/skin.h>
#include <ui/widgets/views/checkboxed_header_view.h>
#include <ui/widgets/views/resource_list_view.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_access_controller.h>

#include <utils/common/event_processors.h>
#include <utils/common/ldap.h>
#include <utils/common/scoped_painter_rollback.h>
#include <utils/math/color_transformations.h>


namespace {

class QnUserListDelegate : public QStyledItemDelegate
{
    typedef QStyledItemDelegate base_type;
    Q_DECLARE_TR_FUNCTIONS(QnUserManagementWidget)

public:
    explicit QnUserListDelegate(QnItemViewHoverTracker* hoverTracker, QObject* parent = nullptr) :
        base_type(parent),
        m_hoverTracker(hoverTracker),
        m_editIcon(qnSkin->icon("user_settings/user_edit.png"))
    {
        NX_ASSERT(m_hoverTracker);
    }

    virtual QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        if (index.column() == QnUserListModel::UserTypeColumn)
            return QnSkin::maximumSize(index.data(Qt::DecorationRole).value<QIcon>());

        return base_type::sizeHint(option, index);
    }

    virtual void paint(QPainter* painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        /* Determine item opacity based on user enabled state: */
        QnScopedPainterOpacityRollback opacityRollback(painter);
        if (!QnUserListModel::isInteractiveColumn(index.column()) &&
            index.sibling(index.row(), QnUserListModel::EnabledColumn).data(Qt::CheckStateRole).toInt() != Qt::Checked)
        {
            painter->setOpacity(painter->opacity() * style::Hints::kDisabledItemOpacity);
        }

        /* Paint right-aligned user type icon: */
        if (index.column() == QnUserListModel::UserTypeColumn)
        {
            auto icon = index.data(Qt::DecorationRole).value<QIcon>();
            if (icon.isNull())
                return;

            auto rect = QStyle::alignedRect(Qt::LeftToRight,
                Qt::AlignRight | Qt::AlignVCenter,
                QnSkin::maximumSize(icon),
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

        /* Link text: */
        QString linkText = tr("Edit");

        /* Measure link width: */
        const int kTextFlags = Qt::TextSingleLine | Qt::AlignVCenter;
        int linkWidth = option.fontMetrics.width(linkText, -1, kTextFlags);

        int lineHeight = option.rect.height();
        QSize iconSize = m_editIcon.actualSize(QSize(lineHeight, lineHeight));
        const int kIconPadding = 0;
        linkWidth += iconSize.width() + kIconPadding;

        /* Draw original text elided: */
        int newTextWidth = textRect.width() - linkWidth - style::Metrics::kStandardPadding;
        newOption.text = newOption.fontMetrics.elidedText(newOption.text, newOption.textElideMode, newTextWidth);
        style->drawControl(QStyle::CE_ItemViewItem, &newOption, painter, newOption.widget);

        opacityRollback.rollback();

        /* Draw link icon: */
        QRect iconRect(textRect.right() - linkWidth + 1, option.rect.top(), iconSize.width(), lineHeight);
        m_editIcon.paint(painter, iconRect, Qt::AlignCenter, QIcon::Active);

        /* Draw link text: */
        QnScopedPainterPenRollback penRollback(painter, newOption.palette.color(QPalette::Normal, QPalette::Link));
        painter->drawText(textRect, kTextFlags | Qt::AlignRight, linkText);
    }

private:
    QPointer<QnItemViewHoverTracker> m_hoverTracker;
    QIcon m_editIcon;
};

} // unnamed namespace

QnUserManagementWidget::QnUserManagementWidget(QWidget* parent) :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::QnUserManagementWidget),
    m_usersModel(new QnUserListModel(parent)),
    m_sortModel(new QnSortedUserListModel(parent)),
    m_header(new QnCheckBoxedHeaderView(QnUserListModel::CheckBoxColumn, parent))
{
    ui->setupUi(this);

    ui->filterLineEdit->addAction(qnSkin->icon("theme/input_search.png"), QLineEdit::LeadingPosition);

    m_sortModel->setSourceModel(m_usersModel);

    auto hoverTracker = new QnItemViewHoverTracker(ui->usersTable);

    auto switchItemDelegate = new QnSwitchItemDelegate(this);
    switchItemDelegate->setHideDisabledItems(true);

    ui->usersTable->setModel(m_sortModel);
    ui->usersTable->setHeader(m_header);
    ui->usersTable->setIconSize(QSize(36, 24));
    ui->usersTable->setItemDelegate(new QnUserListDelegate(hoverTracker, this));
    ui->usersTable->setItemDelegateForColumn(QnUserListModel::EnabledColumn,  switchItemDelegate);

    m_header->setVisible(true);
    m_header->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_header->setSectionResizeMode(QnUserListModel::UserRoleColumn, QHeaderView::Stretch);
    m_header->setSectionsClickable(true);
    connect(m_header, &QnCheckBoxedHeaderView::checkStateChanged, this, &QnUserManagementWidget::at_headerCheckStateChanged);

    ui->usersTable->sortByColumn(QnUserListModel::LoginColumn, Qt::AscendingOrder);

    QnSnappedScrollBar *scrollBar = new QnSnappedScrollBar(this);
    ui->usersTable->setVerticalScrollBar(scrollBar->proxyScrollBar());

    connect(qnGlobalSettings, &QnGlobalSettings::ldapSettingsChanged, this, &QnUserManagementWidget::updateLdapState);
    connect(ui->usersTable,   &QAbstractItemView::clicked,            this, &QnUserManagementWidget::at_usersTable_clicked);

    connect(ui->createUserButton,        &QPushButton::clicked,  this,  &QnUserManagementWidget::createUser);
    connect(ui->rolesButton,             &QPushButton::clicked,  this,  &QnUserManagementWidget::editRoles);
    connect(ui->clearSelectionButton,    &QPushButton::clicked,  this,  &QnUserManagementWidget::clearSelection);
    connect(ui->enableSelectedButton,    &QPushButton::clicked,  this,  &QnUserManagementWidget::enableSelected);
    connect(ui->disableSelectedButton,   &QPushButton::clicked,  this,  &QnUserManagementWidget::disableSelected);
    connect(ui->deleteSelectedButton,    &QPushButton::clicked,  this,  &QnUserManagementWidget::deleteSelected);
    connect(ui->ldapSettingsButton,      &QPushButton::clicked,  this,  &QnUserManagementWidget::openLdapSettings);
    connect(ui->fetchButton,             &QPushButton::clicked,  this,  &QnUserManagementWidget::fetchUsers);

    connect(qnCommon, &QnCommonModule::readOnlyChanged, this, &QnUserManagementWidget::loadDataToUi);

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
    connect(ui->usersTable, &QnTreeView::spacePressed, this, [this](const QModelIndex& index)
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

    setHelpTopic(this,                                                  Qn::SystemSettings_UserManagement_Help);
    setHelpTopic(ui->enableSelectedButton, ui->disableSelectedButton,   Qn::UserSettings_DisableUser_Help);
    setHelpTopic(ui->ldapSettingsButton,                                Qn::UserSettings_LdapIntegration_Help);
    setHelpTopic(ui->fetchButton,                                       Qn::UserSettings_LdapFetch_Help);

    /* Cursor changes with hover: */
    connect(hoverTracker, &QnItemViewHoverTracker::itemEnter, this,
        [this](const QModelIndex& index)
        {
            if (!QnUserListModel::isInteractiveColumn(index.column()))
                ui->usersTable->setCursor(Qt::PointingHandCursor);
            else
                ui->usersTable->unsetCursor();
        });

    connect(hoverTracker, &QnItemViewHoverTracker::itemLeave, this,
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
    ui->createUserButton->setEnabled(!qnCommon->isReadOnly());
    updateLdapState();
    m_usersModel->resetUsers(qnResPool->getResources<QnUserResource>());
}

void QnUserManagementWidget::updateLdapState()
{
    bool currentUserIsLdap = context()->user() && context()->user()->isLdap();
    ui->ldapSettingsButton->setVisible(!currentUserIsLdap);
    ui->ldapSettingsButton->setEnabled(!qnCommon->isReadOnly());
    ui->fetchButton->setVisible(!currentUserIsLdap);
    ui->fetchButton->setEnabled(!qnCommon->isReadOnly() && qnGlobalSettings->ldapSettings().isValid());
}

void QnUserManagementWidget::applyChanges()
{
    auto modelUsers = m_usersModel->users();
    QnUserResourceList usersToDelete;
    for (auto user : qnResPool->getResources<QnUserResource>())
    {
        if (!modelUsers.contains(user))
        {
            usersToDelete << user;
            continue;
        }

        bool enabled = m_usersModel->isUserEnabled(user);
        if (user->isEnabled() != enabled)
        {
            qnResourcesChangesManager->saveUser(user,
                [enabled](const QnUserResourcePtr &user)
                {
                    user->setEnabled(enabled);
                });
        }
    }

    /* User still can press cancel on 'Confirm Remove' dialog. */
    if (confirmUsersDelete(usersToDelete))
        qnResourcesChangesManager->deleteResources(usersToDelete);
    else
        m_usersModel->resetUsers(qnResPool->getResources<QnUserResource>());
}

bool QnUserManagementWidget::hasChanges() const
{
    using boost::algorithm::any_of;
    return any_of(qnResPool->getResources<QnUserResource>(),
        [this, users = m_usersModel->users()](const QnUserResourcePtr& user)
        {
            return !users.contains(user)
                || user->isEnabled() != m_usersModel->isUserEnabled(user);
        });
}

void QnUserManagementWidget::modelUpdated()
{
    ui->usersTable->setColumnHidden(QnUserListModel::UserTypeColumn,
        !boost::algorithm::any_of(visibleUsers(),
            [this](const QnUserResourcePtr& user)
            {
                return !user->isLocal();
            }));

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

    ui->clearSelectionButton->setEnabled(hasSelection);

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

    update();
}

void QnUserManagementWidget::openLdapSettings()
{
    if (!context()->user() || context()->user()->isLdap())
        return;

    QScopedPointer<QnLdapSettingsDialog> dialog(new QnLdapSettingsDialog(this));
    dialog->setWindowModality(Qt::ApplicationModal);
    dialog->exec();
}

void QnUserManagementWidget::editRoles()
{
    menu()->triggerIfPossible(QnActions::UserRolesAction); //TODO: #vkutin #GDM correctly set parent widget
}

void QnUserManagementWidget::createUser()
{
    menu()->triggerIfPossible(QnActions::NewUserAction); //TODO: #GDM correctly set parent widget
}

void QnUserManagementWidget::fetchUsers()
{
    if (!context()->user() || context()->user()->isLdap())
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
            menu()->trigger(QnActions::UserSettingsAction, QnActionParameters(user)
                .withArgument(Qn::FocusTabRole, QnUserSettingsDialog::SettingsPage)
                .withArgument(Qn::ForceRole, true)
            );
    }
}

void QnUserManagementWidget::clearSelection()
{
    m_usersModel->setCheckState(Qt::Unchecked);
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

bool QnUserManagementWidget::confirmUsersDelete(const QnUserResourceList& users)
{
    if (users.isEmpty())
        return false;

    /* Check if user have already silenced this warning. */
    if (qnSettings->showOnceMessages().testFlag(Qn::ShowOnceMessage::DeleteResources))
        return true;

    QnMessageBox messageBox(
        QnMessageBox::Warning,
        Qn::Empty_Help,
        tr("Delete Users..."),
        tr("Confirm Delete Users"),
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        this);
    messageBox.setDefaultButton(QDialogButtonBox::Ok);
    messageBox.setInformativeText(tr("Do you really want to delete the following %n users?",
        "", users.size()));
    messageBox.setCheckBoxText(tr("Do not show this message anymore"));
    messageBox.addCustomWidget(new QnResourceListView(users));

    auto result = messageBox.exec();
    if (messageBox.isChecked())
    {
        Qn::ShowOnceMessages messagesFilter = qnSettings->showOnceMessages();
        messagesFilter |= Qn::ShowOnceMessage::DeleteResources;
        qnSettings->setShowOnceMessages(messagesFilter);
    }

    return result == QDialogButtonBox::Ok;
}
