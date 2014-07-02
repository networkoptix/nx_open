#ifndef UPDATE_DIALOG_H
#define UPDATE_DIALOG_H

#include <QtWidgets/QDialog>

#include <ui/workbench/workbench_context_aware.h>

namespace Ui {
class QnSystemAdministrationDialog;
}

class QnServerUpdatesWidget;
class QnRoutingManagementWidget;

class QnSystemAdministrationDialog : public QDialog, public QnWorkbenchContextAware {
    Q_OBJECT
    typedef QDialog base_type;
public:
    enum Tabs {
        UpdatesTab = 0,
        RoutingTab
    };

    QnSystemAdministrationDialog(QnWorkbenchContext *context, QWidget *parent = 0);
    ~QnSystemAdministrationDialog();

    virtual void reject() override;
    virtual void accept() override;

    void reset();

    void activateTab(int tab);
    void checkForUpdates();

private:
    QScopedPointer<Ui::QnSystemAdministrationDialog> ui;

    QnServerUpdatesWidget *m_updatesWidget;
    QnRoutingManagementWidget *m_routingManagementWidget;
};

#endif // UPDATE_DIALOG_H
