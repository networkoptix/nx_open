#include "user_management_widget_p.h"

#include <ui/widgets/settings/user_management_widget.h>

#include <QtCore/QSortFilterProxyModel>

#include <api/global_settings.h>

#include <common/common_module.h>

#include <core/resource/user_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>

#include <ui/models/user_list_model.h>
#include <ui/actions/action_manager.h>
#include <ui/dialogs/ldap_settings_dialog.h>
#include <ui/widgets/views/checkboxed_header_view.h>

#include <utils/common/ldap.h>


QnUserManagementWidgetPrivate::QnUserManagementWidgetPrivate(QnUserManagementWidget *parent) : QObject(parent)
    , q_ptr(parent)
    , m_usersModel(new QnUserListModel(parent))
    , m_sortModel(new QnSortedUserListModel(parent))
    , m_header(new QnCheckBoxedHeaderView(QnUserListModel::CheckBoxColumn, parent))
{
    m_sortModel->setSourceModel(m_usersModel);

    connect(m_header, &QnCheckBoxedHeaderView::checkStateChanged, this, &QnUserManagementWidgetPrivate::at_headerCheckStateChanged);
}

QnSortedUserListModel* QnUserManagementWidgetPrivate::sortModel() const {
    return m_sortModel;
}

QHeaderView* QnUserManagementWidgetPrivate::header() const {
    return m_header;
}

void QnUserManagementWidgetPrivate::updateHeaderState() {
    m_header->setCheckState(m_usersModel->checkState());
}

void QnUserManagementWidgetPrivate::openLdapSettings() {
    Q_Q(QnUserManagementWidget);

    QScopedPointer<QnLdapSettingsDialog> dialog(new QnLdapSettingsDialog(q));
    dialog->setWindowModality(Qt::ApplicationModal);
    dialog->exec();
}

void QnUserManagementWidgetPrivate::fetchUsers() {
    if (!QnGlobalSettings::instance()->ldapSettings().isValid()) 
        return;

    QnMediaServerResourcePtr server = qnResPool->getResourceById<QnMediaServerResource>(qnCommon->remoteGUID());
    Q_ASSERT(server);
    if (!server)
        return;

    server->apiConnection()->mergeLdapUsersAsync(this, SLOT(at_mergeLdapUsersAsync_finished(int,int,QString)));
}

void QnUserManagementWidgetPrivate::at_mergeLdapUsersAsync_finished(int status, int handle, const QString &errorString) {
    Q_UNUSED(handle);

    if (status == 0 && errorString.isEmpty())
        return;

    // TODO: dk, please show correct message here in case of error
}

void QnUserManagementWidgetPrivate::at_headerCheckStateChanged(Qt::CheckState state) {
    m_usersModel->setCheckState(state);
}

void QnUserManagementWidgetPrivate::at_usersTable_activated(const QModelIndex &index) {
    Q_Q(QnUserManagementWidget);

    QnUserResourcePtr user = index.data(Qn::UserResourceRole).value<QnUserResourcePtr>();
    if (!user)
        return;

    q->menu()->trigger(Qn::UserSettingsAction, QnActionParameters(user));
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
        updateHeaderState();
    }
}
