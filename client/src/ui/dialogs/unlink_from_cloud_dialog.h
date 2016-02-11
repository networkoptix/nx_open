#pragma once

#include <ui/dialogs/message_box.h>
#include <ui/dialogs/workbench_state_dependent_dialog.h>

class QnUnlinkFromCloudDialogPrivate;

class QnUnlinkFromCloudDialog : public QnWorkbenchStateDependentDialog<QnMessageBox>
{
    Q_OBJECT
    typedef QnWorkbenchStateDependentDialog<QnMessageBox> base_type;

public:
    explicit QnUnlinkFromCloudDialog(QWidget *parent = 0);
    ~QnUnlinkFromCloudDialog();

    void done(int result) override;

private:
    QScopedPointer<QnUnlinkFromCloudDialogPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QnUnlinkFromCloudDialog)
};
