// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <ui/dialogs/common/session_aware_dialog.h>

#include <utils/common/ldap.h>

namespace Ui {
    class LdapUsersDialog;
}

class QnLdapUserListModel;
class QnUserRolesModel;
class QSortFilterProxyModel;

class QnLdapUsersDialog: public QnSessionAwareButtonBoxDialog {
    Q_OBJECT
    typedef QnSessionAwareButtonBoxDialog base_type;

public:
    explicit QnLdapUsersDialog(QWidget *parent);
    ~QnLdapUsersDialog();

private:
    void stopTesting(const QString &text = QString());

    /** Update details for already existing users, e.g. email. */
    void updateExistingUsers(const QnLdapUsers &users);

    /** Import new ldap users. */
    void importUsers(const QnLdapUsers &users);

    /** Hide users that are already imported (or default users with same name exist). */
    QnLdapUsers filterExistingUsers(const QnLdapUsers &users) const;

    QnLdapUsers visibleUsers() const;
    QnLdapUsers selectedUsers(bool onlyVisible = false) const;

    void setupUsersTable(const QnLdapUsers& filteredUsers);

private:
    void at_testLdapSettingsFinished(
        bool success, int handle, const QnLdapUsers &users,
        const QString &errorString);

private:
    QScopedPointer<Ui::LdapUsersDialog> ui;

    QTimer *m_timeoutTimer;
    QPushButton *m_importButton;
    QnUserRolesModel* const m_rolesModel;
    std::unique_ptr<QnLdapUserListModel> m_usersModel;
    std::unique_ptr<QSortFilterProxyModel> m_sortModel;
    bool m_loading = true;
};
