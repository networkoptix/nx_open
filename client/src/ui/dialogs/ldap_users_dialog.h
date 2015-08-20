#pragma once

#include <ui/dialogs/workbench_state_dependent_dialog.h>

#include <utils/common/ldap.h>

namespace Ui {
    class LdapUsersDialog;
}

class QnLdapUsersDialog: public QnWorkbenchStateDependentButtonBoxDialog {
    Q_OBJECT
    typedef QnWorkbenchStateDependentButtonBoxDialog base_type;

public:
    explicit QnLdapUsersDialog(QWidget *parent = 0);
    ~QnLdapUsersDialog();

private:
    void stopTesting(const QString &text = QString());
    void importUsers(const QnLdapUsers &users);

    QnLdapUsers filterExistingUsers(const QnLdapUsers &users) const;
    QnLdapUsers visibleUsers() const;
    QnLdapUsers visibleSelectedUsers() const;


private slots:
    void at_testLdapSettingsFinished(int status, const QnLdapUsers &users, int handle, const QString &errorString);

private:
    QScopedPointer<Ui::LdapUsersDialog> ui;

    QTimer *m_timeoutTimer;
    QPushButton *m_importButton;
    bool m_loading;
};
