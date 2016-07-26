#pragma once

#include <ui/dialogs/common/workbench_state_dependent_dialog.h>

namespace Ui {
class UnlinkFromCloudDialog;
}

class QnUnlinkFromCloudDialogPrivate;

class QnUnlinkFromCloudDialog : public QnWorkbenchStateDependentButtonBoxDialog
{
    Q_OBJECT
    using base_type = QnWorkbenchStateDependentButtonBoxDialog;

public:
    explicit QnUnlinkFromCloudDialog(QWidget *parent = 0);
    ~QnUnlinkFromCloudDialog();

    virtual void accept() override;

private:
    QScopedPointer<Ui::UnlinkFromCloudDialog> ui;
    QnUnlinkFromCloudDialogPrivate* d_ptr;
    Q_DECLARE_PRIVATE(QnUnlinkFromCloudDialog)
};
