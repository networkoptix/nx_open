#ifndef UPDATE_DIALOG_H
#define UPDATE_DIALOG_H

#include <ui/dialogs/generic_tabbed_dialog.h>
#include <ui/workbench/workbench_context_aware.h>

class QnWorkbenchStateDelegate;

namespace Ui {
    class QnSystemAdministrationDialog;
}

class QnServerUpdatesWidget;

class QnSystemAdministrationDialog : public QnGenericTabbedDialog, public QnWorkbenchContextAware {
    Q_OBJECT
    typedef QnGenericTabbedDialog base_type;
public:
    enum DialogPage {
        GeneralPage,
        LicensesPage,
        SmtpPage,
        UpdatesPage,
        RoutingManagement,

        PageCount
    };

    QnSystemAdministrationDialog(QWidget *parent = 0);
    ~QnSystemAdministrationDialog();

    virtual void accept() override;
    virtual void reject() override;

private:
    Q_DISABLE_COPY(QnSystemAdministrationDialog)

    QScopedPointer<Ui::QnSystemAdministrationDialog> ui;
    QnServerUpdatesWidget *m_updatesWidget;
    QScopedPointer<QnWorkbenchStateDelegate> m_workbenchStateDelegate;
};

#endif // UPDATE_DIALOG_H
