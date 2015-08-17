#include "user_management_widget.h"
#include "ui_user_management_widget.h"

#include <QtCore/QSortFilterProxyModel>

#include <api/app_server_connection.h>

#include <core/resource/user_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <api/global_settings.h>
#include <utils/common/ldap.h>
#include <utils/network/http/asynchttpclient.h>
#include <common/common_module.h>
#include <client/client_meta_types.h>
#include <ui/models/user_list_model.h>
#include <ui/actions/action_manager.h>
#include <ui/dialogs/ldap_settings_dialog.h>

class QnUserManagementWidgetPrivate : public QObject {
    Q_DECLARE_PUBLIC(QnUserManagementWidget)
public:
    QnUserManagementWidget *q_ptr;

    QnUserListModel *usersModel;
    QnSortedUserListModel *sortModel;

    QnUserManagementWidgetPrivate(QnUserManagementWidget *parent)
        : QObject(parent)
        , q_ptr(parent)
        , usersModel(new QnUserListModel(parent))
        , sortModel(new QnSortedUserListModel(parent))
    {
        sortModel->setSourceModel(usersModel);
    }

    void at_usersTable_activated(const QModelIndex &index);
    void at_usersTable_clicked(const QModelIndex &index);
    
    bool openLdapSettings();
};

void QnUserManagementWidgetPrivate::at_usersTable_activated(const QModelIndex &index) {
    Q_Q(QnUserManagementWidget);

    QnUserResourcePtr user = index.data(Qn::UserResourceRole).value<QnUserResourcePtr>();
    if (!user)
        return;

    q->menu()->trigger(Qn::UserSettingsAction, QnActionParameters(user));
}

void QnUserManagementWidgetPrivate::at_usersTable_clicked(const QModelIndex &index) {
    if (index.column() == QnUserListModel::EditIconColumn)
        at_usersTable_activated(index);
}

void QnUserManagementWidget::at_refreshButton_clicked() {
    Q_D(QnUserManagementWidget);

    if (!QnGlobalSettings::instance()->ldapSettings().isValid()) {
        if (!d->openLdapSettings())
            return;
    }

    QnMediaServerResourcePtr server = qnResPool->getResourceById<QnMediaServerResource>(qnCommon->remoteGUID());
    Q_ASSERT(server);
    if (!server)
        return;

    server->apiConnection()->mergeLdapUsersAsync(this, SLOT(at_mergeLdapUsersAsync_finished(int,int,QString)));
}

void QnUserManagementWidget::at_mergeLdapUsersAsync_finished(int status, int handle, const QString &errorString) {
    Q_UNUSED(handle)

    if (status == 0 && errorString.isEmpty())
        return;

    // TODO: dk, please show correct message here in case of error
}

bool QnUserManagementWidgetPrivate::openLdapSettings() {
    Q_Q(QnUserManagementWidget);

    QScopedPointer<QnLdapSettingsDialog> dialog(new QnLdapSettingsDialog(q));
    dialog->setWindowModality(Qt::ApplicationModal);
    return dialog->exec() == QDialogButtonBox::Ok;
}


QnUserManagementWidget::QnUserManagementWidget(QWidget *parent)
    : base_type(parent)
    , QnWorkbenchContextAware(parent)
    , ui(new Ui::QnUserManagementWidget)
    , d_ptr(new QnUserManagementWidgetPrivate(this))
{
    Q_D(QnUserManagementWidget);

    ui->setupUi(this);

    ui->usersTable->setModel(d->sortModel);
    ui->usersTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->usersTable->horizontalHeader()->setSectionResizeMode(QnUserListModel::PermissionsColumn, QHeaderView::Stretch);
    ui->usersTable->sortByColumn(QnUserListModel::NameColumn, Qt::AscendingOrder);

    connect(ui->usersTable,             &QTableView::activated,     d,      &QnUserManagementWidgetPrivate::at_usersTable_activated);
    connect(ui->usersTable,             &QTableView::clicked,       d,      &QnUserManagementWidgetPrivate::at_usersTable_clicked);

    connect(ui->ldapSettingsButton,     &QPushButton::clicked,      d,      &QnUserManagementWidgetPrivate::openLdapSettings);
    connect(ui->refreshButton,          &QPushButton::clicked,      this,   &QnUserManagementWidget::at_refreshButton_clicked);
}

QnUserManagementWidget::~QnUserManagementWidget() {
}
