#pragma once

#include <QtGlobal>
#include <ui/dialogs/common/message_box.h>
#include <ui/dialogs/common/session_aware_dialog.h>

class QnDisconnectFromCloudDialogPrivate;

enum class CredentialCheckResult
{
    Ok,
    NotAuthorized,
    UserLockedOut,
};

class QnDisconnectFromCloudDialog : public QnSessionAwareMessageBox
{
    Q_OBJECT
    using base_type = QnSessionAwareMessageBox;

public:
    explicit QnDisconnectFromCloudDialog(QWidget *parent = 0);
    ~QnDisconnectFromCloudDialog();

    virtual void accept() override;

signals:
    void cloudPasswordValidated(CredentialCheckResult result, const QString& password);

private:
    QnDisconnectFromCloudDialogPrivate* d_ptr;
    Q_DECLARE_PRIVATE(QnDisconnectFromCloudDialog)
};

