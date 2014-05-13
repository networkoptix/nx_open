#ifndef UPDATE_DIALOG_H
#define UPDATE_DIALOG_H

#include <QtWidgets/QDialog>

#include <ui/workbench/workbench_context_aware.h>

namespace Ui {
class QnSystemAdministrationDialog;
}

class QnServerUpdatesWidget;

class QnSystemAdministrationDialog : public QDialog, public QnWorkbenchContextAware {
    Q_OBJECT
    typedef QDialog base_type;
public:
    QnSystemAdministrationDialog(QnWorkbenchContext *context, QWidget *parent = 0);
    ~QnSystemAdministrationDialog();

    virtual void reject() override;
    virtual void accept() override;

private:
    QScopedPointer<Ui::QnSystemAdministrationDialog> ui;

    QnServerUpdatesWidget *m_updatesWidget;
};

#endif // UPDATE_DIALOG_H
