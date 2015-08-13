#pragma once

#include <ui/dialogs/workbench_state_dependent_dialog.h>

#include <utils/common/ldap_fwd.h>

namespace Ui {
    class LdapSettingsDialog;
}

class QnLdapSettingsDialogPrivate;
class QnLdapSettingsDialog: public QnWorkbenchStateDependentButtonBoxDialog {
    Q_OBJECT
    typedef QnWorkbenchStateDependentButtonBoxDialog base_type;

public:
    explicit QnLdapSettingsDialog(QWidget *parent = 0);
    ~QnLdapSettingsDialog();

    virtual void accept() override;
    virtual void reject() override;

private slots:
    void at_testLdapSettingsFinished(int status, int handle, const QString &errorString);

private:
    QScopedPointer<Ui::LdapSettingsDialog> ui;
    QnLdapSettingsDialogPrivate *d;
    friend class QnLdapSettingsDialogPrivate;
};
