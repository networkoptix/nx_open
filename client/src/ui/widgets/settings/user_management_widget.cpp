#include "user_management_widget.h"
#include "ui_user_management_widget.h"

#include <QtCore/QSortFilterProxyModel>

#include <api/global_settings.h>

#include <common/common_module.h>

#include <core/resource/user_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resources_changes_manager.h>

#include <ui/common/palette.h>
#include <ui/actions/action_manager.h>
#include <ui/dialogs/ldap_settings_dialog.h>
#include <ui/dialogs/ldap_users_dialog.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/models/user_list_model.h>
#include <ui/style/globals.h>
#include <ui/widgets/views/checkboxed_header_view.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_access_controller.h>

#include <utils/common/ldap.h>
#include <utils/math/color_transformations.h>

QnUserManagementWidget::QnUserManagementWidget(QWidget *parent)
    : base_type(parent)
    , QnWorkbenchContextAware(parent)
    , ui(new Ui::QnUserManagementWidget)
    , m_usersModel(new QnUserListModel(parent))
    , m_sortModel(new QnSortedUserListModel(parent))
    , m_header(new QnCheckBoxedHeaderView(QnUserListModel::CheckBoxColumn, parent))
{
    ui->setupUi(this);

    m_sortModel->setSourceModel(m_usersModel);

    ui->usersTable->setModel(m_sortModel);  
    ui->usersTable->setHorizontalHeader(m_header);

    m_header->setVisible(true);
    m_header->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_header->setSectionResizeMode(QnUserListModel::PermissionsColumn, QHeaderView::Stretch);
    m_header->setSectionsClickable(true);
    connect(m_header, &QnCheckBoxedHeaderView::checkStateChanged, this, &QnUserManagementWidget::at_headerCheckStateChanged);

    ui->usersTable->sortByColumn(QnUserListModel::NameColumn, Qt::AscendingOrder);

    auto updateFetchButton = [this] {
        ui->fetchButton->setEnabled(QnGlobalSettings::instance()->ldapSettings().isValid());
    };

    connect(QnGlobalSettings::instance(), &QnGlobalSettings::ldapSettingsChanged, this, updateFetchButton);

    connect(ui->usersTable,              &QTableView::activated, this,  &QnUserManagementWidget::at_usersTable_activated);
    connect(ui->usersTable,              &QTableView::clicked,   this,  &QnUserManagementWidget::at_usersTable_clicked);
    connect(ui->createUserButton,        &QPushButton::clicked,  this,  &QnUserManagementWidget::createUser);
    connect(ui->clearSelectionButton,    &QPushButton::clicked,  this,  &QnUserManagementWidget::clearSelection);
    connect(ui->enableSelectedButton,    &QPushButton::clicked,  this,  &QnUserManagementWidget::enableSelected);
    connect(ui->disableSelectedButton,   &QPushButton::clicked,  this,  &QnUserManagementWidget::disableSelected);
    connect(ui->deleteSelectedButton,    &QPushButton::clicked,  this,  &QnUserManagementWidget::deleteSelected);
    connect(ui->ldapSettingsButton,      &QPushButton::clicked,  this,  &QnUserManagementWidget::openLdapSettings);
    connect(ui->fetchButton,             &QPushButton::clicked,  this,  &QnUserManagementWidget::fetchUsers);

    updateFetchButton();

    m_sortModel->setDynamicSortFilter(true);
    m_sortModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_sortModel->setFilterKeyColumn(-1);
    connect(ui->filterLineEdit,  &QLineEdit::textChanged, this, [this](const QString &text) {
        m_sortModel->setFilterWildcard(text);
    });

    connect(m_sortModel, &QAbstractItemModel::rowsInserted, this,   &QnUserManagementWidget::updateSelection);
    connect(m_sortModel, &QAbstractItemModel::rowsRemoved,  this,   &QnUserManagementWidget::updateSelection);
    connect(m_sortModel, &QAbstractItemModel::dataChanged,  this,   &QnUserManagementWidget::updateSelection);

    setHelpTopic(this,                                                  Qn::SystemSettings_UserManagement_Help);
    setHelpTopic(ui->enableSelectedButton, ui->disableSelectedButton,   Qn::UserSettings_DisableUser_Help);
    setHelpTopic(ui->ldapSettingsButton,                                Qn::UserSettings_LdapIntegration_Help);
    setHelpTopic(ui->fetchButton,                                       Qn::UserSettings_LdapFetch_Help);

}

QnUserManagementWidget::~QnUserManagementWidget() {
}

void QnUserManagementWidget::updateFromSettings() {
    bool currentUserIsLdap = context()->user() && context()->user()->isLdap();
    ui->ldapSettingsButton->setVisible(!currentUserIsLdap);
    ui->fetchButton->setVisible(!currentUserIsLdap);
    updateSelection();
}

void QnUserManagementWidget::updateSelection() {

    auto users = visibleSelectedUsers();
    Qt::CheckState selectionState = Qt::Unchecked;

    if (!users.isEmpty()) {

        if (users.size() == m_sortModel->rowCount())
            selectionState = Qt::Checked;
        else
            selectionState = Qt::PartiallyChecked;
    }

    m_header->setCheckState(selectionState);

    bool selectionActive = selectionState != Qt::Unchecked;

    ui->actionsWidget->setCurrentWidget(selectionActive
        ? ui->selectionPage
        : ui->defaultPage
        );

    using boost::algorithm::any_of;

    ui->enableSelectedButton->setEnabled(any_of(users, [this] (const QnUserResourcePtr &user) {
        return accessController()->hasPermissions(user, Qn::WriteAccessRightsPermission | Qn::SavePermission) 
            && !user->isEnabled();
    }));

    ui->disableSelectedButton->setEnabled(any_of(users, [this] (const QnUserResourcePtr &user) {
        return accessController()->hasPermissions(user, Qn::WriteAccessRightsPermission | Qn::SavePermission) 
            && user->isEnabled()
            && !user->isAdmin();
    }));

    ui->deleteSelectedButton->setEnabled(any_of(users, [this] (const QnUserResourcePtr &user) {
        return accessController()->hasPermissions(user, Qn::RemovePermission) 
            && !user->isAdmin();
    }));

    QColor normalColor = this->palette().color(QPalette::Window);

    /* Here all children palette will be overwritten, so restoring it manually. */
    setPaletteColor(ui->backgroundWidget, QPalette::Window, selectionActive 
        ? m_colors.selectionBackground 
        : normalColor);

    for (QPushButton* button: ui->selectionPage->findChildren<QPushButton*>(QString(), Qt::FindDirectChildrenOnly)) {
        setPaletteColor(button, QPalette::Window, normalColor);       
        setPaletteColor(button, QPalette::Disabled, QPalette::WindowText, m_colors.disabledButtonsText);
    }
    setPaletteColor(m_header, QPalette::Window, normalColor);
    
    update();
}

void QnUserManagementWidget::openLdapSettings() {
    if (!context()->user() || context()->user()->isLdap())
        return;

    QScopedPointer<QnLdapSettingsDialog> dialog(new QnLdapSettingsDialog(this));
    dialog->setWindowModality(Qt::ApplicationModal);
    dialog->exec();
}

void QnUserManagementWidget::createUser() {
    menu()->trigger(Qn::NewUserAction); //TODO: #GDM correctly set parent widget
}

void QnUserManagementWidget::fetchUsers() {
    if (!context()->user() || context()->user()->isLdap())
        return;

    if (!QnGlobalSettings::instance()->ldapSettings().isValid()) 
        return;

    QScopedPointer<QnLdapUsersDialog> dialog(new QnLdapUsersDialog(this));
    dialog->setWindowModality(Qt::ApplicationModal);
    dialog->exec();
}

void QnUserManagementWidget::at_mergeLdapUsersAsync_finished(int status, int handle, const QString &errorString) {
    Q_UNUSED(handle);

    if (status == 0 && errorString.isEmpty())
        return;

    // TODO: dk, please show correct message here in case of error
}

void QnUserManagementWidget::at_headerCheckStateChanged(Qt::CheckState state) {
    for (const auto &user: visibleUsers())
        m_usersModel->setCheckState(state, user);
    updateSelection();
}

void QnUserManagementWidget::at_usersTable_activated(const QModelIndex &index) {
    QnUserResourcePtr user = index.data(Qn::UserResourceRole).value<QnUserResourcePtr>();
    if (!user)
        return;

    menu()->trigger(Qn::UserSettingsAction, QnActionParameters(user));
}

void QnUserManagementWidget::at_usersTable_clicked(const QModelIndex &index) {
    QnUserResourcePtr user = index.data(Qn::UserResourceRole).value<QnUserResourcePtr>();
    if (!user)
        return;

    if (index.column() == QnUserListModel::EditIconColumn) {
        at_usersTable_activated(index);
    }
    else if (index.column() == QnUserListModel::CheckBoxColumn) /* Invert current state */ {
        m_usersModel->setCheckState(index.data(Qt::CheckStateRole).toBool() ? Qt::Unchecked : Qt::Checked, user);
    }
}

void QnUserManagementWidget::clearSelection() {
    m_usersModel->setCheckState(Qt::Unchecked);
}

void QnUserManagementWidget::setSelectedEnabled(bool enabled) {
    for (QnUserResourcePtr user : visibleSelectedUsers()) {
        if (user->isAdmin())
            continue;
        if (!accessController()->hasPermissions(user, Qn::WritePermission))
            continue;
        
        qnResourcesChangesManager->saveUser(user, [enabled](const QnUserResourcePtr &user) {
            user->setEnabled(enabled);
        });
    }
}

void QnUserManagementWidget::enableSelected() {
    setSelectedEnabled(true);
}

void QnUserManagementWidget::disableSelected() {
    setSelectedEnabled(false);
}

void QnUserManagementWidget::deleteSelected() {
    QnUserResourceList usersToDelete;
    for (QnUserResourcePtr user : visibleSelectedUsers()) {
        if (user->isAdmin())
            continue;
        if (!accessController()->hasPermissions(user, Qn::RemovePermission))
            continue;
        usersToDelete << user;
    }

    if (usersToDelete.isEmpty())
        return;

    menu()->trigger(Qn::RemoveFromServerAction, usersToDelete);
}


QnUserResourceList QnUserManagementWidget::visibleUsers() const {
    QnUserResourceList result;

    for (int row = 0; row < m_sortModel->rowCount(); ++row) {
        QModelIndex index = m_sortModel->index(row, QnUserListModel::CheckBoxColumn);
        auto user = index.data(Qn::UserResourceRole).value<QnUserResourcePtr>();
        if (user)
            result << user;
    }
    return result;
}


QnUserResourceList QnUserManagementWidget::visibleSelectedUsers() const {
    QnUserResourceList result;

    for (int row = 0; row < m_sortModel->rowCount(); ++row) {
        QModelIndex index = m_sortModel->index(row, QnUserListModel::CheckBoxColumn);
        bool checked = index.data(Qt::CheckStateRole).toBool();
        if (!checked)
            continue;
        auto user = index.data(Qn::UserResourceRole).value<QnUserResourcePtr>();
        if (user)
            result << user;
    }
    return result;
}

const QnUserManagementColors QnUserManagementWidget::colors() const {
    return m_colors;
}

void QnUserManagementWidget::setColors(const QnUserManagementColors &colors) {
    m_colors = colors;
    updateSelection();
}
