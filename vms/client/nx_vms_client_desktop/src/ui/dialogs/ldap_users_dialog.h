#pragma once

#include <ui/dialogs/common/session_aware_dialog.h>

#include <utils/common/ldap.h>

namespace Ui {
    class LdapUsersDialog;
}

class QnUserRolesModel;

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
    QnLdapUsers visibleSelectedUsers() const;

    void setupUsersTable(const QnLdapUsers& filteredUsers);

private slots:
    void at_testLdapSettingsFinished(int status, const QnLdapUsers &users, int handle, const QString &errorString);

private:
    QScopedPointer<Ui::LdapUsersDialog> ui;

    QTimer *m_timeoutTimer;
    QPushButton *m_importButton;
    QnUserRolesModel* const m_rolesModel;
    bool m_loading = true;
};
