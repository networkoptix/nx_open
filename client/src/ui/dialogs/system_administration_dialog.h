#ifndef UPDATE_DIALOG_H
#define UPDATE_DIALOG_H

#include <QtWidgets/QDialog>

#include <ui/workbench/workbench_context_aware.h>

namespace Ui {
class QnSystemAdministrationDialog;
}

class QnSystemAdministrationDialog : public QDialog, public QnWorkbenchContextAware {
    Q_OBJECT
public:
    QnSystemAdministrationDialog(QnWorkbenchContext *context, QWidget *parent = 0);
    ~QnSystemAdministrationDialog();

private:
    QScopedPointer<Ui::QnSystemAdministrationDialog> ui;
};

#endif // UPDATE_DIALOG_H
