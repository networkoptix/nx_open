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
        LicensesPage,
        SmtpPage,
        UpdatesPage,
        RoutingManagement,
        TimeServerSelection,
        UserManagement,
        CloudManagement,

        PageCount
    };

    QnSystemAdministrationDialog(QWidget *parent = 0);
    ~QnSystemAdministrationDialog();

private:
    Q_DISABLE_COPY(QnSystemAdministrationDialog)

    QScopedPointer<Ui::QnSystemAdministrationDialog> ui;
};

#endif // UPDATE_DIALOG_H
