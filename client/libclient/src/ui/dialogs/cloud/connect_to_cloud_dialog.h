#pragma once

#include <ui/dialogs/common/button_box_dialog.h>
#include <ui/dialogs/common/session_aware_dialog.h>

namespace Ui {
class ConnectToCloudDialog;
}

class QnConnectToCloudDialogPrivate;

class QnConnectToCloudDialog : public QnSessionAwareButtonBoxDialog
{
    Q_OBJECT

    typedef QnSessionAwareButtonBoxDialog base_type;

public:
    explicit QnConnectToCloudDialog(QWidget* parent = nullptr);
    ~QnConnectToCloudDialog();

    void accept() override;

protected:
    virtual void showEvent(QShowEvent* event) override;

private:
    QScopedPointer<Ui::ConnectToCloudDialog> ui;
    QScopedPointer<QnConnectToCloudDialogPrivate> d_ptr;

    Q_DECLARE_PRIVATE(QnConnectToCloudDialog)
    Q_DISABLE_COPY(QnConnectToCloudDialog)
};
