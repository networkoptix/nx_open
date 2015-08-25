#include "user_management_widget.h"
#include "ui_user_management_widget.h"

#include <QtCore/QSortFilterProxyModel>

#include <api/global_settings.h>
#include <api/app_server_connection.h>

#include <common/common_module.h>

#include <core/resource/user_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>

#include <ui/models/user_list_model.h>
#include <ui/actions/action_manager.h>
#include <ui/dialogs/ldap_settings_dialog.h>
#include <ui/dialogs/ldap_users_dialog.h>
#include <ui/widgets/views/checkboxed_header_view.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_access_controller.h>

#include <utils/common/ldap.h>

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
        updateSelection();
    });

    connect(m_sortModel, &QAbstractItemModel::rowsInserted, this, &QnUserManagementWidget::updateSelection);
    connect(m_sortModel, &QAbstractItemModel::rowsRemoved, this, &QnUserManagementWidget::updateSelection);
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

    ui->actionsWidget->setCurrentWidget(selectionState == Qt::Unchecked
        ? ui->defaultPage
        : ui->selectionPage
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
        updateSelection(); //TODO: #GDM it should be automatically updated by signals from model
    }
}

void QnUserManagementWidget::clearSelection() {
    m_usersModel->setCheckState(Qt::Unchecked);
    updateSelection(); //TODO: #GDM it should be automatically updated by signals from model
}

void QnUserManagementWidget::setSelectedEnabled(bool enabled) {
    for (QnUserResourcePtr user : visibleSelectedUsers()) {
        if (user->isAdmin())
            continue;
        if (!accessController()->hasPermissions(user, Qn::WritePermission))
            continue;
        user->setEnabled(enabled);
        QnAppServerConnectionFactory::getConnection2()->getUserManager()->save(user, this, []{} );
    }
    updateSelection();
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
    updateSelection();
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
