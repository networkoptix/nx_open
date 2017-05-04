#ifndef UPDATE_DIALOG_H
#define UPDATE_DIALOG_H

#include <QtWidgets/QDialog>

#include <utils/common/id.h>
#include <ui/dialogs/common/dialog.h>
#include <ui/workbench/workbench_context_aware.h>

namespace Ui {
class QnUpdateDialog;
}

class QnServerUpdatesWidget;
class QnMediaServerUpdateTool;

class QnUpdateDialog : public QnDialog, public QnWorkbenchContextAware {
    Q_OBJECT

    typedef QnDialog base_type;

public:
    explicit QnUpdateDialog(QnWorkbenchContext *context, QWidget *parent = 0);
    ~QnUpdateDialog();

    QnMediaServerUpdateTool *updateTool() const;

private:
    QScopedPointer<Ui::QnUpdateDialog> ui;
    QnServerUpdatesWidget *m_updatesWidget;
};

#endif // UPDATE_DIALOG_H
