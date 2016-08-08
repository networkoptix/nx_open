#pragma once

#include <ui/dialogs/common/message_box.h>
#include <ui/dialogs/common/workbench_state_dependent_dialog.h>

class QnUnlinkFromCloudDialogPrivate;

class QnUnlinkFromCloudDialog : public QnWorkbenchStateDependentDialog<QnMessageBox>
{
    Q_OBJECT
    using base_type = QnWorkbenchStateDependentDialog<QnMessageBox>;

public:
    explicit QnUnlinkFromCloudDialog(QWidget *parent = 0);
    ~QnUnlinkFromCloudDialog();

    virtual void accept() override;

private:
    QnUnlinkFromCloudDialogPrivate* d_ptr;
    Q_DECLARE_PRIVATE(QnUnlinkFromCloudDialog)
};
