#pragma once

#include <ui/dialogs/common/message_box.h>
#include <ui/dialogs/common/session_aware_dialog.h>

class QnUnlinkFromCloudDialogPrivate;

class QnUnlinkFromCloudDialog : public QnSessionAwareMessageBox
{
    Q_OBJECT
    using base_type = QnSessionAwareMessageBox;

public:
    explicit QnUnlinkFromCloudDialog(QWidget *parent = 0);
    ~QnUnlinkFromCloudDialog();

    virtual void accept() override;

private:
    QnUnlinkFromCloudDialogPrivate* d_ptr;
    Q_DECLARE_PRIVATE(QnUnlinkFromCloudDialog)
};
