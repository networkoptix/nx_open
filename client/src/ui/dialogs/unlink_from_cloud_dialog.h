#pragma once

#include <ui/dialogs/message_box.h>

class QnUnlinkFromCloudDialogPrivate;

class QnUnlinkFromCloudDialog : public QnMessageBox
{
    Q_OBJECT
    typedef QnMessageBox base_type;

public:
    explicit QnUnlinkFromCloudDialog(QWidget *parent = 0);
    ~QnUnlinkFromCloudDialog();

    void done(int result) override;

private:
    QScopedPointer<QnUnlinkFromCloudDialogPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QnUnlinkFromCloudDialog)
};
