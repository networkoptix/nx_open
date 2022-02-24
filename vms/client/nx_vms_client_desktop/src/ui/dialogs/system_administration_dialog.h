// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef UPDATE_DIALOG_H
#define UPDATE_DIALOG_H

#include <ui/dialogs/common/session_aware_dialog.h>

class QnSessionAwareDelegate;

namespace Ui {
    class QnSystemAdministrationDialog;
}

class QnSystemAdministrationDialog : public QnSessionAwareTabbedDialog {
    Q_OBJECT
    typedef QnSessionAwareTabbedDialog base_type;
public:
    enum DialogPage {
        GeneralPage,
        UserManagement,
        UpdatesPage,
        LicensesPage,
        SmtpPage,
        SecurityPage,
        CloudManagement,
        TimeServerSelection,
        RoutingManagement,
        Analytics,

        PageCount
    };

    QnSystemAdministrationDialog(QWidget *parent = 0);
    ~QnSystemAdministrationDialog();

private:
    Q_DISABLE_COPY(QnSystemAdministrationDialog)

    QScopedPointer<Ui::QnSystemAdministrationDialog> ui;
};

Q_DECLARE_METATYPE(QnSystemAdministrationDialog::DialogPage)

#endif // UPDATE_DIALOG_H
