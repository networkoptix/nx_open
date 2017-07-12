#pragma once

#include <ui/dialogs/common/message_box.h>
#include <ui/dialogs/common/session_aware_dialog.h>

class QnDisconnectFromCloudDialogPrivate;

class QnDisconnectFromCloudDialog : public QnSessionAwareMessageBox
{
    Q_OBJECT
    using base_type = QnSessionAwareMessageBox;

public:
    explicit QnDisconnectFromCloudDialog(QWidget *parent = 0);
    ~QnDisconnectFromCloudDialog();

    virtual void accept() override;

signals:
    void cloudPasswordValidated(
        bool success,
        const QString& password);

private:
    QnDisconnectFromCloudDialogPrivate* d_ptr;
    Q_DECLARE_PRIVATE(QnDisconnectFromCloudDialog)
};
