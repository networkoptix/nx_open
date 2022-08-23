// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <ui/dialogs/common/session_aware_dialog.h>

#include <nx/vms/api/data/ldap.h>

namespace Ui {
    class LdapSettingsDialog;
}

class QnLdapSettingsDialogPrivate;
class QnLdapSettingsDialog: public QnSessionAwareButtonBoxDialog {
    Q_OBJECT
    typedef QnSessionAwareButtonBoxDialog base_type;

public:
    explicit QnLdapSettingsDialog(QWidget *parent);
    ~QnLdapSettingsDialog();

    virtual void accept() override;
    virtual void reject() override;

private:
    void at_testLdapSettingsFinished(
        bool success,
        int handle,
        const nx::vms::api::LdapUserList& users,
        const QString& errorString);

private:
    QScopedPointer<Ui::LdapSettingsDialog> ui;
    QScopedPointer<QnLdapSettingsDialogPrivate> const d_ptr;
    Q_DECLARE_PRIVATE(QnLdapSettingsDialog)
};
