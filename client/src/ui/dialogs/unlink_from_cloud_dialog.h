#pragma once

#include <ui/dialogs/common/workbench_state_dependent_dialog.h>

class QnUnlinkFromCloudDialogPrivate;

class QnUnlinkFromCloudDialog : public QnWorkbenchStateDependentDialog<QnMessageBox>
{
    Q_OBJECT
    typedef QnWorkbenchStateDependentDialog<QnMessageBox> base_type;

public:
    explicit QnUnlinkFromCloudDialog(QWidget *parent = 0);
    ~QnUnlinkFromCloudDialog();

    virtual void accept() override;

private:
    QScopedPointer<QnUnlinkFromCloudDialogPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QnUnlinkFromCloudDialog)
};
