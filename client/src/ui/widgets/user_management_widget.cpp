#include "user_management_widget.h"
#include "ui_user_management_widget.h"

#include <core/resource/user_resource.h>
#include <api/global_settings.h>
#include <utils/common/ldap.h>
#include <ui/models/user_list_model.h>
#include <ui/actions/action_manager.h>
#include <client/client_meta_types.h>

#include <ui/dialogs/ldap_settings_dialog.h>

class QnUserManagementWidgetPrivate : public QObject {
public:
    QnUserManagementWidget *widget;

    QnUserListModel *usersModel;

    QnUserManagementWidgetPrivate(QnUserManagementWidget *parent)
        : QObject(parent)
        , widget(parent)
        , usersModel(new QnUserListModel(parent))
    {
    }

    void at_usersTable_activated(const QModelIndex &index);
    void at_usersTable_clicked(const QModelIndex &index);
    void at_refreshButton_clicked();
    void openLdapSettings();
};

void QnUserManagementWidgetPrivate::at_usersTable_activated(const QModelIndex &index) {
    QnUserResourcePtr user = index.data(Qn::UserResourceRole).value<QnUserResourcePtr>();
    if (!user)
        return;

    widget->menu()->trigger(Qn::UserSettingsAction, QnActionParameters(user));
}

void QnUserManagementWidgetPrivate::at_usersTable_clicked(const QModelIndex &index) {
    if (index.column() == QnUserListModel::EditIconColumn)
        at_usersTable_activated(index);
}

void QnUserManagementWidgetPrivate::at_refreshButton_clicked() {
    if (!QnGlobalSettings::instance()->ldapSettings().isValid()) {
        openLdapSettings();
        return;
    }

    // TODO: Implement fetching
}

void QnUserManagementWidgetPrivate::openLdapSettings() {
    QScopedPointer<QnLdapSettingsDialog> dialog(new QnLdapSettingsDialog(widget));
    dialog->setWindowModality(Qt::ApplicationModal);
    dialog->exec();
}


QnUserManagementWidget::QnUserManagementWidget(QWidget *parent)
    : base_type(parent)
    , QnWorkbenchContextAware(parent)
    , ui(new Ui::QnUserManagementWidget)
    , d(new QnUserManagementWidgetPrivate(this))
{
    ui->setupUi(this);

    ui->usersTable->setModel(d->usersModel);
    ui->usersTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->usersTable->horizontalHeader()->setSectionResizeMode(QnUserListModel::PermissionsColumn, QHeaderView::Stretch);

    connect(ui->usersTable,             &QTableView::activated,     d,      &QnUserManagementWidgetPrivate::at_usersTable_activated);
    connect(ui->usersTable,             &QTableView::clicked,       d,      &QnUserManagementWidgetPrivate::at_usersTable_clicked);

    connect(ui->ldapSettingsButton,     &QPushButton::clicked,      d,      &QnUserManagementWidgetPrivate::openLdapSettings);
    connect(ui->refreshButton,          &QPushButton::clicked,      d,      &QnUserManagementWidgetPrivate::at_refreshButton_clicked);
}

QnUserManagementWidget::~QnUserManagementWidget() {
}
