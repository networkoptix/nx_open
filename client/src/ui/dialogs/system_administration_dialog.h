#ifndef UPDATE_DIALOG_H
#define UPDATE_DIALOG_H

#include <ui/dialogs/generic_tabbed_dialog.h>
#include <ui/workbench/workbench_context_aware.h>
#include <ui_system_administration_dialog.h>

class QnSystemAdministrationDialog : public QnGenericTabbedDialog, public QnWorkbenchContextAware {
    Q_OBJECT
    typedef QnGenericTabbedDialog base_type;
public:
    enum DialogPage {
        GeneralPage,
        LicensesPage,
        SmtpPage,
        UpdatesPage,

        PageCount
    };

    QnSystemAdministrationDialog(QWidget *parent = 0);
private:
    Q_DISABLE_COPY(QnSystemAdministrationDialog)

    QScopedPointer<Ui::QnSystemAdministrationDialog> ui;
};

#endif // UPDATE_DIALOG_H
