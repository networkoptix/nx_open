#include "user_management_widget.h"
#include "ui_user_management_widget.h"

#include <api/global_settings.h>

#include <ui/models/user_list_model.h>

#include <ui/widgets/settings/private/user_management_widget_p.h>


#include <utils/common/ldap.h>


QnUserManagementWidget::QnUserManagementWidget(QWidget *parent)
    : base_type(parent)
    , QnWorkbenchContextAware(parent)
    , ui(new Ui::QnUserManagementWidget)
    , d_ptr(new QnUserManagementWidgetPrivate(this))
{
    Q_D(QnUserManagementWidget);

    ui->setupUi(this);

    ui->usersTable->setModel(d->sortModel());  
    ui->usersTable->setHorizontalHeader(d->header());

    d->header()->setVisible(true);
    d->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    d->header()->setSectionResizeMode(QnUserListModel::PermissionsColumn, QHeaderView::Stretch);
    d->header()->setSectionsClickable(true);

    ui->usersTable->sortByColumn(QnUserListModel::NameColumn, Qt::AscendingOrder);

    auto updateFetchButton = [this]{
        ui->fetchButton->setEnabled(QnGlobalSettings::instance()->ldapSettings().isValid());
    };


    connect(QnGlobalSettings::instance(), &QnGlobalSettings::ldapSettingsChanged, this, updateFetchButton);
    

    connect(ui->usersTable,             &QTableView::activated,                     d,  &QnUserManagementWidgetPrivate::at_usersTable_activated);
    connect(ui->usersTable,             &QTableView::clicked,                       d,  &QnUserManagementWidgetPrivate::at_usersTable_clicked);
    connect(ui->ldapSettingsButton,     &QPushButton::clicked,                      d,  &QnUserManagementWidgetPrivate::openLdapSettings);
    connect(ui->fetchButton,            &QPushButton::clicked,                      d,  &QnUserManagementWidgetPrivate::fetchUsers);

    updateFetchButton();
}

QnUserManagementWidget::~QnUserManagementWidget() {
}
