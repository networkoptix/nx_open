#include "user_management_widget_p.h"
#include "ui_user_management_widget.h"
#include <ui/widgets/settings/user_management_widget.h>

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
#include <ui/workbench/workbench_access_controller.h>

#include <utils/common/ldap.h>


QnUserManagementWidgetPrivate::QnUserManagementWidgetPrivate(QnUserManagementWidget *parent) 
    : QObject(parent)
    , QnWorkbenchContextAware(parent)
    , q_ptr(parent)
    , m_usersModel(new QnUserListModel(parent))
    , m_sortModel(new QnSortedUserListModel(parent))
    , m_header(new QnCheckBoxedHeaderView(QnUserListModel::CheckBoxColumn, parent))
{
    m_sortModel->setSourceModel(m_usersModel);

    connect(m_header, &QnCheckBoxedHeaderView::checkStateChanged, this, &QnUserManagementWidgetPrivate::at_headerCheckStateChanged);
}


void QnUserManagementWidgetPrivate::setupUi() {
    Q_Q(QnUserManagementWidget);

    q->ui->usersTable->setModel(m_sortModel);  
    q->ui->usersTable->setHorizontalHeader(m_header);

    m_header->setVisible(true);
    m_header->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_header->setSectionResizeMode(QnUserListModel::PermissionsColumn, QHeaderView::Stretch);
    m_header->setSectionsClickable(true);

    q->ui->usersTable->sortByColumn(QnUserListModel::NameColumn, Qt::AscendingOrder);

    auto updateFetchButton = [this] {
        Q_Q(QnUserManagementWidget);
        q->ui->fetchButton->setEnabled(QnGlobalSettings::instance()->ldapSettings().isValid());
    };


    connect(QnGlobalSettings::instance(), &QnGlobalSettings::ldapSettingsChanged, this, updateFetchButton);

    connect(q->ui->usersTable,              &QTableView::activated, this,  &QnUserManagementWidgetPrivate::at_usersTable_activated);
    connect(q->ui->usersTable,              &QTableView::clicked,   this,  &QnUserManagementWidgetPrivate::at_usersTable_clicked);
    connect(q->ui->ldapSettingsButton,      &QPushButton::clicked,  this,  &QnUserManagementWidgetPrivate::openLdapSettings);
    connect(q->ui->fetchButton,             &QPushButton::clicked,  this,  &QnUserManagementWidgetPrivate::fetchUsers);
    connect(q->ui->createUserButton,        &QPushButton::clicked,  this,  &QnUserManagementWidgetPrivate::createUser);
    connect(q->ui->clearSelectionButton,    &QPushButton::clicked,  this,  &QnUserManagementWidgetPrivate::clearSelection);
    connect(q->ui->enableSelectedButton,    &QPushButton::clicked,  this,  &QnUserManagementWidgetPrivate::enableSelected);
    connect(q->ui->disableSelectedButton,   &QPushButton::clicked,  this,  &QnUserManagementWidgetPrivate::disableSelected);
    connect(q->ui->deleteSelectedButton,    &QPushButton::clicked,  this,  &QnUserManagementWidgetPrivate::deleteSelected);

    updateFetchButton();

    m_sortModel->setDynamicSortFilter(true);
    m_sortModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_sortModel->setFilterKeyColumn(-1);
    connect(q->ui->filterLineEdit,  &QLineEdit::textChanged, this, [this](const QString &text) {
        m_sortModel->setFilterWildcard(text);
        updateSelection();
    });

    connect(m_sortModel, &QAbstractItemModel::rowsInserted, this, &QnUserManagementWidgetPrivate::updateSelection);
    connect(m_sortModel, &QAbstractItemModel::rowsRemoved, this, &QnUserManagementWidgetPrivate::updateSelection);
}

void QnUserManagementWidgetPrivate::updateSelection() {
    
    auto users = visibleSelectedUsers();
    Qt::CheckState selectionState = Qt::Unchecked;

    if (!users.isEmpty()) {

        if (users.size() == m_sortModel->rowCount())
            selectionState = Qt::Checked;
        else
            selectionState = Qt::PartiallyChecked;
    }

    m_header->setCheckState(selectionState);

    Q_Q(QnUserManagementWidget);
    q->ui->actionsWidget->setCurrentWidget(selectionState == Qt::Unchecked
        ? q->ui->defaultPage
        : q->ui->selectionPage
        );

    using boost::algorithm::any_of;

    q->ui->enableSelectedButton->setEnabled(any_of(users, [this] (const QnUserResourcePtr &user) {
        return accessController()->hasPermissions(user, Qn::WritePermission) 
            && !user->isEnabled();
    }));

    q->ui->disableSelectedButton->setEnabled(any_of(users, [this] (const QnUserResourcePtr &user) {
        return accessController()->hasPermissions(user, Qn::WritePermission) 
            && user->isEnabled()
            && !user->isAdmin();
    }));

    q->ui->deleteSelectedButton->setEnabled(any_of(users, [this] (const QnUserResourcePtr &user) {
        return accessController()->hasPermissions(user, Qn::RemovePermission) 
            && !user->isAdmin();
    }));

}

void QnUserManagementWidgetPrivate::openLdapSettings() {
    Q_Q(QnUserManagementWidget);

    QScopedPointer<QnLdapSettingsDialog> dialog(new QnLdapSettingsDialog(q));
    dialog->setWindowModality(Qt::ApplicationModal);
    dialog->exec();
}

void QnUserManagementWidgetPrivate::createUser() {
    menu()->trigger(Qn::NewUserAction); //TODO: #GDM correctly set parent widget
}

void QnUserManagementWidgetPrivate::fetchUsers() {
    if (!QnGlobalSettings::instance()->ldapSettings().isValid()) 
        return;

    Q_Q(QnUserManagementWidget);
    QScopedPointer<QnLdapUsersDialog> dialog(new QnLdapUsersDialog(q));
    dialog->setWindowModality(Qt::ApplicationModal);
    dialog->exec();
}

void QnUserManagementWidgetPrivate::at_mergeLdapUsersAsync_finished(int status, int handle, const QString &errorString) {
    Q_UNUSED(handle);

    if (status == 0 && errorString.isEmpty())
        return;

    // TODO: dk, please show correct message here in case of error
}

void QnUserManagementWidgetPrivate::at_headerCheckStateChanged(Qt::CheckState state) {
    for (const auto &user: visibleUsers())
        m_usersModel->setCheckState(state, user);
    updateSelection();
}

void QnUserManagementWidgetPrivate::at_usersTable_activated(const QModelIndex &index) {
    QnUserResourcePtr user = index.data(Qn::UserResourceRole).value<QnUserResourcePtr>();
    if (!user)
        return;

    menu()->trigger(Qn::UserSettingsAction, QnActionParameters(user));
}

void QnUserManagementWidgetPrivate::at_usersTable_clicked(const QModelIndex &index) {
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

void QnUserManagementWidgetPrivate::clearSelection() {
    m_usersModel->setCheckState(Qt::Unchecked);
    updateSelection(); //TODO: #GDM it should be automatically updated by signals from model
}

void QnUserManagementWidgetPrivate::setSelectedEnabled(bool enabled) {
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

void QnUserManagementWidgetPrivate::enableSelected() {
    setSelectedEnabled(true);
}

void QnUserManagementWidgetPrivate::disableSelected() {
    setSelectedEnabled(false);
}

void QnUserManagementWidgetPrivate::deleteSelected() {
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


QnUserResourceList QnUserManagementWidgetPrivate::visibleUsers() const {
    QnUserResourceList result;

    for (int row = 0; row < m_sortModel->rowCount(); ++row) {
        QModelIndex index = m_sortModel->index(row, QnUserListModel::CheckBoxColumn);
        auto user = index.data(Qn::UserResourceRole).value<QnUserResourcePtr>();
        if (user)
            result << user;
    }
    return result;
}


QnUserResourceList QnUserManagementWidgetPrivate::visibleSelectedUsers() const {
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
