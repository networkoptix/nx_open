#ifndef UPDATE_DIALOG_H
#define UPDATE_DIALOG_H

#include <ui/dialogs/common/workbench_state_dependent_dialog.h>

class QnWorkbenchStateDelegate;

namespace Ui {
    class QnSystemAdministrationDialog;
}

class QnSystemAdministrationDialog : public QnWorkbenchStateDependentTabbedDialog {
    Q_OBJECT
    typedef QnWorkbenchStateDependentTabbedDialog base_type;
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
